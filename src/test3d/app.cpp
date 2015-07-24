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


#include "app.h"

#include "hub.h"
#include "water.h"
#include "../util.h"

#include "../random.h"
#include "../err.h"

#include "../ini.h"

#include <stdio.h>
#include <algorithm>

#ifdef _WIN32
#include "winres.h"
#include <SDL2/SDL_syswm.h>
#endif

// Constructor
App::App(void) : pScene(0), done(false), fullscreen (false)
{
#ifdef _WIN32
    icon = NULL;
#endif

    settings_path [0] = NULL;
}
// Destructor
App::~App(void)
{
    Cleanup();
}
void App::ShutDown(void)
{
    done = true;
}

App :: Scene :: Scene (App *p)
{
    pApp = p;
}

#define SETTINGS_FILE "settings.ini"

#define FULLSCREEN_SETTING "fullscreen"
#define SCREENWIDTH_SETTING "screenwidth"
#define SCREENHEIGHT_SETTING "screenheight"

// Initialization functions
bool App::InitApp (void)
{
    strcpy (settings_path, (std::string (SDL_GetBasePath()) + SETTINGS_FILE).c_str());

    int w, h;

    // initialize SDL with screen sizes from settings file:
    w = LoadSetting (settings_path, SCREENWIDTH_SETTING);
    if (w < 640)
        w = 640;

    h = LoadSetting (settings_path, SCREENHEIGHT_SETTING);
    if (h < 480)
        h = 480;

    fullscreen = LoadSetting (settings_path, FULLSCREEN_SETTING) > 0 ? true : false;

    // Create a window.
    if (!InitializeSDL(w, h))
        return false;

    if (!InitializeGL())
        return false;

    pScene = new HubScene(this);

    RandomSeed ();

    if (!pScene->Init())
        return false;

    if (!CheckGLOK("scene init"))
        return false;

    return true;
}

#define MIN_STENCIL_BITS 8

bool App::InitializeGL(void)
{
    mainGLContext = SDL_GL_CreateContext (mainWindow);

    GLenum err = glewInit ();
    if (GLEW_OK != err)
    {
        SetError ("glewInit failed: %s", glewGetErrorString (err));
        return false;
    }

    if (!GL_VERSION_2_0)
    {
        SetError ("OpenGL version 2.0 is not enabled.");
        return false;
    }

    if (!CheckGLOK("GL init"))
        return false;

    GLint stencilGL; int stencilSDL;
    if (SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencilSDL) != 0 || stencilSDL < MIN_STENCIL_BITS)
    {
        SetError ("SDL: Couldn\'t get a stencil buffer with depth %d", MIN_STENCIL_BITS);
        return false;
    }
    glGetIntegerv (GL_STENCIL_BITS, &stencilGL);
    if (stencilGL < MIN_STENCIL_BITS)
    {
        SetError ("openGL: Couldn\'t get a stencil buffer with depth %d", MIN_STENCIL_BITS);
        return false;
    }

    return true;
}

