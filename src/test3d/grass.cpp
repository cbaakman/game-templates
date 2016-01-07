/* Copyright (C) 2015 Coos Baakman

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "grass.h"
#include "../util.h"
#include "../vec.h"
#include "../err.h"
#include "../xml.h"
#include "../random.h"

#include "../shader.h"

#define VERTEX_INDEX 0
#define NORMAL_INDEX 1
#define TEXCOORD_INDEX 2

const GLfloat grass_top_color [3] = {0.0f, 1.0f, 0.0f},
              grass_bottom_color [3] = {0.0f, 0.5f, 0.0f};

const char

    /*
        For some reason, 150 shaders don't work with client states,
        so we must use vertex attributes to refer to position, normal ans texture coords.
     */

    layer_vsh [] = R"shader(
        #version 150

        uniform mat4 projMatrix,
                     modelViewMatrix;

        in vec3 vertex,
                normal;
        in vec2 texCoord;

        out VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexOut;

        void main ()
        {
            gl_Position = projMatrix * modelViewMatrix * vec4 (vertex, 1.0);

            VertexOut.eyeVertex = (modelViewMatrix * vec4 (vertex, 1.0)).xyz;

            mat4 normalMatrix = transpose (inverse (modelViewMatrix));
            VertexOut.eyeNormal = (normalMatrix * vec4 (normalize (normal), 0.0)).xyz;

            VertexOut.texCoord = texCoord;
        }
    )shader",

    layer_gsh [] = R"shader(
        #version 150

        uniform int layer,
                    n_layers;
        uniform float extend_max;
        uniform vec3 wind;

        uniform mat4 modelViewMatrix;

        layout (triangles) in;
        layout (triangle_strip, max_vertices=3) out;

        uniform mat4 projMatrix;

        // [3] for triangles:
        in VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexIn [3];

        out VertexData {
            vec3 eyeNormal;
            vec2 texCoord;
            float extension;
        } VertexOut;

        void main ()
        {
            int i;
            float step = extend_max / n_layers;
            for (i = 0; i < gl_in.length(); i++)
            {
                VertexOut.extension = float (layer) / n_layers;

                vec3 gravity = (modelViewMatrix * vec4 (0.0, -0.2, 0.0, 0.0)).xyz,

                     newVertex = VertexIn [i].eyeVertex + (VertexIn [i].eyeNormal) * layer * step
                                                        + wind * VertexOut.extension
                                                        + gravity * VertexOut.extension * VertexOut.extension;

                gl_Position = projMatrix * vec4 (newVertex, 1.0);

                VertexOut.eyeNormal = VertexIn [i].eyeNormal;
                VertexOut.texCoord = VertexIn [i].texCoord;

                EmitVertex ();
            }
        }

    )shader",

    layer_fsh [] = R"shader(
        #version 150

        uniform sampler2D tex_dots;
        uniform int layer;

        uniform vec3 top_color,
                     bottom_color;

        in VertexData {
            vec3 eyeNormal;
            vec2 texCoord;
            float extension;
        } VertexIn;

        void main ()
        {
            // On the texture, black is opaque, white is transparent.
            vec4 dotSample = texture2D (tex_dots, VertexIn.texCoord.st);
            float alpha = 1.0 - dotSample.r - VertexIn.extension * VertexIn.extension;
            if (layer <= 0)
                alpha = 1.0;

            vec3 color = (1.0 - VertexIn.extension) * bottom_color + VertexIn.extension * top_color;

            gl_FragColor = vec4 (color, alpha);
        }

    )shader",

    poly_fsh [] = R"shader(

        uniform sampler2D tex_grass;

        uniform vec3 top_color,
                     bottom_color;

        void main ()
        {
            // On the texture, black is opaque, white is transparent.
            vec4 sample = texture2D (tex_grass, gl_TexCoord[0].st);

            float a = gl_TexCoord[0].t * gl_TexCoord[0].t;
            vec3 color = (1.0 - a) * bottom_color + a * top_color;

            gl_FragColor = vec4 (color, 1.0 - sample.r);
        }

    )shader";

