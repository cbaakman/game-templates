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


#ifndef LOGIN_H
#define LOGIN_H

#include <openssl/rsa.h>

#include "client.h"
#include "gui.h"
#include "textscroll.h"
#include "connection.h"
#include "../texture.h"
#include "../server/server.h"
#include "../account.h"

#include <SDL2/SDL_mixer.h>

/*
 * This Scene allows a user to log in, using a username and password combination, created using the manager application.
 * The server application must be running for a successful login.
 * If login is successful, the user will get to see the TestConnectionScene.
 */
class LoginScene : public Client::Scene
{
private:
    void Login();

    // variables used during login:
    bool logging;
    float loginTime;

    // Set this string to show an error message on screen
    char errorMessage [100];

    /*
     * Holds the menu items and renders the default cursor.
     */
    class LoginMenu : public Menu
    {
    private:
        // keeps track of time, used for rotating cursor
        float t;

        LoginScene *parent;

        Texture *pCursorTex;
    public:
        LoginMenu (Client*, LoginScene*, Texture *pCursorTex);

        void Update (const float dt);
        void RenderDefaultCursor(int mX, int mY);

    friend class LoginScene;

    } *pMenu;

    /*
     * Insert username here.
     */
    class UsernameBox : public TextInputBox
    {
    private:
        LoginScene* parent;

        GLfloat w,h,fy;

        void RenderCursor(int mX, int mY);
    public:
        void Render();

        MenuObject* NextInLine() const;
        void OnAddChar(char c);
        void OnDelChar(char c);
        void OnBlock();

        UsernameBox(GLfloat x, GLfloat y, Font*, LoginScene* parent);

    friend class LoginScene;

    } *usernameBox;

    /*
     * Insert password here.
     */
    class PasswordBox : public TextInputBox
    {
    private:
        LoginScene* parent;

        GLfloat w,h,fy;

        void RenderCursor(int mX, int mY) ;
    public:
        void Render();

        MenuObject* NextInLine() const;
        void OnAddChar(char c);
        void OnDelChar(char c);
        void OnBlock();

        PasswordBox(GLfloat x, GLfloat y, Font*, LoginScene* parent);

    friend class LoginScene;

    } *passwordBox;

    /*
     * Initializes login when clicked
     */
    class LoginButton : public MenuObject
    {
    private:
        LoginScene* parent;

        GLfloat x,y;
        Font* pFont;
    public:
        LoginButton (GLfloat x, GLfloat y, Font*, LoginScene* parent);
        bool MouseOver (GLfloat x, GLfloat y) const;
        void OnMouseClick (const SDL_MouseButtonEvent *event);
        void Render ();
        void RenderCursor (int mX, int mY);

    friend class LoginScene;

    } *loginButton;


    // Resources
    Mix_Chunk *pSound;
    Texture cursorTex, bgTex, buttonTex;
    Font font, small_font;


    // scene to log into
    ConnectedScene* nextScene;


    // keeps track of time, used for moving background
    float t;

    void OnKeyPress(const SDL_KeyboardEvent *event);

    void OnServerMessage(const Uint8*data, int len);

    void SendEncryptedAuthentication (RSA* publicKey);
    void OnLogin (UserParams* params, UserState* state);

    void RenderCursor (int mX, int mY);
public:
    LoginScene (Client *);
    ~LoginScene ();

    void OnEvent (const SDL_Event *event);

    bool Init (void);

    void Update (const float dt);
    void Render ();

    void OnConnectionLoss();
    void OnLogout();

    const char* GetUsername() const;

    void SetMenuEnabled (bool);

    void SetErrorMessage (const char *msg) { strcpy (errorMessage, msg); }
};

/*
 * Data structure used by TestConnectionScene to keep track of logged in users.
 */
struct RegisteredPlayer
{
    char username [USERNAME_MAXLENGTH];
    UserParams params;
    UserState prev, next;
};

/*
 * This scene allows two logged in users to see each other's mouse cursors and chat messages.
 * Press Enter to chat and use the mouse wheel to show chat history.
 * The server application must run continuously for this scene to stay on.
 */
class TestConnectionScene : public ConnectedScene
{
private:
    /*
     * Allows to enter a chat message.
     */
    class ChatInput : public TextInputBox
    {
    public:

        ChatInput (const GLfloat x, const GLfloat y, const Font*, const int maxTextLength);

        void OnFocusLose ();

        void Render ();

    friend class TestConnectionScene;

    } *pChatInput;

    /*
     * Shows the chat history.
     */
    class ChatScroll : public TextScroll
    {
    private:
        GLfloat alpha;

        TestConnectionScene *parent;

    public:
        ChatScroll (TestConnectionScene *parent, const Font *pFont, const float screen_height, const float screen_width);

        void ResetAlpha ();

        void Update (const float dt);
        void OnMouseWheel (const SDL_MouseWheelEvent *);

        void Render ();

    friend class TestConnectionScene;

    } *pChatScroll;


    Mix_Chunk *pSound;
    Texture *tex, scrollTex;
    const Font* fnt;
    float t, posPer;

    Uint32  serverStartTicks, // server time of first contact
            clientStartTicks; // client time of first contact

    LoginScene *loginScene;

    std::string chat_text;

    char myUsername [USERNAME_MAXLENGTH];
    UserState me;
    UserParams myParams;
    std::list <RegisteredPlayer*> others;

    void OnKeyPress (const SDL_KeyboardEvent *event);

    void SendState();
    void SendChatMessage (const char *);

    // This function predicts a user state by interpolation. Nessesary when ping is high
    void Predict (UserState* out, const UserState* prev, const UserState* next) const;


    void UpdateUserState (RegisteredPlayer* player, UserState* _new);

    void OnChatMessage (const ChatEntry *);
    void OnAddedUser (const char* username, UserParams* params, UserState* state);
    void OnForgetUser (const char* username);
    void OnForgetUser (RegisteredPlayer* player);
    void OnUserState (const char* username, UserState* _new);
    Uint32 GetCurrentTicks() const;

public:

    TestConnectionScene(Client*, LoginScene* s, Texture *cursorTex, Mix_Chunk *, const Font*, const Font *small);
    ~TestConnectionScene();

    void OnLogin(const char* username, UserParams*, UserState*);

    void Update (const float dt);
    void Render ();

    void OnEvent (const SDL_Event *);

    void OnServerMessage (const Uint8 *data, const int len);

    void OnConnectionLoss ();

    bool Init (void);
};

#endif // LOGIN_H
