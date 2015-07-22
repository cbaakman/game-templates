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


//#include<SDL/SDL_syswm.h>
#include<cstdio>

#include "client.h"

#include <string>

#include "../server/server.h"

#include "login.h"

#include "../util.h"
#include "../ini.h"
#include "../err.h"

#define SETTINGS_FILE "settings.ini"

#define FULLSCREEN_SETTING "fullscreen"
#define SCREENWIDTH_SETTING "screenwidth"
#define SCREENHEIGHT_SETTING "screenheight"

#define PORT_SETTING "port"
#define HOST_SETTING "host"

#include <SDL2/SDL_mixer.h>

#ifdef _WIN32
#include "winres.h"
#include <SDL2/SDL_syswm.h>
#endif

Client::Client():
    fromServer(NULL), toServer(NULL), udpPackets(NULL), socket(NULL),

    done(false),
    pScene(NULL)
{
    settingsPath [0] = NULL;
}
Client::~Client()
{
    if (pScene)
        delete pScene;

    CleanUp();

#ifdef _WIN32
    DestroyIcon (icon);
#endif
}
Client :: Scene :: Scene (Client *p)
{
    pClient = p;
}
void ShowError (const char *msg)
{
    #ifdef DEBUG
        fprintf (stderr, "%s\n", msg);
    #else
        if (SDL_ShowSimpleMessageBox (SDL_MESSAGEBOX_ERROR, "An Error Occurred", msg, NULL) < 0)
        {
            // if box display failed
            fprintf (stderr, "%s\n", msg);
        }
    #endif // DEBUG
}
void Client::MainLoop(void)
{
    GLenum statusGL;
    SDL_Event event;
    Uint32 ticks0 = SDL_GetTicks(), ticks;
    float dt;
    char errorString[260];

    while (!done)
    {
#ifdef DEBUG
        // Check for openGL errors
        while ((statusGL = glGetError()) != GL_NO_ERROR)
        {
            GLErrorString(errorString, statusGL);
            fprintf (stderr, "GL Error during main loop: %s\n", errorString);
        }
#endif // DEBUG

        // Handle all events in queue
        SDL_Event event;
        while (SDL_PollEvent (&event)) {

            HandleEvent (&event);
        }

        while(SDLNet_UDP_Recv(socket, fromServer))
        {
            if (fromServer->address.host == serverAddress.host &&
                fromServer->address.port == serverAddress.port)
            {
                if(pScene)
                    pScene->OnServerMessage (fromServer->data, fromServer->len);
            }
        }

        // determine how much time passed since last iteration:
        ticks = SDL_GetTicks();
        dt = float(ticks - ticks0) / 1000;
        ticks0 = ticks;

        if (dt > 0.05f)
            dt = 0.05f;

        if (pScene)
        {
            pScene->Update (dt);
            pScene->Render ();
        }

        // Show whatever is rendered in the main window
        SDL_GL_SwapWindow (mainWindow);
    }
}
void Client::HandleEvent (const SDL_Event *event)
{
    // The only event that the client handles himself is SDL_QUIT
    // The rest is passed on to the current scene.

    switch(event->type) {

        case SDL_QUIT:
            ShutDown ();
            break;

        default:
            if (pScene)
                pScene -> OnEvent (event);
            break;

    } // End switch
}
void EventListener :: OnEvent (const SDL_Event *event)
{
    // Sends SDL events to the appropriate handler functions

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

        case SDL_MOUSEWHEEL:

            this -> OnMouseWheel (&event -> wheel);
            break;

        case SDL_MOUSEBUTTONDOWN:

            this -> OnMouseClick (&event -> button);
            break;

        default:
            break;

    }   // End switch
}
int main (int argc, char* argv[])
{
    Client client;

    if(!client.Init())
    {
        ShowError (GetError ());

        return 1;
    }

    client.MainLoop();
    client.CleanUp();

    return 0;
}
bool Client::SendToServer(const Uint8* data, const int len)
{
    toServer->address.host = serverAddress.host;
    toServer->address.port = serverAddress.port;

    if (len > toServer->maxlen)
    {
        fprintf (stderr, "too many bytes to send: %d max = %d\n", len, toServer->maxlen);
        return false;
    }

    memcpy (toServer -> data, data, len);
    toServer->len = len;
    SDLNet_UDP_Send (socket, -1, toServer);

    if (toServer->status != len)
    {
        fprintf (stderr, "tried to send %d bytes to server, status returned gives %d\n", len, toServer->status);
        return false;
    }

    return true;
}

