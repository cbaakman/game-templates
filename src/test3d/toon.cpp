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

#include "toon.h"
#include "../xml.h"
#include "../io.h"
#include "../err.h"
#include "../shader.h"
#include "../util.h"

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f

ToonScene::ToonScene (App *pApp) : App::Scene (pApp),

    angleY(0), angleX(0), distCamera(10.0f),

    shaderProgram(0)
{
    texBG.tex = 0;
    std::memset (&vbo_lines, 0, sizeof (VertexBuffer));
    std::memset (&vbo_hair, 0, sizeof (VertexBuffer));
    std::memset (&vbo_head, 0, sizeof (VertexBuffer));
    std::memset (&vbo_eyes, 0, sizeof (VertexBuffer));
    std::memset (&vbo_pupils, 0, sizeof (VertexBuffer));
    std::memset (&vbo_mouth, 0, sizeof (VertexBuffer));
}
ToonScene::~ToonScene ()
{
    glDeleteBuffer (&vbo_lines);
    glDeleteBuffer (&vbo_hair);
    glDeleteBuffer (&vbo_head);
    glDeleteBuffer (&vbo_eyes);
    glDeleteBuffer (&vbo_pupils);
    glDeleteBuffer (&vbo_mouth);

    glDeleteTextures (1, &texBG.tex);
    glDeleteProgram (shaderProgram);
}

const char

// Toon vector shader:
toon_vsh [] = R"shader(

    varying vec3 N;
    varying vec3 v;
    varying vec4 color;

    void main()
    {
        v = vec3 (gl_ModelViewMatrix * gl_Vertex);

        N = normalize (gl_NormalMatrix * gl_Normal);

        color = gl_Color;

        gl_Position = ftransform();
    }

)shader",

// Toon fragment shader:
toon_fsh [] = R"shader(

    uniform float cutOff = 0.8;

    varying vec3 N;
    varying vec3 v;
    varying vec4 color;

    void main()
    {
        vec3 L = normalize(gl_LightSource[0].position.xyz - v);

        float lum = clamp (dot(normalize (N), L) + 0.5, 0.0, 1.0);

        if (lum < cutOff)
            lum = cutOff;
        else
            lum = 1.0;

        gl_FragColor = color * lum;
    }

)shader";

bool SetToonVertices (VertexBuffer *pBuffer, const MeshData *pMeshData, const std::list<std::string> &subset_ids,
                  const GLfloat extension=0.0f)
{
    /*
        Almost the same as a default mesh rendering,
        except that the vertices are moved along their normals.
        This makes the rendered mesh a bit larger than the original.
     */

    size_t i = 0, m;
    vec3 p;

    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    pBuffer->n_vertices = 0;
    for (auto id : subset_ids)
    {
        if(pMeshData->subsets.find(id) == pMeshData->subsets.end ())
        {
            SetError ("cannot render %s, no such subset", id.c_str ());
            return false;
        }
        const MeshSubset *pSubset = &pMeshData->subsets.at (id);

        pBuffer->n_vertices += 3 * (pSubset->quads.size () * 2 + pSubset->triangles.size ());
    }

    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (MeshVertex) * pBuffer->n_vertices, NULL, GL_STATIC_DRAW);

    MeshVertex *pbuf = (MeshVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("unable to acquire buffer pointer");
        return false;
    }

    for (auto id : subset_ids)

        ThroughSubsetFaces (pMeshData, id,
            [&] (const int n_vertex, const MeshVertex **p_vertices, const MeshTexel *texels)
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
                        pbuf [i].p = p_vertices [j]->p + extension * p_vertices [j]->n;
                        pbuf [i].n = p_vertices [j]->n;
                        i ++;
                    }
                }
            }
        );


    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