GrassScene::GrassScene (App *pApp) : Scene (pApp),
    angleY(PI/4), angleX(PI/4), distCamera(3.0f),
    layerShader(0), polyShader(0),
    grass_neutral_positions(NULL),
    wind(0.0f, 0.0f, 0.0f), t(0.0f),
    mode(GRASSMODE_POLYGON)
{
    texDots.tex = texGrass.tex = 0;
    std::memset (&vbo_hill, 0, sizeof (VertexBuffer));
    std::memset (&vbo_grass, 0, sizeof (VertexBuffer));
}
GrassScene::~GrassScene ()
{
    delete [] grass_neutral_positions;

    glDeleteBuffer (&vbo_hill);
    glDeleteBuffer (&vbo_grass);
    glDeleteProgram (layerShader);
    glDeleteProgram (polyShader);
    glDeleteTextures (1, &texDots.tex);
    glDeleteTextures (1, &texGrass.tex);
}
struct HillVertex
{
    MeshVertex v;
    MeshTexel t;
};
struct GrassVertex
{
    MeshVertex v;
    MeshTexel t;
};
/*
    We can both enable vertex attribute arrays and client states to
    represent vertex positions, normals and texture coordinates.
    As long as they represent the same data format.
 */
void RenderHillVertices (const VertexBuffer *pBuffer)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableVertexAttribArray (VERTEX_INDEX);
    glVertexAttribPointer (VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof (HillVertex), 0);
    glVertexPointer (3, GL_FLOAT, sizeof (HillVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glEnableVertexAttribArray (NORMAL_INDEX);
    glVertexAttribPointer (NORMAL_INDEX, 3, GL_FLOAT, GL_TRUE, sizeof (HillVertex), (const GLvoid *)(sizeof (vec3)));
    glNormalPointer (GL_FLOAT, sizeof (HillVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableVertexAttribArray (TEXCOORD_INDEX);
    glVertexAttribPointer (TEXCOORD_INDEX, 2, GL_FLOAT, GL_FALSE, sizeof (HillVertex), (const GLvoid *)(2 * sizeof (vec3)));
    glTexCoordPointer (2, GL_FLOAT, sizeof (HillVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableVertexAttribArray (VERTEX_INDEX);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableVertexAttribArray (NORMAL_INDEX);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableVertexAttribArray (TEXCOORD_INDEX);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
void RenderGrassVertices (const VertexBuffer *pBuffer)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableVertexAttribArray (VERTEX_INDEX);
    glVertexAttribPointer (VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), 0);
    glVertexPointer (3, GL_FLOAT, sizeof (GrassVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glEnableVertexAttribArray (NORMAL_INDEX);
    glVertexAttribPointer (NORMAL_INDEX, 3, GL_FLOAT, GL_TRUE, sizeof (GrassVertex), (const GLvoid *)(sizeof (vec3)));
    glNormalPointer (GL_FLOAT, sizeof (GrassVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableVertexAttribArray (TEXCOORD_INDEX);
    glVertexAttribPointer (TEXCOORD_INDEX, 2, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), (const GLvoid *)(2 * sizeof (vec3)));
    glTexCoordPointer (2, GL_FLOAT, sizeof (GrassVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableVertexAttribArray (VERTEX_INDEX);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableVertexAttribArray (NORMAL_INDEX);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableVertexAttribArray (TEXCOORD_INDEX);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

#define GRASS_PER_M2 200
#define MAX_GRASS_ANGLE M_PI_4
#define GRASS_WIDTH 0.2f
#define GRASS_HEIGHT 0.6f
bool GenGrassPolygons (VertexBuffer *pBuffer, vec3 **p_grass_neutral_positions, const MeshData *pMeshData, const std::string &subset_id)
{
    size_t i = 0,
           m, u, j, k;

    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    // Don't know this exactly, depends on the surface area of the floor:
    const size_t n_vertices_estimate = GRASS_PER_M2 * 10 * (pMeshData->quads.size () * 2 + pMeshData->triangles.size ());

    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (GrassVertex) * n_vertices_estimate, NULL, GL_DYNAMIC_DRAW);
    *p_grass_neutral_positions = new vec3 [n_vertices_estimate];

    GrassVertex *pbuf = (GrassVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return false;
    }

    ThroughSubsetFaces (pMeshData, subset_id,
        [&](const int n_vertex, const MeshVertex **p_vertices, const MeshTexel *)
        {
            m = 0;
            if (n_vertex >= 3)
                m++;
            if (n_vertex >= 4)
                m++;

            for (u = 0; u < m; u++)
            {
                const vec3 p0 = p_vertices [triangles [u][0]]->p,
                           p1 = p_vertices [triangles [u][1]]->p,
                           p2 = p_vertices [triangles [u][2]]->p,
                           n = Cross (p1 - p0, p2 - p0).Unit ();

                const float l01 = (p1 - p0).Length (),
                            l02 = (p2 - p0).Length (),
                            l12 = (p2 - p1).Length (),
                            s = (l01 + l02 + l12) / 2,
                            area = sqrt (s * (s - l01) * (s - l02) * (s - l12));

                const size_t n_blades = area * GRASS_PER_M2;
                for (j = 0; j < n_blades; j++)
                {
                    if ((i + 6) >= n_vertices_estimate)
                    {
                        SetError ("vertex estimate exceeded");
                        return false;
                    }

                    // Give each blade a random orientation and position on the triangle.
                    float r1 = RandomFloat (0.0f, 1.0f),
                          r2 = RandomFloat (0.0f, 1.0f),
                          a = RandomFloat (0, 2 * M_PI),
                          b = RandomFloat (-MAX_GRASS_ANGLE, MAX_GRASS_ANGLE),
                          c = RandomFloat (-MAX_GRASS_ANGLE, MAX_GRASS_ANGLE);

                    if (r2 > (1.0f - r1))
                    {
                        /*
                            Outside the triangle, but inside the parallelogram.
                            Rotate 180 degrees to fix
                         */

                        r2 = 1.0f - r2;
                        r1 = 1.0f - r1;
                    }

                    matrix4 transf = matRotY (a) * matRotX (c) * matRotZ (b);

                    vec3 base = p0 + r1 * (p1 - p0) + r2 * (p2 - p0),
                         vertical = transf * n,
                         horizontal = transf * Cross (n, VEC_FORWARD),
                         normal = transf * VEC_BACK;

                    // move grass down into the ground to correct for angle:
                    base -= vertical * (GRASS_WIDTH / 2) * sin (b);

                    // Make a quad from two triangles (6 vertices):

                    pbuf [i].v.p = base - (GRASS_WIDTH / 2) * horizontal;
                    pbuf [i].v.n = normal;
                    pbuf [i].t = {0.0f, 0.0f};

                    pbuf [i + 1].v.p = pbuf [i].v.p + GRASS_HEIGHT * vertical;
                    pbuf [i + 1].v.n = normal;
                    pbuf [i + 1].t = {0.0f, 1.0f};

                    pbuf [i + 2].v.p = base + (GRASS_WIDTH / 2) * horizontal;
                    pbuf [i + 2].v.n = normal;
                    pbuf [i + 2].t = {1.0f, 0.0f};

                    pbuf [i + 3] = pbuf [i + 1];

                    pbuf [i + 4].v.p = pbuf [i + 2].v.p + GRASS_HEIGHT * vertical;
                    pbuf [i + 4].v.n = normal;
                    pbuf [i + 4].t = {1.0f, 1.0f};

                    pbuf [i + 5] = pbuf [i + 2];

                    // Register these so that wind can be calculated
                    for (k=0; k < 6; k++)
                        (*p_grass_neutral_positions) [i + k] = pbuf [i + k].v.p;

                    i += 6;
                }
            }
        }
    );

    pBuffer->n_vertices = i;

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
void UpdateGrassPolygons (VertexBuffer *pBuffer, const vec3 *grass_neutral_positions, const vec3 &wind)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    GrassVertex *pbuf = (GrassVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        fprintf (stderr, "WARNING, failed to obtain buffer pointer during update!\n");
    }


    // Every second, fourth and fifth vertex is a grass blade's top
    for (size_t i = 0; i < pBuffer->n_vertices; i += 6)
    {
        pbuf [i + 1].v.p = grass_neutral_positions [i + 1] + wind;
        pbuf [i + 3].v.p = grass_neutral_positions [i + 3] + wind;
        pbuf [i + 4].v.p = grass_neutral_positions [i + 4] + wind;
    }

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
bool SetHillVertices (VertexBuffer *pBuffer, const MeshData *pMeshData, const std::string &subset_id)
{
    size_t i = 0,
           m, u;

    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    pBuffer->n_vertices = 3 * (pMeshData->quads.size () * 2 + pMeshData->triangles.size ());

    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (HillVertex) * pBuffer->n_vertices, NULL, GL_STATIC_DRAW);

    HillVertex *pbuf = (HillVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return false;
    }

    ThroughSubsetFaces (pMeshData, subset_id,
        [&](const int n_vertex, const MeshVertex **p_vertices, const MeshTexel *texels)
        {
            m = 0;
            if (n_vertex >= 3)
                m++;
            if (n_vertex >= 4)
                m++;

            for (u = 0; u < m; u++)
            {
                for (int j : triangles [u])
                {
                    pbuf [i].v = *(p_vertices [j]);
                    pbuf [i].t = texels [j];
                    i ++;
                }
            }
        }
    );

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
bool CheckGLError (const char *doing)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        char s [100];
        GLErrorString (s, err);
        SetError ("%s while %s", s, doing);
        return false;
    }

    return true;
}
void GrassScene::AddAll (Loader *pLoader)
{
    const std::string zipPath = std::string (SDL_GetBasePath()) + "test3d.zip";

    pLoader->Add (
        [this, zipPath] ()
        {
            SDL_RWops *f = SDL_RWFromZipArchive (zipPath.c_str(), "hill.xml");
            if (!f)
                return false;

            xmlDocPtr  pDoc = ParseXML (f);
            f->close(f);

            if (!pDoc)
            {
                SetError ("error parsing hill.xml: %s", GetError ());
                return false;
            }

            // Convert xml to mesh:

            bool success = ParseMesh (pDoc, &meshDataHill);
            xmlFreeDoc (pDoc);

            if (!success)
            {
                SetError ("error parsing hill.xml: %s", GetError ());
                return false;
            }

            /*
                This mesh is not animated, so we only need to fill the vertex buffer once
                and render from it every frame.
             */

            glGenBuffer (&vbo_hill);
            if (!SetHillVertices (&vbo_hill, &meshDataHill, "0"))
            {
                SetError ("error filling vertex buffer for hill");
                return false;
            }

            glGenBuffer (&vbo_grass);
            if (!GenGrassPolygons (&vbo_grass, &grass_neutral_positions, &meshDataHill, "0"))
            {
                SetError ("error filling grass vertex buffer");
                return false;
            }

            return true;
        }
    );

    pLoader->Add (
        [this] ()
        {
            GLuint vsh, gsh, fsh;

            vsh = CreateShader (layer_vsh, GL_VERTEX_SHADER);
            gsh = CreateShader (layer_gsh, GL_GEOMETRY_SHADER);
            fsh = CreateShader (layer_fsh, GL_FRAGMENT_SHADER);
            if (!(vsh && gsh && fsh))
            {
                glDeleteShader (vsh);
                glDeleteShader (gsh);
                glDeleteShader (fsh);
                return false;
            }

            layerShader = CreateShaderProgram (vsh, gsh, fsh);
            if (!layerShader)
                return false;

            glBindAttribLocation (layerShader, VERTEX_INDEX, "vertex");
            if (!CheckGLError ("setting vertex attrib location"))
                return false;
            glBindAttribLocation (layerShader, NORMAL_INDEX, "normal");
            if (!CheckGLError ("setting normal attrib location"))
                return false;
            glBindAttribLocation (layerShader, TEXCOORD_INDEX, "texCoord");
            if (!CheckGLError ("setting texCoord attrib location"))
                return false;

            // Schedule deletion:
            glDeleteShader (vsh);
            glDeleteShader (gsh);
            glDeleteShader (fsh);

            return true;
        }
    );

    pLoader->Add (
        [this] ()
        {
            GLuint fsh;

            fsh = CreateShader (poly_fsh, GL_FRAGMENT_SHADER);
            if (!fsh)
            {
                glDeleteShader (fsh);
                return false;
            }

            polyShader = CreateShaderProgram (fsh);
            if (!polyShader)
                return false;

            // Schedule deletion:
            glDeleteShader (fsh);

            return true;

        }
    );

    pLoader->Add (
        [this, zipPath] ()
        {
            SDL_RWops *f = SDL_RWFromZipArchive (zipPath.c_str(), "dots.png");
            if (!f) // file or archive missing
                return false;

            bool success = LoadPNG (f, &texDots);
            f->close (f);

            if (!success)
            {
                SetError ("error parsing dots.png: %s", GetError ());
                return false;
            }

            return true;
        }
    );

    pLoader->Add (
        [this, zipPath] ()
        {
            SDL_RWops *f = SDL_RWFromZipArchive (zipPath.c_str(), "grass.png");
            if (!f) // file or archive missing
                return false;

            bool success = LoadPNG (f, &texGrass);
            f->close (f);

            if (!success)
            {
                SetError ("error parsing grass.png: %s", GetError ());
                return false;
            }

            // Don't repeat at the edges of the grass blade texture:
            glBindTexture (GL_TEXTURE_2D, texGrass.tex);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture (GL_TEXTURE_2D, 0);

            return true;
        }
    );

}
void GrassScene::Update (const float dt)
{
    t += dt;
    wind.x = 0.1f * sin (t);

    UpdateGrassPolygons (&vbo_grass, grass_neutral_positions, wind);
}
void GrassScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (event->type == SDL_KEYDOWN)
    {
        if (event->keysym.sym == SDLK_a)
        {
            switch (mode)
            {
            case GRASSMODE_LAYER:
                mode = GRASSMODE_POLYGON;
                break;
            case GRASSMODE_POLYGON:
                mode = GRASSMODE_LAYER;
                break;
            }
        }
    }
}
void GrassScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out

    distCamera -= 0.3f * event->y;

    if (distCamera < 2.0f)
        distCamera = 2.0f;
}
void GrassScene::OnMouseMove(const SDL_MouseMotionEvent *event)
{
    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change the camera angles

        angleY += 0.01f * event -> xrel;
        angleX += 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < 0.2f)
            angleX = 0.2f;
    }
}

const GLfloat ambient [] = {0.3f, 0.3f, 0.3f, 1.0f},
              diffuse [] = {1.0f, 1.0f, 1.0f, 1.0f};

const vec3 posLight (0.0f, 5.0f, 0.0f);

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f
#define N_GRASS_LAYERS 20
void GrassScene::Render (void)
{
    int w, h, i;
    GLint loc;

    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    // Set the 3d projection matrix:
    glMatrixMode (GL_PROJECTION);
    matrix4 matCamera = matPerspec (VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf (matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode (GL_MODELVIEW);
    vec3 posCamera = vec3 (0, 0, -distCamera);
    posCamera *= matRotY (angleY) * matRotX (angleX);
    const matrix4 matCameraView = matLookAt (posCamera, VEC_O, VEC_UP);
    glLoadMatrixf (matCameraView.m);

    glClearColor (0.0f, 0.7f, 1.0f, 1.0f);
    glClearDepth (1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable (GL_LIGHTING);
    glLightModelfv (GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuse);
    glEnable (GL_LIGHT0);
    GLfloat vLightPos [4] = {posLight.x, posLight.y, posLight.z, 1.0f};// 0 is directional
    glLightfv (GL_LIGHT0, GL_POSITION, vLightPos);

    glDisable (GL_STENCIL_TEST);
    glDisable (GL_CULL_FACE);

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDepthMask (GL_TRUE);

    switch (mode)
    {
    case GRASSMODE_LAYER:

        glDisable (GL_COLOR_MATERIAL);

        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable (GL_TEXTURE_2D);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, texDots.tex);

        glUseProgram (layerShader);

        loc = glGetUniformLocation (layerShader, "projMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matCamera.m);

        loc = glGetUniformLocation (layerShader, "modelViewMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matCameraView.m);

        loc = glGetUniformLocation (layerShader, "wind");
        glUniform3fv (loc, 1, wind.v);

        loc = glGetUniformLocation (layerShader, "top_color");
        glUniform3fv (loc, 1, grass_top_color);

        loc = glGetUniformLocation (layerShader, "bottom_color");
        glUniform3fv (loc, 1, grass_bottom_color);

        loc = glGetUniformLocation (layerShader, "extend_max");
        glUniform1f (loc, GRASS_HEIGHT);

        // Render The Grass layer by layer:
        loc = glGetUniformLocation (layerShader, "n_layers");
        glUniform1i (loc, N_GRASS_LAYERS);
        loc = glGetUniformLocation (layerShader, "layer");
        for (i = 0; i < N_GRASS_LAYERS; i++)
        {
            glUniform1i (loc, i);
            RenderHillVertices (&vbo_hill);
        }

        glUseProgram (0);
    break;
    case GRASSMODE_POLYGON:

        glUseProgram (0);

        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable (GL_TEXTURE_2D);

        glEnable (GL_COLOR_MATERIAL);
        glColor3fv (grass_bottom_color);
        RenderHillVertices (&vbo_hill);

        glEnable (GL_TEXTURE_2D);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, texGrass.tex);

        glUseProgram (polyShader);

        loc = glGetUniformLocation (polyShader, "top_color");
        glUniform3fv (loc, 1, grass_top_color);

        loc = glGetUniformLocation (polyShader, "bottom_color");
        glUniform3fv (loc, 1, grass_bottom_color);

        // transparent fragments may render to the color buffer, but not to the depth buffer
        glAlphaFunc (GL_GREATER, 0.99f) ;
        glEnable (GL_ALPHA_TEST);
        RenderGrassVertices (&vbo_grass);
        glDisable (GL_ALPHA_TEST);

        glUseProgram (0);
    break;
    }
}
