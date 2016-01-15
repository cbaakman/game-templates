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


#include "login.h"
#include <math.h>
#include <openssl/x509.h>

#include "../texture.h"
#include "../util.h"
#include "../err.h"
#include "../xml.h"
#include "../thread.h"

#include "../server/server.h"

#define INPUT_TEXT_SPACING 30.0f
#define INPUT_CHAT_SPACING 10.0f
#define CHAT_VISIBILITY_TIME 7.0f // seconds
#define CHAT_ALPHA_DECREASE 1.0f // per second

void RenderSprite (Texture* tex,
                   GLfloat x, GLfloat y,
                   GLfloat tx1, GLfloat ty1, GLfloat tx2, GLfloat ty2,
                   GLfloat px=0, GLfloat py=0)
{
    glPushAttrib (GL_TEXTURE_BIT);

    glBindTexture (GL_TEXTURE_2D, tex->tex);

    GLfloat w = tx2 - tx1, h = ty2 - ty1;

    GLsizei tw = tex->w,
            th = tex->h;

    x -= px;
    y -= py;

    glBegin (GL_QUADS);
    glTexCoord2f (tx1 / tw, 1.0f - ty1 / th); glVertex2f (x,     y    );
    glTexCoord2f (tx2 / tw, 1.0f - ty1 / th); glVertex2f (x + w, y    );
    glTexCoord2f (tx2 / tw, 1.0f - ty2 / th); glVertex2f (x + w, y + h);
    glTexCoord2f (tx1 / tw, 1.0f - ty2 / th); glVertex2f (x,     y + h);
    glEnd ();

    glPopAttrib();
}

void LoginScene::UsernameBox::Render()
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor3f(1.0f,1.0f,1.0f);
    GLfloat ux = GetX()-INPUT_TEXT_SPACING, uy = GetY();
    glTranslatef (ux, uy, 0);
    glRenderText (GetFont(), "Username:", TEXTALIGN_RIGHT);
    glTranslatef (-ux, -uy, 0);

    glColor3f(0,1.0f,1.0f);
    TextInputBox::Render();

    glPopAttrib();
}
MenuObject* LoginScene::UsernameBox::NextInLine() const
{
    return parent->passwordBox;
}
void LoginScene::UsernameBox::OnAddChar(char c)
{
    Mix_PlayChannel (-1, parent->pSound, 0);
}
void LoginScene::UsernameBox::OnDelChar(char c)
{
    Mix_PlayChannel (-1, parent->pSound, 0);
}
void LoginScene::UsernameBox::OnBlock()
{
    Mix_PlayChannel (-1, parent->pSound, 0);
}
LoginScene::UsernameBox::UsernameBox (GLfloat x, GLfloat y,
                                      Font* font, LoginScene* parent)
: TextInputBox (x, y, font, USERNAME_MAXLENGTH-1, TEXTALIGN_LEFT, NULL)
{
    w = 300.0f;
    h = 50.0f;
    this->parent = parent;

    SetText("<username>");
}
void LoginScene::PasswordBox::Render()
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor3f(1.0f,1.0f,1.0f);

    GLfloat ux = GetX() - INPUT_TEXT_SPACING, uy = GetY();
    glTranslatef (ux, uy, 0);
    glRenderText (GetFont(), "Password:", TEXTALIGN_RIGHT);
    glTranslatef (-ux, -uy, 0);

    glColor3f(0,1.0f,1.0f);
    TextInputBox::Render();

    glPopAttrib();
}
void LoginScene::PasswordBox::RenderCursor(int mX, int mY)
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderSprite(&parent->cursorTex,
              mX, mY,
              32, 0, 54, 40,
              12.3f, 25.9f);

    glPopAttrib();
}
void LoginScene::UsernameBox::RenderCursor(int mX, int mY)
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderSprite(&parent->cursorTex,
              mX, mY,
              32, 0, 54, 40,
              12.3f, 25.9f);

    glPopAttrib();
}
LoginScene::LoginMenu::LoginMenu (Client *pClient, LoginScene *sc, Texture *pTex) : Menu (pClient),
    parent (sc),
    pCursorTex (pTex),
    t (0.0f)
{
}
void LoginScene::LoginMenu::Update (const float dt)
{
    t += dt;

    Menu::Update (dt);
}
void LoginScene::LoginMenu::RenderDefaultCursor(int mX, int mY)
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (parent->logging)
    {
        glPushMatrix();
        matrix4 m = matTranslation (vec3 (mX, mY, 0)) * matRotZ (8 * t);
        glLoadMatrixf(m);

        RenderSprite(pCursorTex,
                  0, 0,
                  96, 0, 128, 32,
                  16, 16);

        glPopMatrix();
    }
    else
    {
        RenderSprite(pCursorTex,
                  mX, mY,
                  0, 0, 32, 40,
                  2.4f, 2.4f);
    }

    glPopAttrib();
}
MenuObject* LoginScene::PasswordBox::NextInLine () const
{
    return parent->usernameBox;
}
void LoginScene::PasswordBox::OnAddChar (char c)
{
    Mix_PlayChannel(-1, parent->pSound, 0);
}
void LoginScene::PasswordBox::OnDelChar (char c)
{
    Mix_PlayChannel(-1, parent->pSound, 0);
}
void LoginScene::PasswordBox::OnBlock ()
{
    Mix_PlayChannel(-1, parent->pSound, 0);
}
LoginScene::LoginButton::LoginButton(GLfloat x, GLfloat y,
                                     Font *pFont, LoginScene* parent)
{
    this->x=x;
    this->y=y;
    this->pFont=pFont;
    this->parent=parent;
}
bool LoginScene::LoginButton::MouseOver(GLfloat mX, GLfloat mY) const
{
    return (mX>(x-91)&&mX<(x+91)&&mY>(y-42)&&mY<(y+42));
}
void LoginScene::LoginButton::OnMouseClick (const SDL_MouseButtonEvent *event)
{
    if (MouseOver(event->x,event->y) && event->button == SDL_BUTTON_LEFT)
    {
        parent->StartLogin();
    }
}
void LoginScene::LoginButton::RenderCursor(int mX, int mY)
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderSprite(&parent->cursorTex,
              mX, mY,
              56, 0, 90, 37,
              7, 6);

    glPopAttrib();
}
void LoginScene::LoginButton::Render()
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int mX, mY;
    SDL_GetMouseState(&mX,&mY);
    if(IsInputEnabled() && MouseOver(mX,mY))
    {
        glColor3f(0,1,1);
    }
    else
    {
        glColor3f(1,1,1);
    }

    RenderSprite(&parent->buttonTex,
                  x, y,
                  0, 0, 256, 128,
                  128, 64);

    float line_y = y - (GetLineSpacing (pFont) - 1.0f) / 2;
    glTranslatef (x, line_y, 0);
    glRenderText (pFont, "LOG IN", TEXTALIGN_MID);
    glTranslatef (-x, -line_y, 0);

    glPopAttrib();
}
LoginScene::PasswordBox::PasswordBox( GLfloat x, GLfloat y,
                                     Font* font, LoginScene* parent )
