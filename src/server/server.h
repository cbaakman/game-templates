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
#define MAX_CHAT_LENGTH 100 // must fit inside PACKET_MAXSIZE

/*
    A netsig byte is usually placed at the beginning
    of the package data. It tells the server or client
    what data will follow after it.

    In some cases, the netsig byte alone is enough
    information already.
 */
#define NETSIG_PINGSERVER           0x01
#define NETSIG_PINGCLIENT           0x03

#define NETSIG_INTERNALERROR        0x07
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

struct LoginParams // must fit inside PACKET_MAXSIZE
{
    char username [USERNAME_MAXLENGTH],
         password [PASSWORD_MAXLENGTH];
    int udp_port;
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
struct ChatEntry // must fit inside PACKET_MAXSIZE
{
    char username [USERNAME_MAXLENGTH], // who said it?
         message [MAX_CHAT_LENGTH]; // what was said?
};

#define RANDSTOCK_SIZE 25

class Server
{
private:
    enum MessageType {SERVER_MSG_INFO, SERVER_MSG_ERROR};

    // Used for random number generation from within any thread:
    SDL_mutex *pRandMutex;
    unsigned int rseed;
    unsigned int GetNextRand (void);

    struct User // created after login, identified by IP-adress
    {
        Uint32 ticksSinceLastContact;
        bool pinging;

        IPaddress address;
        char accountName [USERNAME_MAXLENGTH]; // empty if not authenticated

        UserState state;
        UserParams params;

        User (const IPaddress *, const char *accountName, const UserParams *);
    };

    SDL_mutex *pUsersMutex; // must lock when accessing user list
    typedef User* UserP;
    std::list <UserP> users;
    Uint64 maxUsers;

    bool IsServerFull (void);
    bool AddUser (UserP user);
    UserP GetUser (const IPaddress *address);
    UserP GetUser (const char* accountName);
    void DelUser (UserP user);

    std::list <ChatEntry> chat_history;

    std::string settingsPath,
                accountsPath;

    char command [COMMAND_MAXLENGTH];

    UDPpacket *in, *out,
                **udpPackets;
    int port;
    UDPsocket udp_socket;
    TCPsocket tcp_socket;
    IPaddress mAddress;

    bool done; // if true, loopThread ends
    SDL_Thread *loopThread;
    int LoopThreadFunc (void);

    void Update (Uint32 ticks);

    void PrintUsers () const;
    void PrintChatHistory () const;

    void TellAboutLogout (UserP to, const char *loggedOutUsername);

    void OnUDPPackage (const IPaddress& clientAddress, Uint8*data, int len);
    void OnTCPConnection (TCPsocket clientSocket);
    void OnLogin (TCPsocket, const IPaddress *pClientIP);
    void OnLogout (User* user);

    bool SendToClient (const IPaddress& clientAddress, const Uint8 *data, int len);

    void OnPlayerRemove (UserP user);

    void TellUserAboutUser (UserP to, const UserP about);

    void OnChatMessage (const UserP, const char *);
    void OnStateSet (UserP user, const UserState *newState);
    void SendUserStateToAll (const UserP user);

    void SendToAll (const Uint8 *, const int len);

    void OnMessage (MessageType, const char *format, ...);

public:

    Server ();
    ~Server ();

    bool Init ();
    void CleanUp ();
};

#endif // SERVER_H
