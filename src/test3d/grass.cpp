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

#include "../shader.h"

#define VERTEX_INDEX 0
#define NORMAL_INDEX 1
#define TEXCOORD_INDEX 2

const char

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
        uniform float extend_max = 0.5;
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

        in VertexData {
            vec3 eyeNormal;
            vec2 texCoord;
            float extension;
        } VertexIn;

        void main ()
        {
            vec4 dotSample = texture2D (tex_dots, VertexIn.texCoord.st);
            float alpha = 1.0 - dotSample.r - VertexIn.extension * VertexIn.extension;
            if (layer <= 0)
                alpha = 1.0;

            gl_FragColor = vec4 (0.0, 0.5 + 0.5 * VertexIn.extension, 0.0, alpha);
        }

    )shader";

GrassScene::GrassScene (App *pApp) : Scene (pApp),
    angleY(PI/4), angleX(PI/4), distCamera(3.0f),
    grassShader(0),
    wind(0.0f, 0.0f, 0.0f), t(0.0f)
{
    texDots.tex = 0;
    std::memset (&vbo_hill, 0, sizeof (VertexBuffer));
}
GrassScene::~GrassScene ()
{
    glDeleteBuffer (&vbo_hill);
    glDeleteProgram (grassShader);
    glDeleteTextures (1, &texDots.tex);
}
struct HillVertex
{
    MeshVertex v;
    MeshTexel t;
};
void RenderHillVertices (const VertexBuffer *pBuffer)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    //glEnableClientState (GL_VERTEX_ARRAY);
    glEnableVertexAttribArray (VERTEX_INDEX);
    glVertexAttribPointer (VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof (HillVertex), 0);
    //glVertexPointer (3, GL_FLOAT, sizeof (HillVertex), 0);

    //glEnableClientState (GL_NORMAL_ARRAY);
    glEnableVertexAttribArray (NORMAL_INDEX);
    glVertexAttribPointer (NORMAL_INDEX, 3, GL_FLOAT, GL_TRUE, sizeof (HillVertex), (const GLvoid *)(sizeof (vec3)));
    //glNormalPointer (GL_FLOAT, sizeof (HillVertex), (const GLvoid *)(sizeof (vec3)));

    //glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableVertexAttribArray (TEXCOORD_INDEX);
    glVertexAttribPointer (TEXCOORD_INDEX, 2, GL_FLOAT, GL_FALSE, sizeof (HillVertex), (const GLvoid *)(2 * sizeof (vec3)));
    //glTexCoordPointer (2, GL_FLOAT, sizeof (HillVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    //glDisableClientState (GL_VERTEX_ARRAY);
    glDisableVertexAttribArray (VERTEX_INDEX);
    //glDisableClientState (GL_NORMAL_ARRAY);
    glDisableVertexAttribArray (NORMAL_INDEX);
    //glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableVertexAttribArray (TEXCOORD_INDEX);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
bool SetHillVertices (VertexBuffer *pBuffer, const MeshData *pMeshData, const std::string &subset_id)
{
    size_t i = 0,
           m;

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

            for (int u = 0; u < m; u++)
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

            grassShader = CreateShaderProgram (vsh, gsh, fsh);
            if (!grassShader)
                return false;

            glBindAttribLocation (grassShader, VERTEX_INDEX, "vertex");
            if (!CheckGLError ("setting vertex attrib location"))
                return false;
            glBindAttribLocation (grassShader, NORMAL_INDEX, "normal");
            if (!CheckGLError ("setting normal attrib location"))
                return false;
            glBindAttribLocation (grassShader, TEXCOORD_INDEX, "texCoord");
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
        [this, zipPath] ()
        {
            SDL_RWops *f = SDL_RWFromZipArchive (zipPath.c_str(), "dots.png");
            if (!f) // file or archive missing
                return false;

            bool success = LoadPNG(f, &texDots);
            f->close(f);

            if (!success)
            {
                SetError ("error parsing dots.png: %s", GetError ());
                return false;
            }

            return true;
        }
    );
}
void GrassScene::Update (const float dt)
{
    t += dt;
    wind.x = 0.1f * sin (t);
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

    glEnable (GL_LIGHTING);
    glLightModelfv (GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuse);
    glEnable (GL_LIGHT0);
    GLfloat vLightPos [4] = {posLight.x, posLight.y, posLight.z, 1.0f};// 0 is directional
    glLightfv (GL_LIGHT0, GL_POSITION, vLightPos);

    glDisable (GL_STENCIL_TEST);
    glDisable (GL_CULL_FACE);
    glDepthFunc (GL_LEQUAL);
    glDepthMask (GL_TRUE);
    glDisable (GL_COLOR_MATERIAL);

    glEnable (GL_DEPTH_TEST);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable (GL_TEXTURE_2D);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texDots.tex);

    glUseProgram (grassShader);

    loc = glGetUniformLocation (grassShader, "projMatrix");
    glUniformMatrix4fv (loc, 1, GL_FALSE, matCamera.m);

    loc = glGetUniformLocation (grassShader, "modelViewMatrix");
    glUniformMatrix4fv (loc, 1, GL_FALSE, matCameraView.m);

    loc = glGetUniformLocation (grassShader, "wind");
    glUniform3f (loc, wind.x, wind.y, wind.z);

    // Render The Grass polygons, layer by layer:
    loc = glGetUniformLocation (grassShader, "n_layers");
    glUniform1i (loc, N_GRASS_LAYERS);
    loc = glGetUniformLocation (grassShader, "layer");
    for (i = 0; i < N_GRASS_LAYERS; i++)
    {
        glUniform1i (loc, i);
        RenderHillVertices (&vbo_hill);
    }

    glUseProgram (0);
}