: TextInputBox (x, y, font, PASSWORD_MAXLENGTH-1, TEXTALIGN_LEFT,'*')
{
    w=300.0f;
    h=50.0f;
    this->parent = parent;

    SetText("------");
}
LoginScene::LoginScene (Client *p) : Scene(p),
    pSound (NULL),
    nextScene (NULL),

    usernameBox (NULL), passwordBox (NULL), loginButton (NULL),

    logging (false),
    loginSocket (NULL),
    t (0.0f)
{
    errorMessage[0] = NULL;

    cursorTex.tex = bgTex.tex = buttonTex.tex = 0;

    pMenu = new LoginMenu (pClient, this, &cursorTex);
}
LoginScene::~LoginScene ()
{
    SDLNet_TCP_Close (loginSocket);

    if (pSound)
        Mix_FreeChunk (pSound);

    if(nextScene && nextScene->IsLoggedIn())
        OnLogout();

    delete nextScene;

    glDeleteTextures (1, &cursorTex.tex);
    glDeleteTextures (1, &bgTex.tex);
    glDeleteTextures (1, &buttonTex.tex);

    /* Don't delete the menu objects!
       The menu will do this! */

    delete pMenu;
}
/**
 * This function runs in a separate thread.
 * Within it, it's safe to modify errorMessage
 * as long as logging is set to 'true'. When logging
 * is 'false' other threads start reading from errorMessage again!
 */
