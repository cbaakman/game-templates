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


#include "hub.h"
#include "../util.h"
#include "../font.h"
#include "../xml.h"
#include "../err.h"

// Color for the help text in each of the scenes (RGB):
const GLfloat textColors [][3] = {{1.0f, 1.0f, 1.0f},
                                  {1.0f, 1.0f, 1.0f},
                                  {0.0f, 0.0f, 0.0f},
                                  {0.0f, 1.0f, 0.0f}};

HubScene::HubScene (App *pApp) : Scene (pApp),
    pWaterScene (NULL),
    pShadowScene (NULL),
    pToonScene (NULL),
    pMapperScene (NULL),
    pCurrent (NULL),
    alphaH(0),
    help(0)
{
    std::string nav = "F1: Dummy Test\n"
                      "F2: Water Test\n"
                      "F3: Toon Test\n"
                      "F4: Displacement map Test";

    helpText [0] = "Use w,a,s,d & SPACE to move mr. Dummy around\n"
                    "Use the mouse to move the camera\n"
                   "Press b to show mr. Dummy's bones\n"
                   "Press n to show mr. Dummy's normals\n"
                   "Press z to show collision wireframe\n" + nav ;

    helpText [1] = "Use the arrow keys, pgUp & pgDown to move the cube.\n"
                   "Use the mouse to move the camera\n" + nav ;

    helpText [2] = "Use the mouse to move the camera\n" + nav ;

    helpText [3] = "Use the mouse to move the camera\n" + nav ;


    pWaterScene = new WaterScene (pApp);

    pShadowScene = new ShadowScene (pApp);

    pToonScene = new ToonScene (pApp);

    pMapperScene = new MapperScene (pApp);

    // Start with this scene:
    pCurrent = pMapperScene;
    help = 3;
}
HubScene::~HubScene()
{
    delete pWaterScene;
    delete pShadowScene;
    delete pToonScene;
    delete pMapperScene;
}
bool HubScene::Init ()
{
    std::string resPath = std::string(SDL_GetBasePath()) + "test3d.zip";
    bool success;

    if (!pWaterScene->Init())
        return false;

    if (!pShadowScene->Init())
        return false;

    if (!pToonScene->Init())
        return false;

    if (!pMapperScene->Init())
        return false;

    SDL_RWops *fontInput = SDL_RWFromZipArchive (resPath.c_str(), "Lumean.svg");
    if (!fontInput) // file or archive missing
        return false;

    // Parse the svg as xml document:
    xmlDocPtr pDoc = ParseXML (fontInput);
    fontInput->close (fontInput);

    if (!pDoc)
    {
        SetError ("error parsing Lumean.svg: %s", GetError ());
        return false;
    }

    // Convert xml to font object:
    success = ParseSVGFont (pDoc, 16, &font);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing Lumean.svg: %s", GetError ());
        return false;
    }

    return true;
}
void HubScene::Update (float dt)
{
    // Update current scene:
    pCurrent -> Update (dt);

    /*
        Update help text alpha.
        If the key is down alpha must be 1.0,
        else decrease gradually.
     */
    const Uint8 *state = SDL_GetKeyboardState (NULL);
    if (state [SDL_SCANCODE_H])
    {
        alphaH = 1.0f;
    }
    else if (alphaH > 0.0f)
        alphaH -= dt;
}
void HubScene::Render ()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    glViewport (0, 0, w, h);

    // Render current scene:
    pCurrent -> Render ();

    // Render the text:
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_LIGHTING);
    glDisable (GL_STENCIL_TEST);
    glDisable (GL_CULL_FACE);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
        Screen is a w x h rectangle.
        Positive y means down.
     */
    glMatrixMode(GL_PROJECTION);
    matrix4 matScreen = matOrtho(0.0f, w, 0.0f, h, -1.0f, 1.0f);
    glLoadMatrixf (matScreen.m);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMultMatrixf (matTranslation(10.0f, 10.0f, 0.0f).m);

    /*
        When the help key is down (alpha = 1.0), help is shown.
        When alpha goes down to 0.0, "Press 'h' for help shows up"
     */

    GLfloat maxWidth = w - 20.0f;
    glColor4f (textColors [help][0], textColors [help][1], textColors [help][2], 1.0f - alphaH);
    glRenderText (&font, "Press 'h' for help", TEXTALIGN_LEFT, maxWidth);

    glColor4f (textColors [help][0], textColors [help][1], textColors [help][2], alphaH);
    glRenderText (&font, helpText [help].c_str(), TEXTALIGN_LEFT, maxWidth);
}
void HubScene::OnEvent(const SDL_Event *event)
{
    Scene :: OnEvent (event);

    // pass on event to current scene:
    pCurrent -> OnEvent (event);
}
void HubScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if(event->type == SDL_KEYDOWN)
    {
        if(event->keysym.sym == SDLK_ESCAPE)
            pApp->ShutDown();

        // These keys switch between scenes:

        if(event->keysym.sym == SDLK_F1)
        {
            pCurrent = pShadowScene;
            help = 0;
        }

        if(event->keysym.sym == SDLK_F2)
        {
            pCurrent = pWaterScene;
            help = 1;
        }

        if(event->keysym.sym == SDLK_F3)
        {
            pCurrent = pToonScene;
            help = 2;
        }

        if(event->keysym.sym == SDLK_F4)
        {
            pCurrent = pMapperScene;
            help = 3;
        }
    }
}
