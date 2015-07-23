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


#ifndef APP_H
#define APP_H

#include <GL/glew.h>
#include <GL/gl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <stdio.h>

class App
{
public:

    class Scene
    {
    protected:
        App *pApp;

    public:
        Scene (App *);
        virtual void OnEvent (const SDL_Event *event);

        virtual bool Init (void) { return true; }
        virtual void Update (float dt) {}
        virtual void Render (void) = 0;

        virtual void OnMouseMove (const SDL_MouseMotionEvent *event) {}
        virtual void OnKeyPress (const SDL_KeyboardEvent *event) {}
        virtual void OnMouseWheel (const SDL_MouseWheelEvent *event) {}
    };

    // Constructor and destructor
    App (void);
    virtual ~App (void);

    bool InitApp (void);

    void MainLoop (void);

    // Cleanup functions
    void Cleanup (void);

    void ShutDown (void);

    SDL_Window *GetMainWindow (void) { return mainWindow; }

    bool SetFullScreen (const bool fullscreen);
    bool SetResolution (const int w, const int h);

#ifdef _WIN32
    HICON icon;
#endif

private:
    char settings_path [FILENAME_MAX];

    bool fullscreen;

    SDL_Window *mainWindow;
    SDL_GLContext mainGLContext;
    bool done;

    Scene *pScene;

    // Initialization functions
    bool InitializeGL (void);
    bool InitializeSDL (Uint32 width, Uint32 height);

    // Event functions
    void HandleEvent(const SDL_Event* event);
};

#endif // APP_H