void LoginScene::LoginProc (const LoginParams params)
{
    // used to prevent blocking connections:
    #define LOGINT_CHECK if (!logging) goto login_fail;

    Uint8 buf [PACKET_MAXSIZE],
          signal = NULL;
    int n_sent, n_recieved, keyLen;
    RSA *publicKey;

    LOGINT_CHECK;

    loginSocket = pClient->Server_TCP_Connect ();
    if (!loginSocket)
    {
        strcpy (errorMessage, "Error connecting to the server!");
        goto login_fail;
    }

    LOGINT_CHECK;

    signal = NETSIG_LOGINREQUEST;
    if (SDLNet_TCP_Send (loginSocket, &signal, 1) != 1)
    {
        strcpy (errorMessage, "Disconnected from the server");
        goto login_fail;
    }

    LOGINT_CHECK;

    // Expect response from server here:
    if ((n_recieved = SDLNet_TCP_Recv (loginSocket, buf, PACKET_MAXSIZE)) <= 0)
    {
        strcpy (errorMessage, "Disconnected from the server");
        goto login_fail;
    }

    LOGINT_CHECK;

    signal = buf [0];

    if (signal == NETSIG_SERVERFULL)
    {
        strcpy (errorMessage, "Server is full");
        goto login_fail;
    }
    else if (signal == NETSIG_INTERNALERROR)
    {
        strcpy (errorMessage, "Serverside Internal Error");
        goto login_fail;
    }
    else if (signal == NETSIG_RSAPUBLICKEY)
    {
        if (n_recieved <= 1)
        {
            strcpy (errorMessage, "Disconnected from the server");

            fprintf (stderr, "missing attached public key\n");
            goto login_fail;
        }

        const unsigned char *pKey = buf + 1;
        publicKey = d2i_RSAPublicKey (NULL, &pKey, n_recieved - 1);
        if (!publicKey)
        {
            strcpy (errorMessage, "Disconnected from the server");

            fprintf (stderr, "d2i_RSAPublicKey failed\n");
            goto login_fail;
        }

        const int encSZ = RSA_size (publicKey); // encrypted data buffer size
        if (maxFLEN (publicKey) < sizeof (LoginParams)) // must fit in
        {
            strcpy (errorMessage, "Disconnected from the server");

            fprintf (stderr, "data too big for rsa key\n");
            goto login_fail;
        }

        Uint8 *encrypted = new Uint8 [encSZ];

        int encrypted_len = RSA_public_encrypt (sizeof (LoginParams), (const unsigned char *)&params, encrypted, publicKey, SERVER_RSA_PADDING);
        if (SDLNet_TCP_Send (loginSocket, encrypted, encrypted_len) != encrypted_len)
        {
            strcpy (errorMessage, "Disconnected from the server");

            fprintf (stderr, "error while sending encrypted login: %s\n", SDLNet_GetError ());
            goto login_fail;
        }

        delete [] encrypted;
        RSA_free (publicKey);
    }
    else // invalid signal
    {
        strcpy (errorMessage, "Disconnected from the server");
        goto login_fail;
    }

    LOGINT_CHECK;

    // Expect response from server here:
    if ((n_recieved = SDLNet_TCP_Recv (loginSocket, buf, PACKET_MAXSIZE)) <= 0)
    {
        if (n_recieved < 0)
            fprintf (stderr, "error while recieving server response: %s\n", SDLNet_GetError ());
        else
            fprintf (stderr, "server hung up before responding\n");

        strcpy (errorMessage, "Disconnected from the server");
        goto login_fail;
    }

    LOGINT_CHECK;

    signal = buf [0];

    if (signal == NETSIG_ALREADYLOGGEDIN)
    {
        strcpy (errorMessage, "Already logged in");
        goto login_fail;
    }
    else if (signal == NETSIG_SERVERFULL)
    {
        strcpy (errorMessage, "Server is full");
        goto login_fail;
    }
    else if (signal == NETSIG_AUTHENTICATIONERROR)
    {
        strcpy (errorMessage, "Authentication error");
        goto login_fail;
    }
    else if (signal == NETSIG_LOGINSUCCESS)
    {
        UserParams *pUserParams = (UserParams *) (buf + 1);
        UserState *pState = (UserState *)(buf + 1 + sizeof (UserParams));

        OnLogin (pUserParams, pState);
        return;
    }

login_fail:

    CancelLogin ();
}
void LoginScene::StartLogin ()
{
    const char  *un = usernameBox->GetText(),
                *pw = passwordBox->GetText();

    // Remove any previous error messages:
    errorMessage [0] = NULL;

    if (!(*un && *pw))
    {
        strcpy (errorMessage, "Please fill in a username AND a password!");
        return;
    }

    // Make a new scene that follows the login screen:
    delete nextScene;
    nextScene = new TestConnectionScene (pClient, this, &cursorTex, pSound, &font, &small_font);
    if (!nextScene->Init())
    {
        sprintf (errorMessage, "Error loading TestConnectionScene: %s", GetError ());
        return;
    }

    // Username and password must be passed on to the thread by value.
    LoginParams params;
    GetLoginParams (&params);

    // Keeps track of how long the login takes:
    loginTime = 0.0f;

    // Change the scene's GUI state:
    logging = true;
    pMenu->DisableInput ();

    // Try to connect in the background:
    SDL_Thread *pLoginThread = MakeSDLThread
    (
        [&] ()
        {
            LoginProc (params);

            return 0;
        },
        "login_thread"
    );

    /*
        Since the login thread takes care of the login procedure
        and the main thread is just for user input, we don't need
        to join them. The login thread will simply start the next scene
        when done (or re-enable this one). And the main thread does not need to have
        a reference allocated to this thread.
     */
    SDL_DetachThread (pLoginThread);
}
void LoginScene::GetLoginParams (LoginParams *p)
{
    const char  *un = usernameBox->GetText(),
                *pw = passwordBox->GetText();

    if (!*un || !*pw)
    {
        fprintf (stderr, "username or password empty\n");
        return;
    }

    // Place username and password in package:
    strcpy (p->username, un);
    strcpy (p->password, pw);
    p->udp_port = pClient->GetUDPAddress ()->port;
}
bool LoginScene::Init ()
{
    std::string archive = std::string(SDL_GetBasePath()) + "client.zip";

    SDL_RWops *f;
    bool success;

    f = SDL_RWFromZipArchive (archive.c_str(), "handwriting.svg");
    if (!f) // file or archive missing
        return false;

    xmlDocPtr pDoc = ParseXML(f);
    f->close (f);

    if (!pDoc)
    {
        SetError ("error parsing handwriting.svg: %s", GetError ());
        return false;
    }

    if (!ParseSVGFont (pDoc, 16, &small_font))
    {
        SetError ("error parsing handwriting.svg: %s", GetError ());
        return false;
    }

    success = ParseSVGFont (pDoc, 28, &font);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing handwriting.svg: %s", GetError ());
        return false;
    }

    f = SDL_RWFromZipArchive (archive.c_str(), "button.png");
    if (!f)
        return false;
    success = LoadPNG (f, &buttonTex);
    f->close (f);

    if (!success)
    {
        SetError ("Error reading button.png: %s", GetError ());
        return false;
    }

    f = SDL_RWFromZipArchive (archive.c_str(), "bluebg.png");
    if (!f)
        return false;
    success = LoadPNG (f, &bgTex);
    f->close (f);

    if (!success)
    {
        SetError ("Error reading bluebg.png: %s", GetError ());
        return false;
    }

    f = SDL_RWFromZipArchive (archive.c_str(), "cursor.png");
    if (!f)
        return false;
    success = LoadPNG (f, &cursorTex);
    f->close (f);

    if (!success)
    {
        SetError ("Error reading cursor.png: %s", GetError ());
        return false;
    }

    if (pClient->AudioEnabled ())
    {
        f = SDL_RWFromZipArchive (archive.c_str(), "menu.wav");
        if (!f)
            return false;
        pSound = Mix_LoadWAV_RW(f, false);
        f->close (f);

        if (!pSound)
        {
            SetError ("error loading menu.wav: %s", Mix_GetError());
            return false;
        }
    }

    int w, h;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &w, &h);

    usernameBox = new UsernameBox (w * 0.5f, 100.0f, &font, this);
    passwordBox = new PasswordBox (w * 0.5f, 150.0f, &font, this);
    loginButton = new LoginButton (w * 0.5f, 250.0f, &font, this);

    pMenu->AddMenuObject (usernameBox);
    pMenu->AddMenuObject (passwordBox);
    pMenu->AddMenuObject (loginButton);

    pMenu->FocusMenuObject (usernameBox);

    return true;
}
void LoginScene::OnEvent (const SDL_Event *event)
{
    pMenu->OnEvent (event);

    Scene::OnEvent (event);

    // screen size might have changed, put menu items in middle of screen

    int w, h;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &w, &h);

    usernameBox->SetX (w * 0.5f);
    passwordBox->SetX (w * 0.5f);
    loginButton->SetX (w * 0.5f);
}
void LoginScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    switch(event->keysym.sym)
    {
    case SDLK_RETURN:
        StartLogin ();
    break;
    case SDLK_ESCAPE:
        pClient->ShutDown ();
        return;

    default: return;
    }
}
void LoginScene::OnServerMessage (const Uint8*data,int len)
{
    char signature = data [0];

    if(nextScene && (
        signature == NETSIG_USERSTATE ||
        signature == NETSIG_ADDPLAYER ||
        signature == NETSIG_DELPLAYER
        )) // this message is meant for the next scene
    {
        fprintf(stderr, "warning: login recieves data from the server for next scene\n");
        nextScene->OnServerMessage (data, len);
    }

    data++; len--;
}
void LoginScene::CancelLogin (void)
{
    // Close the tcp connection:
    SDLNet_TCP_Close (loginSocket);
    loginSocket = NULL;

    // Change the GUI back to the original state:
    pMenu->EnableInput ();
    logging = false;
}
void LoginScene::OnLogin (UserParams* params, UserState* state)
{
    // Now it's time to activate the next scene:
    pClient->SwitchScene (nextScene);
    nextScene->OnLogin (GetUsername(), params, state);

    logging = false;

    // Close the tcp connection:
    SDLNet_TCP_Close (loginSocket);
    loginSocket = NULL;
}
void LoginScene::OnLogout ()
{
    Uint8 message = NETSIG_LOGOUT;
    pClient->SendToServer (&message, 1);

    pMenu->EnableInput ();
}
void LoginScene::OnConnectionLoss ()
{
    pMenu->EnableInput ();
    strcpy (errorMessage, "Disconnected from Server!");
}
void LoginScene::Update (const float dt)
{
    t += dt;

    if (logging)
    {
        loginTime += dt;
        if (loginTime > CONNECTION_TIMEOUT)
        {
            strcpy (errorMessage, "Connection Timeout!");

            CancelLogin ();
        }
    }

    pMenu->Update (dt);
}
void LoginScene::Render ()
{
    int screenWidth, screenHeight;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &screenWidth, &screenHeight);

    // set a 2D projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(
        matOrtho(0, (GLfloat)screenWidth,
                 0, (GLfloat)screenHeight,
                 -1.0f, 1.0f)
                 );

    // start operating on the modelview matrix from here
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // settings for Rendering transparent textures:
    glPushAttrib(GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_LIGHTING_BIT|GL_POLYGON_BIT|GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);

    // Render the background:
    glBindTexture(GL_TEXTURE_2D, bgTex.tex);
    GLfloat bgx = t * -0.2f,
            bgy = t * -0.1f;
    GLsizei bgtw, bgth;
    bgtw = GLfloat (screenWidth) / bgTex.w;
    bgth = GLfloat (screenHeight) / bgTex.h;

    glBegin(GL_QUADS);
    glTexCoord2f (bgx, bgy + bgth);
    glVertex2f (0, 0);

    glTexCoord2f (bgx + bgtw, bgy + bgth);
    glVertex2f (screenWidth, 0);

    glTexCoord2f (bgx + bgtw, bgy);
    glVertex2f (screenWidth, screenHeight);

    glTexCoord2f (bgx, bgy);
    glVertex2f (0, screenHeight);
    glEnd();

