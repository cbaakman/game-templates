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

    shaderProgram (0)
{
}
ToonScene::~ToonScene ()
{
    glDeleteTextures (1, &texBG.tex);
    glDeleteProgram (shaderProgram);
}
bool ToonScene::Init ()
{
    std::string resPath = std::string(SDL_GetBasePath()) + "test3d.zip",
                sourceV = "", sourceF = "";;

    SDL_RWops *f;
    bool success;
    xmlDocPtr pDoc;

    // Load head mesh:
    f = SDL_RWFromZipArchive (resPath.c_str(), "head.xml");
    if (!f)
        return false;
    pDoc = ParseXML (f);
    f->close(f);

    if (!pDoc)
    {
        SetError ("error parsing head.xml: %s", GetError ());
        return false;
    }

    // Convert xml to mesh:
    success = ParseMesh (pDoc, &meshDataHead);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing head.xml: %s", GetError ());
        return false;
    }

    // Load background image:
    f = SDL_RWFromZipArchive (resPath.c_str(), "toonbg.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texBG);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing toonbg.png: %s", GetError ());
        return false;
    }

    // Load the vertex shader source:
    f = SDL_RWFromZipArchive (resPath.c_str(), "shaders/toon.vsh");
    if (!f)
        return false;

    success = ReadAll (f, sourceV);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing toon.vsh: %s", GetError ());
        return false;
    }

    // Load the fragment shader source:
    f = SDL_RWFromZipArchive (resPath.c_str(), "shaders/toon.fsh");
    if (!f)
        return false;
    success = ReadAll (f, sourceF);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing toon.fsh: %s", GetError ());
        return false;
    }

    // Create shader from sources:
    shaderProgram = CreateShaderProgram (sourceV, sourceF);
    if (!shaderProgram)
    {
        SetError ("error creating shader program from toon.vsh and toon.fsh: %s", GetError ());
        return false;
    }

    return true;
}
void RenderUnAnimatedSubsetExtension (const MeshData *pMeshData, std::string id, const GLfloat howmuch)
{
    /*
        Almost the same as a normal mesh rendering,
        except that the vertices are moved along their normals.
     */

    if(pMeshData->subsets.find(id) == pMeshData->subsets.end())
    {
        SetError ("cannot render %s, no such subset", id.c_str());
        return;
    }
    const MeshSubset *pSubset = &pMeshData->subsets.at(id);

    const MeshTexel *t;
    const vec3 *n;
    vec3 p;

    for (std::list <PMeshQuad>::const_iterator it = pSubset->quads.begin(); it != pSubset->quads.end(); it++)
    {
        glBegin(GL_QUADS);

        PMeshQuad pQuad = *it;
        for (size_t j = 0; j < 4; j++)
        {
            t = &pQuad->texels[j];
            n = &pMeshData->vertices.at (pQuad->GetVertexID(j)).n;
            p = pMeshData->vertices.at (pQuad->GetVertexID(j)).p + howmuch * (*n);

            glTexCoord2f (t->u, t->v);
            glNormal3f (n->x, n->y, n->z);
            glVertex3f (p.x, p.y, p.z);
        }

        glEnd();
    }

    glBegin(GL_TRIANGLES);
    for (std::list <PMeshTriangle>::const_iterator it = pSubset->triangles.begin(); it != pSubset->triangles.end(); it++)
    {
        PMeshTriangle pTri = *it;
        for (size_t j = 0; j < 3; j++)
        {
            t = &pTri->texels[j];
            n = &pMeshData->vertices.at (pTri->GetVertexID(j)).n;
            p = pMeshData->vertices.at (pTri->GetVertexID(j)).p + howmuch * (*n);

            glTexCoord2f (t->u, t->v);
            glNormal3f (n->x, n->y, n->z);
            glVertex3f (p.x, p.y, p.z);
        }
    }
    glEnd();
}

GLfloat lightAmbient [] = {0.5f, 0.5f, 0.5f, 1.0f},
        lightDiffuse [] = {0.5f, 0.5f, 0.5f, 1.0f},
        posLight[] = {-10.0f, 5.0f, -10.0f, 0.0f};

#define STENCIL_MASK 0xFFFFFFFF

void ToonScene::Render ()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);


    // Set the 3d projection matrix:
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

    // Set the 3d projection matrix:
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

    const GLfloat lineThickness = 0.1f;

    glCullFace (GL_FRONT);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);
    glStencilFunc (GL_ALWAYS, 1, STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);

    glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
    RenderUnAnimatedSubsetExtension (&meshDataHead, "0", lineThickness); // head black lines

    RenderUnAnimatedSubsetExtension (&meshDataHead, "2", lineThickness); // hair black lines

    glUseProgram(shaderProgram);

    // Draw only inside the black lines body
    glStencilFunc (GL_EQUAL, 1, STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask (GL_TRUE);

    glEnable (GL_CULL_FACE);
    glFrontFace (GL_CCW);

    glColor4fv (meshDataHead.subsets.at("0").diffuse);
    RenderUnAnimatedSubset (&meshDataHead, "0"); // head

    /* The face is inside the head, but drawn over it */
    glDisable (GL_DEPTH_TEST);

    glColor4fv (meshDataHead.subsets.at("1").diffuse);
    RenderUnAnimatedSubset (&meshDataHead, "1"); // eyes

    glColor4fv (meshDataHead.subsets.at("4").diffuse);
    RenderUnAnimatedSubset (&meshDataHead, "4"); // pupils

    glColor4fv (meshDataHead.subsets.at("3").diffuse);
    RenderUnAnimatedSubset (&meshDataHead, "3"); // mouth

    glEnable (GL_DEPTH_TEST);

    glColor4fv (meshDataHead.subsets.at("2").diffuse);
    RenderUnAnimatedSubset (&meshDataHead, "2"); // hair

    glUseProgram (NULL);
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