bool SetToonVertices (VertexBuffer *pBuffer, const MeshData *pMeshData, const std::string &subset_id)
{
    std::list<std::string> subset_ids;
    subset_ids.push_back (subset_id);

    return SetToonVertices (pBuffer, pMeshData, subset_ids);
}
void RenderToonVertices (const VertexBuffer *pBuffer)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    // Match the vertex format: position [3], normal [3]
    glEnableClientState (GL_VERTEX_ARRAY);
    glVertexPointer (3, GL_FLOAT, sizeof (MeshVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glNormalPointer (GL_FLOAT, sizeof (MeshVertex), (const GLvoid *)(3 * sizeof (float)));

    glDrawArrays (GL_TRIANGLES, 0, pBuffer->n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableClientState (GL_NORMAL_ARRAY);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
void ToonScene::AddAll (Loader *pLoader)
{
    const std::string resPath = std::string (SDL_GetBasePath()) + "test3d.zip";

    pLoader->Add (
        [&, resPath] ()
        {
            if (!LoadMesh (resPath, "head.xml", &meshDataHead))
                return false;

            /*
                This mesh is not animated, so we only need to fill the vertex buffer once
                and render from it every frame.
             */

            const GLfloat lineThickness = 0.1f;
            glGenBuffer (&vbo_lines);
            std::list <std::string> subsets;

            // head black lines:
            subsets.push_back ("0");

            // hair black lines:
            subsets.push_back ("2");

            if (!SetToonVertices (&vbo_lines, &meshDataHead, subsets, lineThickness))
            {
                SetError ("error setting line vertices to buffer: %s", GetError ());
                return false;
            }

            glGenBuffer (&vbo_head);
            if (!SetToonVertices (&vbo_head, &meshDataHead, "0"))
            {
                SetError ("error setting head vertices to buffer: %s", GetError ());
                return false;
            }

            glGenBuffer (&vbo_eyes);
            if (!SetToonVertices (&vbo_eyes, &meshDataHead, "1"))
            {
                SetError ("error setting eyes vertices to buffer: %s", GetError ());
                return false;
            }

            glGenBuffer (&vbo_hair);
            if (!SetToonVertices (&vbo_hair, &meshDataHead, "2"))
            {
                SetError ("error setting hair vertices to buffer: %s", GetError ());
                return false;
            }

            glGenBuffer (&vbo_mouth);
            if (!SetToonVertices (&vbo_mouth, &meshDataHead, "3"))
            {
                SetError ("error setting mouth vertices to buffer: %s", GetError ());
                return false;
            }

            glGenBuffer (&vbo_pupils);
            if (!SetToonVertices (&vbo_pupils, &meshDataHead, "4"))
            {
                SetError ("error setting pupils vertices to buffer: %s", GetError ());
                return false;
            }

            return true;
        }
    );

    pLoader->Add (LoadPNGFunc (resPath, "toonbg.png", &texBG));

    pLoader->Add (
        [&] ()
        {
            // Create shader from sources:
            shaderProgram = CreateShaderProgram (GL_VERTEX_SHADER, toon_vsh,
                                                 GL_FRAGMENT_SHADER, toon_fsh);

            if (!shaderProgram)
                return false;

            return true;
        }
    );
}

GLfloat lightAmbient [] = {0.5f, 0.5f, 0.5f, 1.0f},
        lightDiffuse [] = {0.5f, 0.5f, 0.5f, 1.0f},
        posLight[] = {-10.0f, 5.0f, -10.0f, 0.0f};

#define STENCIL_MASK 0xFFFFFFFF

void ToonScene::Render ()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);


    // Set orthographic projection matrix:
    glMatrixMode (GL_PROJECTION);
    matrix4 matScreen = matOrtho(-w / 2, w / 2, -h / 2, h / 2, -1.0f, 1.0f);
    glLoadMatrixf (matScreen.m);

    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity ();

    glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glDisable (GL_BLEND);
    glDisable (GL_LIGHTING);
    glDisable (GL_CULL_FACE);
    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);


    // Render colored background
    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
    glEnable (GL_TEXTURE_2D);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texBG.tex);
    glBegin (GL_QUADS);

    glTexCoord2f (0.0f, 1.0f);
    glVertex3f (-w/2, w/2, 0.0f);

    glTexCoord2f (1.0f, 1.0f);
    glVertex3f (w/2, w/2, 0.0f);

    glTexCoord2f (1.0f, 0.0f);
    glVertex3f (w/2, -w/2, 0.0f);

    glTexCoord2f (0.0f, 0.0f);
    glVertex3f (-w/2, -w/2, 0.0f);
    glEnd ();

    glClearDepth (1.0f);
    glClearColor (0, 1, 1, 1);
    glClearStencil (0);
    glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Set perspective 3d projection matrix:
    glMatrixMode (GL_PROJECTION);
    matrix4 matCamera = matPerspec(VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf(matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode (GL_MODELVIEW);
    vec3 posCamera = vec3 (0, 0, -distCamera);
    const matrix4 matCameraView = matLookAt(posCamera, VEC_O, VEC_UP);
    glLoadMatrixf(matCameraView.m);

    glEnable (GL_STENCIL_TEST);
    glDisable (GL_TEXTURE_2D);

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glLightfv(GL_LIGHT0, GL_POSITION, posLight);

    matrix4 rotateHead = matRotY (angleY) * matRotX (angleX);
    glMultMatrixf (rotateHead.m);

    glCullFace (GL_FRONT);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE); // no depth values, we need to draw inside it.
    glStencilFunc (GL_ALWAYS, 1, STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);

    glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
    RenderToonVertices (&vbo_lines);

    glUseProgram(shaderProgram);

    // Draw only inside the black lines body
    glStencilFunc (GL_EQUAL, 1, STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask (GL_TRUE);

    glEnable (GL_CULL_FACE);
    glFrontFace (GL_CCW);

    glColor4fv (meshDataHead.subsets.at("0").diffuse);
    RenderToonVertices (&vbo_head);

    // The face is inside the head, but drawn over it
    glDisable (GL_DEPTH_TEST);

    glColor4fv (meshDataHead.subsets.at("1").diffuse);
    RenderToonVertices (&vbo_eyes);

    glColor4fv (meshDataHead.subsets.at("4").diffuse);
    RenderToonVertices (&vbo_pupils);

    glColor4fv (meshDataHead.subsets.at("3").diffuse);
    RenderToonVertices (&vbo_mouth);

    glEnable (GL_DEPTH_TEST);

    // Hair must be drawn over the face:
    glColor4fv (meshDataHead.subsets.at("2").diffuse);
    RenderToonVertices (&vbo_hair);

    glUseProgram (0);
}
void ToonScene::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change camera angles

        angleY -= 0.01f * event -> xrel;
        angleX -= 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < -1.5f)
            angleX = -1.5f;
    }
}
void ToonScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