#define MESSAGE_Y 350

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if(logging)
    {
        glColor3f(1,1,1);

        GLfloat msgX = GLfloat(screenWidth)/2,
                msgY = MESSAGE_Y+5*sin(8*t);

        glTranslatef (msgX, msgY, 0);
        glRenderText (&font, "Logging in, please wait...", TEXTALIGN_MID);
        glTranslatef (-msgX, -msgY, 0);
    }
    else if (errorMessage[0])
    {
        glColor3f(1,0.2f,0);

        GLfloat msgX = GLfloat(screenWidth)/2,
                msgY = MESSAGE_Y;

        glTranslatef (msgX, msgY, 0);
        glRenderText (&font, errorMessage, TEXTALIGN_MID);
        glTranslatef (-msgX, -msgY, 0);
    }

    glPopAttrib();

    // Render the menu and all its components:
    pMenu->Render();

    glPopMatrix();
}
const char* LoginScene::GetUsername () const
{
    return usernameBox->GetText();
}
void LoginScene::SetMenuEnabled (bool enable)
{
    if (enable)
        pMenu->EnableInput ();
    else
        pMenu->DisableInput ();
}

TestConnectionScene::ChatInput::ChatInput (const GLfloat x, const GLfloat y, const Font *pFont, const int maxTextLength)
 : TextInputBox (x, y, pFont, maxTextLength, TEXTALIGN_LEFT, NULL)
{
}
void TestConnectionScene::ChatInput::OnFocusLose ()
{
    SetText ("");
}
void TestConnectionScene::ChatInput::Render ()
{
    if (IsFocussed())
    {
        glPushAttrib(
            GL_CURRENT_BIT|
            GL_COLOR_BUFFER_BIT|
            GL_DEPTH_BUFFER_BIT|
            GL_LIGHTING_BIT|
            GL_POLYGON_BIT|
            GL_TEXTURE_BIT);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);

        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLfloat x = GetX () - INPUT_CHAT_SPACING,
                y = GetY ();
        glTranslatef (x, y, 0.0f);
        glRenderText (GetFont(), "say:", TEXTALIGN_RIGHT);
        glTranslatef (-x, -y, 0.0f);

        glPopAttrib();
    }

    TextInputBox::Render ();
}
TestConnectionScene::ChatScroll::ChatScroll (TestConnectionScene *_parent, const Font *pFont, const float screen_height, const float screen_width)
 : TextScroll (10.0f, screen_height - 160.0f,
               300.0f, 4.0f, 120.0f,
               0.0f, 6.0f, -3.0f,
               pFont, TEXTALIGN_LEFT),
    parent (_parent),
    alpha (0.0f)
{
}
void RenderQuad (const GLfloat qx1, const GLfloat qy1, const GLfloat qx2, const GLfloat qy2,
                 const GLfloat tx1, const GLfloat ty1, const GLfloat tx2, const GLfloat ty2)
{
    glTexCoord2f (tx1, ty1);
    glVertex2f (qx1, qy1);
    glTexCoord2f (tx2, ty1);
    glVertex2f (qx2, qy1);
    glTexCoord2f (tx2, ty2);
    glVertex2f (qx2, qy2);
    glTexCoord2f (tx1, ty2);
    glVertex2f (qx1, qy2);
}
void TestConnectionScene::ChatScroll::ResetAlpha ()
{
     alpha = 1.0f + CHAT_VISIBILITY_TIME * CHAT_ALPHA_DECREASE;
}
void TestConnectionScene::ChatScroll::Update (const float dt)
{
    if (parent->pChatInput->IsFocussed ())
    {
        ResetAlpha ();
    }
    else if (alpha > 0.0f)
        alpha -= CHAT_ALPHA_DECREASE * dt;

    if (alpha < 0.0f)
    {
        alpha = 0.0f;
        SetScroll (GetMaxBarY ()); // invisible, move chat down
    }
}
void TestConnectionScene::ChatScroll::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    if (event->y != 0)
        ResetAlpha ();

    TextScroll::OnMouseWheel (event);
}
void TestConnectionScene::ChatScroll::Render ()
{
    float strip_x1, strip_x2, strip_y1, strip_y2,
          bar_x1, bar_x2, bar_y1, bar_y2,
          text_x1, text_y1, text_x2, text_y2;

    GLfloat qx1, qx2, qy1, qy2, tx1, tx2, ty1, ty2;

    GetStripRect (strip_x1, strip_y1, strip_x2, strip_y2);
    GetBarRect (bar_x1, bar_y1, bar_x2, bar_y2);
    GetTextRect (text_x1, text_y1, text_x2, text_y2);

    glPushAttrib (GL_DEPTH_BUFFER_BIT|
                  GL_COLOR_BUFFER_BIT|
                  GL_STENCIL_BUFFER_BIT|
                  GL_CURRENT_BIT|
                  GL_LIGHTING_BIT|
                  GL_POLYGON_BIT|
                  GL_TEXTURE_BIT);

    glDisable (GL_DEPTH_TEST);
    glDisable (GL_LIGHTING);
    glDisable (GL_CULL_FACE);

    glEnable (GL_BLEND);
    glEnable (GL_TEXTURE_2D);

    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f (1, 1, 1, alpha);

    // can we actually scroll? If not, then don't render the bar or strip
    if (GetMinBarY () < GetMaxBarY ())
    {
        // stretch the texture images so that they fit onto the strip and bar
        glBindTexture (GL_TEXTURE_2D, parent->scrollTex.tex);

        GLfloat tys [4] = {138.0f + 5.0f, 118.0f, 30.0f, 5.0f},
                qys [4];
        int ny, nx;

        glBegin (GL_QUADS);

        qys [0] = strip_y1 - 5;
        qys [1] = strip_y1 + 20;
        qys [2] = strip_y2 - 20;
        qys [3] = strip_y2 + 5;

        qx1 = (strip_x1 + strip_x2) / 2 - 5.0f;
        qx2 = qx1 + 10.0f;

        tx1 = (12.0f - 5.0f) / parent->scrollTex.w;
        tx2 = (12.0f + 5.0f) / parent->scrollTex.w;

        for (ny = 0; ny < 3; ny++)
        {
            qy1 = qys [ny];
            qy2 = qys [ny + 1];

            ty1 = tys [ny] / parent->scrollTex.h;
            ty2 = tys [ny + 1] / parent->scrollTex.h;

            RenderQuad (qx1, qy1, qx2, qy2, tx1, ty1, tx2, ty2);
        }

        qys [0] = bar_y1 - 5;
        qys [1] = bar_y1 + 20;
        qys [2] = bar_y2 - 20;
        qys [3] = bar_y2 + 5;

        qx1 = (bar_x1 + bar_x2) / 2 - 5.0f;
        qx2 = qx1 + 10.0f;

        tx1 = (34.0f - 5.0f) / parent->scrollTex.w;
        tx2 = (34.0f + 5.0f) / parent->scrollTex.w;

        for (ny = 0; ny < 3; ny++)
        {
            qy1 = qys [ny];
            qy2 = qys [ny + 1];

            ty1 = tys [ny] / parent->scrollTex.h;
            ty2 = tys [ny + 1] / parent->scrollTex.h;

            RenderQuad (qx1, qy1, qx2, qy2, tx1, ty1, tx2, ty2);
        }

        glEnd ();
    }

    GLdouble plane1 [] = {0.0f, 1.0f, 0.0f, -text_y1},
             plane2 [] = {0.0f, -1.0f, 0.0f, text_y2};
    glClipPlane (GL_CLIP_PLANE0, plane1);
    glClipPlane (GL_CLIP_PLANE1, plane2);
    glEnable (GL_CLIP_PLANE0);
    glEnable (GL_CLIP_PLANE1);

    RenderText ();

    glDisable (GL_CLIP_PLANE0);
    glDisable (GL_CLIP_PLANE1);

    glPopAttrib ();
}
TestConnectionScene::TestConnectionScene(Client *p, LoginScene* s, Texture* cursorTex, Mix_Chunk *_pSound, const Font *pFont, const Font *pSmallFont)
: ConnectedScene(p),
    loginScene (s),
    pSound (_pSound),
    tex (cursorTex)
{
    fnt = pFont;
    scrollTex.tex = NULL;
    t = 0;
    posPer = 0;
    myParams.hue = 0; // red

    serverStartTicks = 0;

    myUsername [0] = NULL; // not known until login

    chat_text = "";

    int w, h;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &w, &h);

    // These are menu objects without a menu, the scene must handle them by itself.
    // We don't use a menu here because the mouse has a different function here.
    // The chat input and scroll are activated by buttons, rather than being clicked.
    pChatInput = new ChatInput (50, h - 30, pSmallFont, MAX_CHAT_LENGTH - 1);
    pChatScroll = new ChatScroll (this, pSmallFont, h, w);
}
bool TestConnectionScene::Init (void)
{
    /*
     * Make the TestConnectionScene load at least one resource by itself to see that works too:
     */

    std::string archive = std::string (SDL_GetBasePath()) + "client.zip";

    SDL_RWops *f;
    bool success;

    f = SDL_RWFromZipArchive (archive.c_str(), "scroll.png");
    if (!f)
        return false;
    success = LoadPNG (f, &scrollTex);
    f->close (f);

    if (!success)
    {
        SetError ("Error reading scroll.png: %s", GetError ());
        return false;
    }

    return true;
}
TestConnectionScene::~TestConnectionScene()
{
    delete pChatInput;
    delete pChatScroll;

    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        delete other;
    }
}
void TestConnectionScene::OnLogin(const char* username, UserParams* params, UserState* state)
{
    ConnectedScene::OnLogin(username, params, state);

    SDL_SetWindowTitle (pClient->GetMainWindow (), username);

    strcpy(myUsername,username);
    myParams = *params;
    me = *state;

    // see if I'm in this list already
    for (std::list <RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if (strcmp (other->username,username)==0)
        {
            // If so, then take my parameters from it
            myParams = other->params;
            delete other;
            it = others.erase (it);
            break;
        }
    }

    int mX,mY; SDL_GetMouseState(&mX,&mY); me.pos = vec2(mX,mY);
}
#define UPDATE_POS_PERIOD 0.1 // seconds
void TestConnectionScene::Update(const float dt)
{
    t += dt;

    int mX, mY;
    SDL_GetMouseState (&mX, &mY);
    me.pos = vec2 (mX, mY);

    posPer += dt;
    if(posPer > UPDATE_POS_PERIOD)
    {
        SendState();
        posPer=0;
    }

    Uint32 currentTicks=GetCurrentTicks();
    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if( currentTicks > (other->next.ticks + CONNECTION_TIMEOUT*1000) )
        {
            // forget players of which no more info is recieved
            OnForgetUser (other);
        }
    }

    pChatInput->Update (dt);
    pChatScroll->Update (dt);

    ConnectedScene::Update (dt);
}
void TestConnectionScene::SendState()
{
    int len=1+sizeof(UserState);
    Uint8* data = new Uint8[len];
    data[0]=NETSIG_USERSTATE;
    memcpy(data+1,&me,sizeof(UserState));
    pClient->SendToServer(data,len);
    delete[] data;
}
void TestConnectionScene::Render()
{
    int screenWidth, screenHeight;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &screenWidth, &screenHeight);

    // set a 2D projection matrix
    glMatrixMode (GL_PROJECTION);
    glLoadMatrixf(
        matOrtho(0, (GLfloat) screenWidth,
                 0, (GLfloat) screenHeight,
                 -1.0f, 1.0f)
                 );

    glMatrixMode(GL_MODELVIEW);

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render the smileys:
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int mX,mY;
    SDL_GetMouseState (&mX, &mY);

    if (mX > 0 && mX < screenWidth && mY > 0 && mY < screenHeight)
    {
        glPushMatrix();
        matrix4 m =  matTranslation (vec3 (mX, mY, 0)) * matRotZ (0.7 * sin(8 * t));
        glLoadMatrixf(m);

        glColorHSV (myParams.hue, 1.0f, 1.0f);
        RenderSprite(tex,
                  0, 0,
                  96, 32, 128, 64,
                  16, 16);

        glPopMatrix();
    }

    // Render the other users too:
    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if (strcmp (other->username, myUsername) == 0)
            continue; // this is me

        UserState now;
        Predict(&now,&other->prev,&other->next);

        if (now.pos.x > 0 && now.pos.x < screenWidth && now.pos.y > 0 && now.pos.y < screenHeight)
        {
            glPushMatrix();
            matrix4 m = matTranslation (vec3 (now.pos.x, now.pos.y, 0)) * matRotZ (0.7 * sin(8 * t));
            glLoadMatrixf(m);

            glColorHSV (other->params.hue, 1.0f, 1.0f);
            RenderSprite(tex,
                      0, 0,
                      96, 32, 128, 64,
                      16, 16);

            glPopMatrix();
        }
    }

    glPopAttrib();

    // Render the ping text to the screen:
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1,1,1);
    static char pingText[64];
    sprintf(pingText,"ping: %u",GetPing());

    glTranslatef (10, 10, 0);
    glRenderText (fnt, pingText, TEXTALIGN_LEFT);
    glTranslatef (-10, -10, 0);

    glPopAttrib();

    pChatInput->Render ();
    pChatScroll->Render ();
}
void TestConnectionScene::OnEvent (const SDL_Event *event)
{
    ConnectedScene::OnEvent (event);

    if (pChatInput->IsFocussed ())
        pChatInput->OnEvent (event);

    pChatScroll->OnEvent (event);
}
void TestConnectionScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    switch(event->keysym.sym)
    {
    case SDLK_ESCAPE:

        if (pChatInput->IsFocussed ())
        {
            pChatInput->SetFocus (false);
        }
        else
        {
            Logout();
        }
        return;

    case SDLK_RETURN:

        if (pChatInput->IsFocussed ())
        {
            SendChatMessage (pChatInput->GetText ());

            pChatInput->SetFocus (false);

            // scroll down in the chat when enter is pressed:
            pChatScroll->SetScroll (pChatScroll->GetMaxBarY ());
        }
        else
        {
            pChatInput->SetFocus (true);

            pChatScroll->ResetAlpha ();
        }
        return;

    default: return;
    }
}
void TestConnectionScene::SendChatMessage (const char *text)
{
    int l = strlen (text),
        len = l + 2;

    if (l > 0) // check for empty strings
    {
        Uint8 *data = new Uint8 [len];
        data [0] = NETSIG_CHATMESSAGE;
        strcpy ((char*)(data + 1), text);
        pClient->SendToServer (data, len);
        delete [] data;
    }
}
void TestConnectionScene::OnServerMessage (const Uint8 *data, int len)
{
    ConnectedScene::OnServerMessage(data, len);

    Uint8 signature = data[0];
    data++; len--;
    if( signature==NETSIG_USERSTATE && len==(sizeof(UserState)+USERNAME_MAXLENGTH) )
    {
        const char* username = (const char*)data; data += USERNAME_MAXLENGTH;
        UserState* state = (UserState*) data;

        OnUserState(username,state);
    }
    else if(signature == NETSIG_ADDPLAYER &&
        len == (USERNAME_MAXLENGTH + sizeof(UserParams) + sizeof(UserState)))
    {
        const char* username = (const char*)data; data += USERNAME_MAXLENGTH;
        UserParams* params = (UserParams*) data; data += sizeof(UserParams);
        UserState* state = (UserState*) data;

        OnAddedUser(username,params,state);
    }
    else if(signature == NETSIG_DELPLAYER)
    {
        OnForgetUser((const char*)data);
    }
    else if (signature == NETSIG_CHATMESSAGE && len == sizeof (ChatEntry))
    {
        OnChatMessage ((ChatEntry *)data);
    }
}
void TestConnectionScene::OnChatMessage (const ChatEntry *entry)
{
    char s [256];

    sprintf (s, "[%s]: %s", entry->username, entry->message);

    if (chat_text.size () > 0)
        chat_text += '\n';
    chat_text += s;

    bool max_down = pChatScroll->GetBarY () >= pChatScroll->GetMaxBarY ();

    pChatScroll->SetText (chat_text.c_str());
    pChatScroll->ResetAlpha ();
    if (max_down)
    {
        pChatScroll->SetScroll (pChatScroll->GetMaxBarY ());
    }

    Mix_PlayChannel(-1, pSound, 0);
}
void TestConnectionScene::OnAddedUser (const char* username, UserParams* params, UserState* state)
{
    if (serverStartTicks <= 0) // first contact, set the clock
    {
        serverStartTicks = state->ticks;
        clientStartTicks = SDL_GetTicks();
    }
    state->ticks -= serverStartTicks; // make it relative to the first contact

    if(strcmp(myUsername,username)==0) // It's about me
    {
        myParams = *params;
        return;
    }

    // First find out whether the user is already there...
    RegisteredPlayer *n = NULL;
    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if (strcmp (other->username, username) == 0) // this is the one
        {
            n = other;
        }
    }
    if (n) // already registered
    {
        UpdateUserState (n, state);
    }
    else // not yet known
    {
        n = new RegisteredPlayer;
        others.push_back (n);

        strcpy (n->username, username);
        memcpy (&n->params,  params, sizeof(UserParams));

        // Set both prev and next to this initial state
        memcpy (&n->prev,    state,  sizeof(UserState));
        memcpy (&n->next,    state,  sizeof(UserState));
    }
}
void TestConnectionScene::OnForgetUser (const char* username)
{
    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if(strcmp (other->username, username)==0) // this is the one
        {
            OnForgetUser (other);
            return;
        }
    }
}
void TestConnectionScene::OnForgetUser (RegisteredPlayer* player)
{
    others.remove (player);
    delete player;
}
void TestConnectionScene::OnUserState (const char* username, UserState* state)
{
    if (serverStartTicks <= 0) // first contact, set the clock
    {
        serverStartTicks = state->ticks;
        clientStartTicks = SDL_GetTicks();
    }
    state->ticks -= serverStartTicks; // make it relative to the first contact

    if(strcmp(myUsername,username)==0) //  it's me
    {
        return;
    }

    for (std::list<RegisteredPlayer*>::iterator it = others.begin(); it != others.end(); it++)
    {
        RegisteredPlayer *other = *it;

        if(strcmp(other->username,username)==0) // this is the one
        {
            UpdateUserState(other,state);
            return;
        }
    }

    // otherwise it isn't here yet, ask the server about it:
    int len = 1 + USERNAME_MAXLENGTH;
    Uint8 *data = new Uint8[len];
    data [0] = NETSIG_REQUESTPLAYERINFO;
    strcpy((char*)(data + 1), username);
    pClient->SendToServer(data, len);
    delete [] data;
}
Uint32 TestConnectionScene::GetCurrentTicks() const
{
    return (SDL_GetTicks() - clientStartTicks); // relative to first server contact
}
void TestConnectionScene::UpdateUserState(RegisteredPlayer* player, UserState* n)
{
    Uint32 current = GetCurrentTicks();

    // Put current state at prev
    UserState cur; Predict(&cur,&player->prev,&player->next);
    cur.ticks = current;
    memcpy (&player->prev, &cur, sizeof(UserState));

    // put new state at next
    n->ticks = current + 100;
    memcpy (&player->next, n, sizeof(UserState));
}
void TestConnectionScene::Predict (UserState* out, const UserState* prev, const UserState* next) const
{
    Uint32 current = GetCurrentTicks();

    if (current < next->ticks && next->ticks > prev->ticks)
    {
        // interpolate
        Uint32 ticksRange = next->ticks - prev->ticks;
        float d = float (current - prev->ticks) / ticksRange,
              invd = 1.0f - d;

        out->pos = invd * prev->pos + d * next->pos;
    }
    else
    {
        out->pos = next->pos;
    }
}
void TestConnectionScene::OnConnectionLoss ()
{
    SDL_SetWindowTitle (pClient->GetMainWindow (), "client");

    loginScene ->SetErrorMessage ("Disconnected from the Server");
    pClient -> SwitchScene (loginScene);
    loginScene -> SetMenuEnabled (true);
}
void TestConnectionScene::OnLogout ()
{
    SDL_SetWindowTitle (pClient->GetMainWindow (), "client");

    pClient -> SwitchScene (loginScene);
    loginScene -> SetMenuEnabled (true);
}
