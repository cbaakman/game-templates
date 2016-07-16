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

#include "vecs.h"
#include "../xml.h"
#include "../io.h"
#include "../err.h"
#include "../shader.h"
#include "../util.h"
#include "../GLutil.h"

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f

VecScene::VecScene (App *pApp, const Font *pFnt) : App::Scene (pApp),

    pFont (pFnt),
    angleY(0), angleX(0), distCamera(3.0f),
    mode(DISPLAY_AXIS),
    t(0.0f)
{
}
void RenderTextAt (const Font *pFont, const vec3 &pos, const char *text)
{
    matrix4 modelView, textTransform;
    vec3 modelViewRight, modelViewUp, modelViewForward;

    glGetFloatv (GL_MODELVIEW_MATRIX, modelView.m);

    modelViewRight = vec3 (modelView.m11, modelView.m12, modelView.m13).Unit(),
    modelViewUp = vec3 (modelView.m21, modelView.m22, modelView.m23).Unit(),
    modelViewForward = vec3 (modelView.m31, modelView.m32, modelView.m33).Unit();

    textTransform = matID ();
    textTransform.m11 = modelViewRight.x;
    textTransform.m12 = modelViewRight.y;
    textTransform.m13 = modelViewRight.z;

    textTransform.m21 = -modelViewUp.x;
    textTransform.m22 = -modelViewUp.y;
    textTransform.m23 = -modelViewUp.z;

    textTransform.m31 = modelViewForward.x;
    textTransform.m32 = modelViewForward.y;
    textTransform.m33 = modelViewForward.z;

    const float fsize = 0.1f;

    textTransform = matTranslation (pos.x, pos.y, pos.z)
                  * matScale (fsize / pFont->size)
                  * matInverse (textTransform)
                  * matTranslation (0.0f, -0.8f * pFont->size, 0.0f);

    glMultMatrixf (textTransform.m);

    glRenderText (pFont, text, TEXTALIGN_MID);

    glLoadMatrixf (modelView.m);
}
void VecScene::Update (const float dt)
{
    t += dt;
}
void VecScene::Render ()
{
    int w, h;
    vec3 posCamera,
         v1, v2, v12, v21;

    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    glClearDepth (1.0f); glDepthMask (GL_TRUE);
    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set perspective 3d projection matrix:
    glMatrixMode (GL_PROJECTION);
    matrix4 matCamera = matPerspec (VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf (matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode (GL_MODELVIEW);
    posCamera = matRotY (angleY) * matRotX (angleX) * vec3 (0, 0, -distCamera);
    const matrix4 matCameraView = matLookAt (posCamera, VEC_O, VEC_UP);
    glLoadMatrixf (matCameraView.m);

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);

    glDisable (GL_STENCIL_TEST);
    glDisable (GL_TEXTURE_2D);
    glDisable (GL_LIGHTING);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);
    glUseProgram (NULL);

    if (mode == DISPLAY_AXIS)
    {
        glBegin (GL_LINES);
        glColor4f (1.0f, 0.0f, 0.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (1.0f, 0.0f, 0.0f);

        glColor4f (0.0f, 1.0f, 0.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (0.0f, 1.0f, 0.0f);

        glColor4f (0.0f, 0.0f, 1.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (0.0f, 0.0f, 1.0f);
        glEnd ();

        glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
        RenderCube (VEC_O, 0.05f);

        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);
        RenderTextAt (pFont, vec3 (1.0f, 0.0f, 0.0f), "x");
        RenderTextAt (pFont, vec3 (0.0f, 1.0f, 0.0f), "y");
        RenderTextAt (pFont, vec3 (0.0f, 0.0f, 1.0f), "z");
    }
    else if (mode == DISPLAY_CROSS)
    {
        v1 = vec3 (1.0f, 0.0f, 0.0f);
        v2 = matRotY (t) * v1;
        v12 = Cross (v1, v2);
        v21 = Cross (v2, v1);

        glBegin (GL_LINES);
        glColor4f (1.0f, 0.0f, 0.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (v1.x, v1.y, v1.z);

        glColor4f (1.0f, 0.5f, 0.5f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (v2.x, v2.y, v2.z);

        glColor4f (0.0f, 0.0f, 1.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (v12.x, v12.y, v12.z);

        glColor4f (0.5f, 0.5f, 1.0f, 1.0f);
        glVertex3f (0.0f, 0.0f, 0.0f);
        glVertex3f (v21.x, v21.y, v21.z);
        glEnd ();

        glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
        RenderCube (VEC_O, 0.05f);

        glEnable (GL_BLEND);
        glDepthMask (GL_FALSE);
        RenderTextAt (pFont, v1, "v1");
        RenderTextAt (pFont, v2, "v2");
        RenderTextAt (pFont, v12, "v1 x v2");
        RenderTextAt (pFont, v21, "v2 x v1");
    }
}
void VecScene::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    if (SDL_GetRelativeMouseMode ())
        SDL_SetRelativeMouseMode(SDL_FALSE);

    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change camera angles

        angleY += 0.01f * event -> xrel;
        angleX += 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < -1.5f)
            angleX = -1.5f;
    }
}
void VecScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
void VecScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (event->type == SDL_KEYDOWN)
    {
        switch (event->keysym.sym)
        {
        case SDLK_x:
            mode = DISPLAY_CROSS;
            break;
        case SDLK_a:
            mode = DISPLAY_AXIS;
            break;
        }
    }
}