bool Client::Init()
{
    strcpy (settingsPath, (std::string(SDL_GetBasePath()) + SETTINGS_FILE).c_str());

    // initialize SDL_net:
    char hostName [100];
    int port = LoadSetting (settingsPath, PORT_SETTING);
    LoadSettingString (settingsPath, HOST_SETTING, hostName);

    if (SDLNet_Init() < 0)
    {
        SetError ("SDLNet_Init: %s", SDLNet_GetError());
        return false;
    }
    if (!(socket = SDLNet_UDP_Open (0))) // the client can run on a random port
    {
        SetError ("SDLNet_UDP_Open: %s", SDLNet_GetError());
        return false;
    }
    if (SDLNet_ResolveHost (&serverAddress, hostName, port) == -1)
    {
        SetError ("SDLNet_ResolveHost with hostname %s and port %d: %s", hostName, port, SDLNet_GetError());
        return false;
    }
    if(!(udpPackets = SDLNet_AllocPacketV(2, PACKET_MAXSIZE)))
    {
        SetError ("SDLNet_AllocPacketV: %s", SDLNet_GetError());
        return false;
    }

    toServer = udpPackets[0];
    fromServer = udpPackets[1];

    // initialize SDL with screen sizes from settings file:
    w = LoadSetting (settingsPath, SCREENWIDTH_SETTING);
    if (w < 800)
        w = 800;

    h = LoadSetting (settingsPath, SCREENHEIGHT_SETTING);
    if (h < 600)
        h = 600;

    fullscreen = LoadSetting (settingsPath, FULLSCREEN_SETTING) > 0 ? true : false;

    int error = SDL_Init(SDL_INIT_EVERYTHING);
    if(error != 0)
    {
        SetError ("Unable to initialize SDL: %s", SDL_GetError());
        return false;
    }

    // Set the openGL parameters we want:
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    mainWindow = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, GetVideoFlags());

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
    HICON icon = LoadIcon (hInstance, MAKEINTRESOURCE(IDI_ICON));

    // Set the icon for the window
    SendMessage (wm_info.info.win.window, WM_SETICON, ICON_SMALL, (LPARAM) icon);

    // Set the icon in the task manager
    SendMessage (wm_info.info.win.window, WM_SETICON, ICON_BIG, (LPARAM) icon);
#endif

    mainGLContext = SDL_GL_CreateContext(mainWindow);

    // We use glew to check if our openGL version is at least 2.0

    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        SetError ("glewInit failed: %s", glewGetErrorString(err));
        return false;
    }

    if(!GL_VERSION_2_0)
    {
        SetError ("OpenGL version 2.0 is not enabled.");
        return false;
    }

    // See if nothing went wrong:
    if (!CheckGLOK("GL init"))
        return false;

    // Initialize SDL_mixer, check if audio is enabled.
    has_audio = (Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 1024) == 0);
    if (!has_audio)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning: Cannot open Audio", Mix_GetError(), NULL);

    SDL_ShowCursor (0);

    // Initialize the login scene
    pScene = new LoginScene (this);

    if (!pScene->Init())
        return false;

    return true;
}
void Client::CleanUp()
{
    SDL_GL_DeleteContext(mainGLContext);
    SDL_DestroyWindow(mainWindow);
    SDL_Quit();

    if (udpPackets)
    {
        SDLNet_FreePacketV(udpPackets);
        udpPackets = NULL;
        fromServer = toServer = NULL;
    }
    if (socket)
    {
        SDLNet_UDP_Close(socket);
        socket = NULL;
    }
    SDLNet_Quit();
    Mix_CloseAudio();
}

// Used to see what SDL videoflags we need
Uint32 Client::GetVideoFlags()
{
    Uint32 videoFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL ;
    if (fullscreen)
        videoFlags |= SDL_WINDOW_FULLSCREEN;

    return videoFlags;
}
void Client::SwitchScene (Scene *p)
{
    this->pScene = p;
}
bool Client :: WindowFocus () const
{
    return true;
}
void Client::ShutDown()
{
    done = true;

    // Tell the scene about it:
    if (pScene)
        pScene -> OnShutDown() ;
}
