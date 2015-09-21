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


#ifndef SERVER_H
#define SERVER_H

#include <SDL2/SDL_net.h>
#include <openssl/rsa.h>
#include <string>
#include <list>

#include "../geo2d.h"
#include "../account.h"

#define PACKET_MAXSIZE 512
#define MAX_CHAT_LENGTH 100 // must fit inside packet

/*
    A netsig byte is usually placed at the beginning
    of the package data. It tells the server or client
    what data will follow after it.

    In some cases, the netsig byte alone is enough
    information already.
 */
#define NETSIG_PINGSERVER           0x01
#define NETSIG_RSAENCRYPTED         0x02
#define NETSIG_PINGCLIENT           0x03

#define NETSIG_RSAPUBLICKEY         0x08
#define NETSIG_LOGINREQUEST         0x09
#define NETSIG_AUTHENTICATE         0x10
#define NETSIG_LOGINSUCCESS         0x11
#define NETSIG_AUTHENTICATIONERROR  0x12
#define NETSIG_SERVERFULL           0x13
#define NETSIG_ALREADYLOGGEDIN      0x14
#define NETSIG_LOGOUT               0x15

#define NETSIG_REQUESTPLAYERINFO    0x23
#define NETSIG_DELPLAYER            0x22
#define NETSIG_ADDPLAYER            0x21
#define NETSIG_USERSTATE            0x20
#define NETSIG_CHATMESSAGE          0x21

#define COMMAND_MAXLENGTH 256

#define SERVER_RSA_PADDING RSA_PKCS1_PADDING
inline int maxFLEN (RSA* rsa) { return (RSA_size(rsa) - 11); }

#define CONNECTION_TIMEOUT 10.0f // seconds

struct LoginParams
{
    char username [USERNAME_MAXLENGTH],
         password [PASSWORD_MAXLENGTH];
};
struct UserParams
{
    int hue; // color
};
struct UserState
{
    vec2 pos;
    Uint32 ticks;
};
struct ChatEntry
{
    char username [USERNAME_MAXLENGTH], // who said it?
         message [MAX_CHAT_LENGTH]; // what was said?
};

class Server
{
private:
    enum MessageType {INFO, ERROR};

    struct User // created after login, identified by IP-adress
    {
        IPaddress address;
        char accountName[USERNAME_MAXLENGTH]; // empty if not authenticated
        Uint32 ticksSinceLastContact;
        bool pinging;
        RSA* key; // For password encryption/decryption.

        UserState state;
        UserParams params;
    };
    typedef User* UserP;
    UserP* users;
    Uint64 maxUsers;
    static bool LoggedIn (User*);

    UserP AddUser ();
    UserP GetUser (const IPaddress& address);
    UserP GetUser (const char* accountName);
    void DelUser (const Uint64 i);
    void DelUser (UserP user);

    std::list <ChatEntry> chat_history;

    std::string settingsPath,
                accountsPath;

    char command [COMMAND_MAXLENGTH];

    UDPpacket *in, *out,
                **udpPackets;
    int port;
    UDPsocket socket;

    bool done; // if true, loopThread ends
    SDL_Thread *loopThread;
    static int LoopThreadFunc (void* server);

    void Update (Uint32 ticks);

    void PrintUsers () const;
    void PrintChatHistory () const;

    void TellAboutLogout(User* to, const char* loggedOutUsername);

    void OnRequest (const IPaddress& clientAddress, Uint8*data, int len);
    void OnRSAEncrypted (const IPaddress& clientAddress, Uint8*data, int len);
    void OnLoginRequest (const IPaddress& clientAddress);
    void OnAuthenticate (const IPaddress& clientAddress, LoginParams* params);
    void OnLogout (User* user);

    bool SendToClient (const IPaddress& clientAddress,const Uint8*data, int len);

    void OnPlayerRemove (User* user);

    void TellUserAboutUser (User* to, User* about);

    void OnChatMessage (const User*, const char*);
    void OnStateSet (User* user,UserState* newState);
    void SendUserStateToAll (User* user);

    void SendToAll (const Uint8*, const int len);

    void OnMessage (MessageType, const char *format, ...);

public:

    Server();
    ~Server();

    bool Init();
    void CleanUp();
};

#endif // SERVER_H