bool App::InitializeSDL (Uint32 width, Uint32 height)
{
    int error;

    error = SDL_Init (SDL_INIT_EVERYTHING);
    if(error != 0)
    {
        SetError ("Unable to initialize SDL: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute (SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, 4);

    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL ;
    if (fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_INPUT_GRABBED;

    // Create the window
    mainWindow = SDL_CreateWindow ("3D Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);

#ifdef _WIN32
    /*
      In windows executables, the icon is simply included.
      However, we must still attach it to the window.
    */

    SDL_SysWMinfo wm_info;
    SDL_VERSION (&wm_info.version);
    if (!SDL_GetWindowWMInfo (mainWindow, &wm_info))
    {
        SetError ("SDL_GetWindowWMInfo failed: %s", SDL_GetError());
        return false;
    }

    HINSTANCE hInstance = GetModuleHandle (NULL);
    icon = LoadIcon (hInstance, MAKEINTRESOURCE(IDI_ICON));

    // Set the icon for the window
    SendMessage (wm_info.info.win.window, WM_SETICON, ICON_SMALL, (LPARAM) icon);

    // Set the icon in the task manager
    SendMessage (wm_info.info.win.window, WM_SETICON, ICON_BIG, (LPARAM) icon);
#endif

    return true;
}


// Cleanup functions
void App::Cleanup(void)
{
#ifdef _WIN32
    DestroyIcon (icon);
    icon = NULL;
#endif

    delete pScene;
    pScene = NULL;

    SDL_GL_DeleteContext (mainGLContext);
    SDL_DestroyWindow (mainWindow);
    SDL_Quit ();

    mainGLContext = NULL;
    mainWindow = NULL;
}

void ShowError (const char *msg)
{
    #ifdef DEBUG
        fprintf (stderr, "%s\n", msg);
    #else
        if (SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR, "An Error Occurred", msg, NULL) < 0)
        { // if box display failed
            fprintf (stderr, "%s\n", msg);
        }
    #endif // DEBUG
}

// Event-related functions
void App::MainLoop (void)
{
    GLenum statusGL;
    SDL_Event event;
    Uint32 ticks0 = SDL_GetTicks(), ticks;
    float dt;
    char errorString[260];

    while(!done) {

#ifdef DEBUG
        while ((statusGL = glGetError()) != GL_NO_ERROR)
        {
            GLErrorString (errorString, statusGL);
            std::string msg = std::string ("GL Error during main loop: %s") + errorString;
            ShowError (msg.c_str());
        }
#endif // DEBUG

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleEvent(&event);
        }

        ticks = SDL_GetTicks();
        dt = float(ticks - ticks0) / 1000;
        ticks0 = ticks;

        if (dt > 0.2f)
            dt = 0.2f;

        pScene->Update(dt);
        pScene->Render();

        SDL_GL_SwapWindow (mainWindow);

    }   // End while
}

void App :: Scene :: OnEvent (const SDL_Event *event)
{
    switch(event->type) {

        case SDL_KEYDOWN:

            if (event->key.repeat == 0)
                this -> OnKeyPress (&event -> key);
            break;

        case SDL_KEYUP:

            break;

        case SDL_MOUSEMOTION:

            this -> OnMouseMove (&event -> motion);
            break;

        case SDL_MOUSEWHEEL :

            this -> OnMouseWheel (&event -> wheel);
            break;

        default:
            break;

    }   // End switch
}
bool App::SetFullScreen (const bool want_fullscreen)
{
    if (want_fullscreen == fullscreen)
        return true;

    if (want_fullscreen)
    {
        SDL_SetWindowPosition (mainWindow, 0, 0);

        int x, y, w, h;
        SDL_GetWindowSize (mainWindow, &w, &h);
        SDL_GetMouseState (&x, &y);

        // Clamp mouse position
        x = std::max (0, std::min (x, w));
        y = std::max (0, std::min (y, h));
        SDL_WarpMouseInWindow (mainWindow, x, y);
    }

    Uint32 flags = want_fullscreen? SDL_WINDOW_FULLSCREEN : 0;

    if (SDL_SetWindowFullscreen (mainWindow, flags) < 0)
    {
        SetError ("Cannot set fullscreen to %d: %s", want_fullscreen, SDL_GetError ());
        return false;
    }

    fullscreen = want_fullscreen;
    SaveSetting (settings_path, FULLSCREEN_SETTING, fullscreen);

    // Some final adjustments
    if (fullscreen)
    {
        SDL_SetWindowGrab (mainWindow, SDL_TRUE);
    }
    else
    {
        SDL_SetWindowGrab (mainWindow, SDL_FALSE);
        SDL_SetWindowPosition (mainWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    return true;
}
bool App::SetResolution (const int w, const int h)
{
    SDL_DisplayMode mode;

    SDL_SetWindowSize (mainWindow, w, h);

    if (SDL_GetWindowDisplayMode (mainWindow, &mode) < 0)
    {
        SetError ("Error getting display mode: %s", SDL_GetError ());
        return false;
    }

    mode.w = w;
    mode.h = h;

    if (SDL_SetWindowDisplayMode (mainWindow, &mode) < 0)
    {
        SetError ("Error setting display to %d x %d %s", w, h, SDL_GetError ());
        return false;
    }

    if (fullscreen) //  need to switch to windowed to take effect
    {
        if (!SetFullScreen (false))
            return false;

        if (!SetFullScreen (true))
            return false;
    }

    SaveSetting (settings_path, SCREENWIDTH_SETTING, w);
    SaveSetting (settings_path, SCREENHEIGHT_SETTING, h);

    return true;
}
void App::HandleEvent (const SDL_Event *event)
{
    switch(event->type) {

        case SDL_QUIT:
            done = true;
            break;

        default:

            if (pScene)
                pScene -> OnEvent(event);
            break;

    } // End switch
}

int main (int argc, char *argv[])
{
    App app;

    if (!app.InitApp())
    {
        ShowError (GetError ());

        return 1;
    }

    app.MainLoop();
    app.Cleanup();

    return 0;
}

