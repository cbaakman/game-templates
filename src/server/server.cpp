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


#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <string.h>
#include <errno.h>
#include <ctime>

#include <openssl/err.h>

#include "server.h"

#include "../io.h"
#include "../err.h"
#include "../ini.h"
#include "../ip.h"
#include "../str.h"
#include "../thread.h"
#include "../http.h"

#define PORT_SETTING "port"
#define MAXLOGIN_SETTING "max-login"
#define ACCOUNTSDIR_SETTING "accounts-dir"

#define ACCOUNT_DIR "accounts"
#define CONNECTION_PINGPERIOD 1000 // ticks

#define PROCESS_TAG "server"

const int CONNECTION_TIMEOUT_TICKS = CONNECTION_TIMEOUT * 1000;

Server::User::User (const IPaddress *pAddr, const char *_accountName, const UserParams *pParams)
{
    strcpy (accountName, _accountName);
    ticksSinceLastContact = 0;
    memcpy (&address, pAddr, sizeof (IPaddress));

    memcpy (&params, pParams, sizeof (UserParams));
    state.pos.x = -1000;
    state.pos.y = -1000;

    ticksSinceLastContact = 0;
}

#define RSA_ERRBUF_SIZE 256

int RecieveEncrypted (TCPsocket clientSocket,
                      void *decrypted_data, const int decrypted_maxlen)
{
    int n_sent,
        n_recieved,
        keySize, keyPackageSize,
        decryptedSize,
        maxflen;
    unsigned char *public_key_package, *public_key,
                  encrypted [PACKET_MAXSIZE];
    char errbuf [RSA_ERRBUF_SIZE];

    RSA* keyPair;
    BIGNUM *bn;
    bool success;
    Uint8 signal;


    bn = BN_new ();
    BN_set_word (bn, 65537);
    keyPair = RSA_new ();
    success = RSA_generate_key_ex (keyPair, 1024, bn, NULL) &&
              RSA_check_key (keyPair);

    BN_free (bn);

    if (!success)
    {
        ERR_error_string_n (ERR_get_error (), errbuf, RSA_ERRBUF_SIZE);
        SetError ("RSA key generation failed: %s", errbuf);

        RSA_free (keyPair);

        signal = NETSIG_INTERNALERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            fprintf (stderr, "Could not send internal error to user: %s",
                     SDLNet_GetError ());
        }

        return -1;
    }

    // Copy public key to byte array and send to user:
    keySize = i2d_RSAPublicKey (keyPair, NULL);
    keyPackageSize = keySize + 1;
    public_key_package = new unsigned char [keyPackageSize];
    public_key_package [0] = NETSIG_RSAPUBLICKEY;
    public_key = public_key_package + 1;
    i2d_RSAPublicKey (keyPair, &public_key);

    n_sent = SDLNet_TCP_Send (clientSocket, public_key_package, keyPackageSize);
    delete [] public_key_package;

    if (n_sent != keyPackageSize)
    {
        SetError ("Error sending key: %s", SDLNet_GetError());
        RSA_free (keyPair);
        return -1;
    }

    // Receive encrypted data from user:
    n_recieved = SDLNet_TCP_Recv (clientSocket, encrypted, PACKET_MAXSIZE);
    if (n_recieved <= 0)
    {
        if (n_recieved < 0)

            SetError ("Error recieving encrypted login: %s", SDLNet_GetError ());
        else
            SetError ("Connection unexpectedly closed while recieving encrypted login");

        RSA_free (keyPair);
        return -1;
    }

    // Decrypt login parameters:
    maxflen = maxFLEN (keyPair);
    if (maxflen > decrypted_maxlen)
    {
        SetError ("Allocated decryption buffer is too small");
        return -1;
    }

    decryptedSize = RSA_private_decrypt (n_recieved, encrypted,
                                         (unsigned char *)decrypted_data,
                                         keyPair, SERVER_RSA_PADDING);
    RSA_free (keyPair);

    if (decryptedSize < 0)
    {
        ERR_error_string_n (ERR_get_error (), errbuf, RSA_ERRBUF_SIZE);
        SetError ("Error decrypting: %s", errbuf);
    }

    return decryptedSize;
}

/**
 * This runs in a temporary thread.
 */
void Server::OnLogin (TCPsocket clientSocket, const IPaddress *pClientIP)
{
    int decryptedSize, n_sent, len;
    unsigned char decrypted [PACKET_MAXSIZE];
    Uint8 signal, *data;

    // If the server is full at this point, don't bother.
    if (IsServerFull ())
    {
        signal = NETSIG_SERVERFULL;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            Message (SERVER_MSG_ERROR, "WARNING, could not send server full error to user: %s",
                     SDLNet_GetError ());
        }
        return;
    }

    decryptedSize = RecieveEncrypted (clientSocket, decrypted, PACKET_MAXSIZE);
    if (decryptedSize <= 0)
    {
        char ip [100];
        ip2String (*pClientIP, ip);

        Message (SERVER_MSG_ERROR,
                 "Error recieving encrypted login from %s: %s",
                 ip, GetError ());
        return;
    }
    else if (decryptedSize != sizeof (LoginParams))
    {
        Message (SERVER_MSG_ERROR,
                 "error recieving login parameters, wrong data size recieved");

        signal = NETSIG_AUTHENTICATIONERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            Message (SERVER_MSG_ERROR,
                     "WARNING, could not send authentication error to user: %s",
                     SDLNet_GetError ());
        }

        return;
    }

    // Get decrypted username, password, etc.
    const LoginParams *pParams = (const LoginParams *)decrypted;

    if (GetUser (pParams->username)) // this user is already logged in
    {
        Uint8 signal = NETSIG_ALREADYLOGGEDIN;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            Message (SERVER_MSG_ERROR,
                     "WARNING, could not send already logged in error to user: %s", SDLNet_GetError ());
        }
    }
    else if (authenticate (accountsPath.c_str(), pParams->username, pParams->password))
    {
        // To keep it thread safe, we must copy these values before the user enters the list..
        IPaddress clientAddress;
        clientAddress.host = pClientIP->host;
        clientAddress.port = pParams->udp_port;

        UserParams userParams;
        userParams.hue = GetNextRand () % 360; // give the user a random color

        UserP pUser = new User (&clientAddress, pParams->username, &userParams);

        UserState startState = pUser->state;

        // Try to add user to the list:
        if (AddUser (pUser))
        {
            // Send client the message that login succeeded,
            // along with the user's first state and parameters:
            len = 1 + sizeof (UserParams) + sizeof (UserState);
            data = new Uint8 [len];
            data [0] = NETSIG_LOGINSUCCESS;
            memcpy (data + 1, &userParams, sizeof (UserParams));
            memcpy (data + 1 + sizeof (UserParams), &startState, sizeof (UserState));
            n_sent = SDLNet_TCP_Send (clientSocket, data, len);
            delete data;

            if (n_sent != len)
            {
                Message (SERVER_MSG_ERROR, "WARNING, could not send login conformation to user: %s",
                         SDLNet_GetError ());
            }

            Message (SERVER_MSG_INFO, "%s just logged in", pUser->accountName);

            // Tell other users about this new user:
            if (SDL_LockMutex (pUsersMutex) == 0)
            {
                for (const UserP pOtherUser : users)
                {
                    if(pOtherUser != pUser)
                    {
                        TellUserAboutUser (pOtherUser, pUser);
                        TellUserAboutUser (pUser, pOtherUser);
                    }
                }

                SDL_UnlockMutex (pUsersMutex);
            }
            else
                Message (SERVER_MSG_ERROR, "Error telling users about new user, could not lock mutex: %s",
                         SDL_GetError ());
        }
        else // AddUser failed, server full
        {
            signal = NETSIG_SERVERFULL;
            if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
            {
                Message (SERVER_MSG_ERROR, "WARNING, could not send server full error to user: %s",
                         SDLNet_GetError ());
            }
        }
    }
    else // authentication failure
    {
        signal = NETSIG_AUTHENTICATIONERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            Message (SERVER_MSG_ERROR, "WARNING, could not send authentication error to user: %s",
                     SDLNet_GetError ());
        }
    }
}

void STDAppender::Message (MessageType type, const char *format, va_list args)
{
    FILE *stream;
    switch (type)
    {
    case SERVER_MSG_DEBUG:
    case SERVER_MSG_INFO:
        stream = stdout;
    break;
    case SERVER_MSG_ERROR:
        stream = stderr;
    break;
    }

    vfprintf (stream, format, args);
    fprintf (stream, "\n");
}
FileLogAppender::FileLogAppender (const char *log_path)
{
    strcpy (path, log_path);

    // Empty the log file, before appending:
    FILE *pFile = fopen (path, "w");
    fclose (pFile);
}
FileLogAppender::FileLogAppender (const std::string &log_path)
    : FileLogAppender (log_path.c_str ()) {}
void FileLogAppender::Message (MessageType type, const char *format, va_list args)
{
    time_t rawtime;
    struct tm * timeinfo;
    char timebuf [100];

    // Append to log file
    FILE *pFile = fopen (path, "a");

    time (&rawtime);
    timeinfo = localtime (&rawtime);

    strftime (timebuf, 100,"[%d-%m-%Y %H:%M:%S] ",timeinfo);
    fputs (timebuf, pFile);

    switch (type)
    {
    case SERVER_MSG_DEBUG:

        fprintf (pFile, "DEBUG: ");
    break;
    case SERVER_MSG_INFO:

        fprintf (pFile, "INFO: ");
    break;
    case SERVER_MSG_ERROR:

        fprintf (pFile, "ERROR: ");
    break;
    }

    vfprintf (pFile, format, args);
    fprintf (pFile, "\n");

    fclose (pFile);
}
void Server::Message (MessageType type, const char *format, ...)
{
    #ifndef DEBUG
        if (type == SERVER_MSG_DEBUG)
            return;
    #endif

    if (SDL_LockMutex (pMessageMutex) != 0)
    {
        fprintf (stderr, "Cannot send message, unable to lock mutex: %s",
                 SDL_GetError ());
        return;
    }

    va_list args;
    va_start (args, format);

    pMessageAppender->Message (type, format, args);

    va_end (args);

    SDL_UnlockMutex (pMessageMutex);
}
Server::Server() :
    pMessageAppender(new STDAppender),
    pUsersMutex(NULL),
    maxUsers(0),
    in(NULL), out(NULL),
    udpPackets(NULL),
    tcp_socket(NULL), udp_socket(NULL)
{
    pUsersMutex = SDL_CreateMutex ();
    pRandMutex = SDL_CreateMutex ();
    pChatMutex = SDL_CreateMutex ();
    pMessageMutex = SDL_CreateMutex ();

    rseed = time (NULL);
}
Server::~Server()
{
    // Clean up anything that was allocated/linked:
    NetCleanUp ();
    ResourceCleanUp ();

    // These must be destroyed last!
    SDL_DestroyMutex (pUsersMutex);
    SDL_DestroyMutex (pRandMutex);
    SDL_DestroyMutex (pChatMutex);
    SDL_DestroyMutex (pMessageMutex);

    delete pMessageAppender;
}
bool Server::Configure (void)
{
#ifdef CONFDIR
    settingsPath = std::string (CONFDIR) + PATH_SEPARATOR + "server.ini";
#else
    settingsPath = std::string (SDL_GetBasePath ()) + "settings.ini";
#endif

    if (!LoadSettingString (settingsPath, ACCOUNTSDIR_SETTING, accountsPath))
        accountsPath = std::string (SDL_GetBasePath ()) + ACCOUNT_DIR;

    #ifdef IMPL_UNIX_DEAMON
        pidPath = "/var/run/server.pid";
    #endif

    // load the maxUsers setting from the config
    maxUsers = LoadSetting (settingsPath.c_str(), MAXLOGIN_SETTING);
    if (maxUsers <= 0)
    {
        Message (SERVER_MSG_ERROR, "Max Login not found in %s", settingsPath.c_str());
        return false;
    }

    // Load the port number from config
    port = LoadSetting (settingsPath.c_str(), PORT_SETTING);
    if (port <= 0)
    {
        Message (SERVER_MSG_ERROR, "Port not set in %s", settingsPath.c_str());
        return false;
    }

    return true;
}
bool Server::NetInit (void)
{
    if (SDLNet_Init() < 0)
    {
        Message (SERVER_MSG_ERROR, "SDLNet_Init: %s", SDLNet_GetError());
        return false;
    }

    // Open a socket and listen at the configured port:
    if (!(udp_socket = SDLNet_UDP_Open (port)))
    {
        Message (SERVER_MSG_ERROR, "SDLNet_UDP_Open: %s", SDLNet_GetError());
        return false;
    }

    if (SDLNet_ResolveHost (&mAddress, NULL, port) < 0)
    {
        Message (SERVER_MSG_ERROR, "SDLNet_ResolveHost: %s", SDLNet_GetError());
        return false;
    }

    // Open a socket and listen at the configured port:
    if (!(tcp_socket = SDLNet_TCP_Open (&mAddress)))
    {
        Message (SERVER_MSG_ERROR, "SDLNet_TCP_Open: %s", SDLNet_GetError());
        return false;
    }

    // Allocate packets of the size we need, one for incoming, one for outgoing
    if (!(udpPackets = SDLNet_AllocPacketV (2, PACKET_MAXSIZE)))
    {
        Message (SERVER_MSG_ERROR, "SDLNet_AllocPacketV: %s", SDLNet_GetError());
        return false;
    }
    in = udpPackets [0];
    out = udpPackets [1];

    return true;
}

/**
 * Can be used from any thread.
 */
unsigned int Server::GetNextRand (void)
{
    if (SDL_LockMutex (pRandMutex) == 0)
    {
        rseed = 9185162 * rseed + 2471575;

        unsigned int r = rseed % RAND_MAX;

        SDL_UnlockMutex (pRandMutex);
        return r;
    }
    else
    {
        Message (SERVER_MSG_ERROR, "Warning: could not lock rand mutex: %s",
                 SDL_GetError ());
        return 0;
    }
}
void Server::NetCleanUp()
{
    if(udpPackets)
    {
        SDLNet_FreePacketV (udpPackets);
        udpPackets = NULL;
        in = out = NULL;
    }
    if(udp_socket)
    {
        SDLNet_UDP_Close (udp_socket);
        udp_socket = NULL;
    }
    if (tcp_socket)
    {
        SDLNet_TCP_Close (tcp_socket);
        tcp_socket = NULL;
    }

    SDLNet_Quit();

    if (SDL_LockMutex (pUsersMutex) == 0)
    {
        for (UserP pUser : users)
            delete pUser;
        users.clear ();

        SDL_UnlockMutex (pUsersMutex);
    }
    else
        Message (SERVER_MSG_ERROR, "Error deleting users, could not lock mutex!");
}
bool RawResourceLoad (const std::string &archive, const std::string &filename, std::string &out)
{
    SDL_RWops *f;
    bool success;

    f = SDL_RWFromZipArchive (archive.c_str (), filename.c_str ());
    if (!f) // file or archive missing
    {
        SetError (SDL_GetError ());
        return false;
    }

    success = ReadAll (f, out);
    f->close (f);

    if (!success)
    {
        SetError ("failed to load %s: %s", filename.c_str (), GetError ());
        return false;
    }

    return true;
}
bool Server::ResourceInit (void)
{
#ifdef RESDIR
    std::string archive = std::string (RESDIR) + PATH_SEPARATOR + "server.zip";
#else
    std::string archive = std::string (SDL_GetBasePath ()) + "server.zip";
#endif

    if (!(RawResourceLoad (archive, "client-icon.ico", icon_bytes)
        && RawResourceLoad (archive, "base.html", base_html)
        && RawResourceLoad (archive, "users.html", users_html)
        && RawResourceLoad (archive, "chat.html", chat_html)))
    {
        Message (SERVER_MSG_ERROR, GetError ());
        return false;
    }

    return true;
}
void Server::ResourceCleanUp (void)
{
}
bool Server::IsServerFull (void)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Error checking server full, could not lock mutex: %s",
                 SDL_GetError ());
        return true;
    }

    bool b = users.size () >= maxUsers;

    SDL_UnlockMutex (pUsersMutex);

    return b;
}
bool Server::AddUser (UserP pUser)
{
    if (users.size() < maxUsers)
    {
        if (SDL_LockMutex (pUsersMutex) != 0)
        {
            Message (SERVER_MSG_ERROR, "Error adding user, could not lock mutex: %s",
                     SDL_GetError ());
            return false;
        }

        users.push_back (pUser);

        SDL_UnlockMutex (pUsersMutex);
        return true;
    }

    return false;
}
Server::UserP Server::GetUser (const IPaddress *pAddress)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Error getting user, could not lock mutex: %s",
                 SDL_GetError ());
        return NULL;
    }

    for (UserP pUser : users)
    {
        if (pUser->address.host == pAddress->host &&
            pUser->address.port == pAddress->port)
        {
            SDL_UnlockMutex (pUsersMutex);
            return pUser;
        }
    }

    SDL_UnlockMutex (pUsersMutex);
    return NULL;
}
Server::UserP Server::GetUser (const char* accountName)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Error getting user, could not lock mutex: %s",
                 SDL_GetError ());
        return NULL;
    }

    for (UserP pUser : users)
    {
        if (strcmp (pUser->accountName, accountName)==0 )
        {
            SDL_UnlockMutex (pUsersMutex);
            return pUser;
        }
    }

    SDL_UnlockMutex (pUsersMutex);
    return NULL;
}
void Server::OnPlayerRemove (Server::UserP pUser)
{
    // Tell other players about this one's removal:
    int len = USERNAME_MAXLENGTH + 1;
    Uint8*data = new Uint8 [USERNAME_MAXLENGTH + 1];
    data [0] = NETSIG_DELPLAYER;
    memcpy (data + 1, pUser->accountName, USERNAME_MAXLENGTH);

    if (SDL_LockMutex (pUsersMutex) == 0)
    {
        // Send the message to all clients:
        for (UserP pOtherUser : users)
        {
            if(pOtherUser != pUser)
            {
                SendToClient (pOtherUser->address, data, len);
            }
        }

        SDL_UnlockMutex (pUsersMutex);
    }
    else
        Message (SERVER_MSG_ERROR, "OnPlayerRemove, error locking mutex: %s", SDL_GetError ());

    delete[] data;
}
void Server::TellAboutLogout (UserP to, const char* loggedOutUsername)
{
    // User 'to' will be told that 'loggedOutUsername' has logged out.

    int len = USERNAME_MAXLENGTH + 1;
    Uint8*data = new Uint8 [USERNAME_MAXLENGTH + 1];
    data[0] = NETSIG_DELPLAYER;
    memcpy (data + 1, loggedOutUsername, USERNAME_MAXLENGTH);

    // Send data package to client about logged out user
    SendToClient (to->address, data, len);
    delete[] data;
}
void Server::TellUserAboutUser (UserP to, const UserP about)
{
    // user 'about' has been added and user 'to' must know

    int len = 1 + USERNAME_MAXLENGTH + sizeof(UserParams) + sizeof(UserState);
    Uint8* data = new Uint8[len];
    data [0] = NETSIG_ADDPLAYER;
    memcpy (data + 1,
           about->accountName,USERNAME_MAXLENGTH);
    memcpy (data + 1 + USERNAME_MAXLENGTH,
           &about->params, sizeof(UserParams));
    memcpy (data + 1 + USERNAME_MAXLENGTH + sizeof(UserParams),
           &about->state, sizeof(UserState));

    // Send data package to client about  user
    SendToClient (to->address, data, len);
    delete [] data;
}
void Server::DelUser (Server::UserP pUser)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Error removing user, could not lock mutex: %s",
                 SDL_GetError ());
        return;
    }

    users.remove (pUser);
    delete pUser;

    SDL_UnlockMutex (pUsersMutex);
}
void Server::Update (Uint32 ticks)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Error updating users, could not lock mutex: %s",
                 SDL_GetError ());
        return;
    }

    std::list<UserP> toRemove;
    for (UserP pUser : users)
    {
        // periodically send pings to every user

        pUser->ticksSinceLastContact += ticks;
        if(!(pUser->pinging) && pUser->ticksSinceLastContact > CONNECTION_PINGPERIOD)
        {
            Uint8 ping = NETSIG_PINGSERVER;
            SendToClient (pUser->address, &ping, 1);
            pUser->pinging = true;
        }
        if (pUser->ticksSinceLastContact > CONNECTION_TIMEOUT_TICKS )
        {
            Message (SERVER_MSG_INFO, "%s timed out", pUser->accountName);

            toRemove.push_back (pUser);
        }
    }
    SDL_UnlockMutex (pUsersMutex);

    for (UserP pUser : toRemove)
    {
        OnPlayerRemove (pUser);
        DelUser (pUser);
    }
}
void Server::OnStateSet (UserP user, const UserState* state)
{
    // Must lock the mutex when altering a user in the list.
    if (SDL_LockMutex (pUsersMutex) == 0)
    {
        for (UserP pUser : users)
        {
            if (pUser == user)
            {
                user->state.ticks = SDL_GetTicks(); // Update to current time
                user->state.pos = state->pos;
            }
        }

        SDL_UnlockMutex (pUsersMutex);
    }
    else
        Message (SERVER_MSG_ERROR, "Error setting user state, could not lock mutex: %s",
                 SDL_GetError ());

    SendUserStateToAll (user);
}
void Server::OnChatMessage (const UserP pUser, const char *msg)
{
    ChatEntry e;
    strcpy (e.username, pUser->accountName);
    strcpy (e.message, msg);

    if (SDL_LockMutex (pChatMutex) == 0)
    {
        chat_history.push_back (e);

        SDL_UnlockMutex (pChatMutex);
    }
    else
        Message (SERVER_MSG_ERROR, "Error adding chat message, could not lock mutex: %s",
                 SDL_GetError ());

    Message (SERVER_MSG_INFO, "%s said: %s", pUser->accountName, msg);

    // Tell everybody about this chat message:

    int len = sizeof (e) + 1;
    Uint8* data = new Uint8[len];
    data [0] = NETSIG_CHATMESSAGE;
    memcpy (data + 1, &e, sizeof (e));

    SendToAll (data, len);

    delete [] data;
}
void Server::SendUserStateToAll (const UserP pUser)
{
    int len = 1 + USERNAME_MAXLENGTH + sizeof(UserState);
    Uint8* data = new Uint8 [len];
    data[0] = NETSIG_USERSTATE;
    memcpy (data + 1, &pUser->accountName, USERNAME_MAXLENGTH);
    memcpy (data + 1 + USERNAME_MAXLENGTH, &(pUser->state), sizeof (UserState));

    SendToAll (data, len);

    delete [] data;
}
void Server::SendToAll (const Uint8 *data, const int len)
{
    if (SDL_LockMutex (pUsersMutex) == 0)
    {
        // Sends data package to all users in the list
        for (UserP pUser : users)
        {
            SendToClient (pUser->address, data, len);
        }

        SDL_UnlockMutex (pUsersMutex);
    }
    else
        Message (SERVER_MSG_ERROR, "WARNING, failed to lock mutex while sending a message to all: %s",
                 SDL_GetError ());
}
void Server::OnLogout (Server::User* user)
{
    // User requested logout, remove and tell other users

    OnPlayerRemove (user);
    DelUser (user);

    Message (SERVER_MSG_INFO, "%s just logged out", user->accountName);
}
void Server::OnUDPPackage (const IPaddress& clientAddress, Uint8 *data, int len)
{
    const UserP user = GetUser (&clientAddress);
    if (user)
    {
        // this is a sign of life, reset ping time
        user->ticksSinceLastContact = 0;
    }
    else // package came from user that was not logged in
        return;

    Uint8 signature = data [0];
    data++; len--;

    // What we do now depends on the signature (first byte)
    switch (signature)
    {
    case NETSIG_LOGOUT:
        OnLogout (user);
    break;
    case NETSIG_PINGCLIENT:
        SendToClient(clientAddress,&signature,1);
    break;
    case NETSIG_PINGSERVER:
        user->pinging=false;
    break;
    case NETSIG_USERSTATE:
        if (len == sizeof(UserState))
        {
            UserState* state=(UserState*)data;
            OnStateSet(user, state);
        }
    break;
    case NETSIG_CHATMESSAGE:
    {
        // make sure it's null terminated and not too long:
        char *msg = (char *)data;
        int i = 0;
        while (msg [i] && i < len && (i + 1) < MAX_CHAT_LENGTH)
            i ++;

        if (msg [i])
            msg [i] = NULL;

        OnChatMessage (user, msg);
    }
    break;
    case NETSIG_REQUESTPLAYERINFO:
        if (len == USERNAME_MAXLENGTH)
        {
            User* other = GetUser ((const char *)data);
            if (other)
            {
                TellUserAboutUser (user, other);
            }
            else // this user doesn't know yet about this other user logging out
            {
                TellAboutLogout (user, (const char *)data);
            }
        }
    default:
        return;
    }
}
#define MAX_RECV 1024
void Server::OnTCPConnection (TCPsocket clientSocket)
{
    if (!clientSocket)
        return;

    IPaddress *pClientIP = SDLNet_TCP_GetPeerAddress (clientSocket);
    if (!pClientIP)
    {
        Message (SERVER_MSG_ERROR, "SDLNet_TCP_GetPeerAddress: %s",
                 SDLNet_GetError ());
        return;
    }
    char ipString [100];
    ip2String (*pClientIP, ipString);

    Message (SERVER_MSG_DEBUG,
             "tcp connection established with client at socket 0x%x with address %s",
             clientSocket, ipString);

    Uint8 signal;
    int nrecv;
    if ((nrecv = SDLNet_TCP_Recv (clientSocket, &signal, 1)) == 1)
    {
        if (signal == NETSIG_LOGINREQUEST)

            OnLogin (clientSocket, pClientIP);

        else if (signal == 'G') // is it GET ?
        {
            // Collect the rest of the bytes..
            char data [MAX_RECV];
            data [0] = signal;
            int len = SDLNet_TCP_Recv (clientSocket, data + 1, MAX_RECV - 1);

            std::string method,
                        path,
                        host;

            if (ParseHttpRequest (data, len, method, path, host)
                && method == "GET")

                OnHttpGet (clientSocket, host, path);
        }
    }
    else if (nrecv == 0)

        Message (SERVER_MSG_ERROR,
                 "Newly opened tcp socket %u at %s was unexpectedly closed by the peer",
                 clientSocket, ipString);

    else

        Message (SERVER_MSG_ERROR,
                 "Recieved no data from newly opened tcp socket %u at %s: %s",
                 clientSocket, ipString, SDLNet_GetError ());

    SDLNet_TCP_Close (clientSocket);
}
void ReplaceIn (std::string &in, const std:: string &what, const std::string &by)
{
    size_t pos = in.length ();
    while (pos > 0)
    {
        pos = in.rfind (what, pos - 1);
        if (pos == std::string::npos)
            break;

        in.replace (pos, what.length (), by);
    }
}
void Server::OnHttpGet (TCPsocket clientSocket, const std::string &host, const std::string &path)
{
    std::string response = "",
                url = std::string ("http://") + host + path;

    if (path == "/")
    {
        std::string html = base_html;

        ReplaceIn (html, "{{ title }}", "SERVER");
        ReplaceIn (html, "{{ content }}", users_html + chat_html);

        response = HTTPResponseOK (html.c_str (), html.size (), "text/html; charset=UTF-8");
    }
    else if (path == "/favicon.ico")
    {
        response = HTTPResponseOK (icon_bytes.c_str (), icon_bytes.size (), "image/x-icon");
    }
    else if (path == "/users")

        response = HTTPResponseFound ((url + "/").c_str ());

    else if (path == "/users/")
    {
        std::string json;
        UserListJSON (json);

        response = HTTPResponseOK (json.c_str (), json.size (), "text/json; charset=UTF-8");
    }
    else if (path == "/chat")

        response = HTTPResponseFound ((url + "/").c_str ());

    else if (path == "/chat/")
    {
        std::string json;
        ChatHistoryJSON (json);

        response = HTTPResponseOK (json.c_str (), json.size (), "text/json; charset=UTF-8");
    }
    else
        response = HTTPResponseNotFound ();

    if (SDLNet_TCP_Send (clientSocket, response.c_str (), response.size ())
        != response.size ())

        Message (SERVER_MSG_ERROR, "SDLNet_TCP_Send: %s", SDLNet_GetError());
}
bool Server::SendToClient (const IPaddress& clientAddress, const Uint8*data, int len)
{
    out->address.host = clientAddress.host;
    out->address.port = clientAddress.port;

    // If package is to big, cut it
    if (len > (out->maxlen))
        len = out->maxlen;

    memcpy (out->data, data, len);
    out->len = len;

    SDLNet_UDP_Send (udp_socket, -1, out);

    if (out->status != len) // the wrong number has been returned
        return false;

    return true;
}
void Server::ChatHistoryJSON (std::string &json)
{
    char s [100];
    bool comma = false;

    json = "";

    if (SDL_LockMutex (pChatMutex) != 0)
    {
        Message (SERVER_MSG_ERROR,
                 "Cannot output chat history, error locking mutex: %s",
                 SDL_GetError ());
        return;
    }

    json += "[";

    for (const ChatEntry &entry : chat_history)
    {
        if (comma)
            json += ",";
        comma = true;

        std::string message (entry.message);
        ReplaceIn (message, "\\", "\\\\");

        sprintf (s, "{\"user\":\"%s\", \"message\":\"%s\"}",
                 entry.username, message.c_str ());

        json += s;
    }

    json += "]";

    SDL_UnlockMutex (pChatMutex);
}
void Server::UserListJSON (std::string &json)
{
    bool comma = false;
    char ipStr [IP_STRINGLENGTH],
         s [100];

    json = "";

    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        Message (SERVER_MSG_ERROR, "Cannot output users, couldn't lock mutex: %s",
                 SDL_GetError ());

        return;
    }

    json += "[";

    for (UserP pUser : users)
    {
        if (comma)
            json += ",";
        comma = true;

        ip2String (pUser->address, ipStr);
        sprintf (s, "{\"ip\":\"%s\", \"name\":\"%s\", \"contact\":%u}",
                 ipStr, pUser->accountName, pUser->ticksSinceLastContact);

        json += s;
    }

    json += "]";

    SDL_UnlockMutex (pUsersMutex);
}
int Server::MainLoop (void)
{
    // This function continually runs in a separate thread to handle client requests

    Uint32 ticks0 = SDL_GetTicks(), ticks;
    TCPsocket clientSocket;

    while (!StopCondition ())
    {
        // Poll for incoming tcp connections:
        while ((clientSocket = SDLNet_TCP_Accept (tcp_socket)) != NULL)
        {
            SDL_Thread *pThread = MakeSDLThread (
            [this, clientSocket]
            {
                this->OnTCPConnection (clientSocket);
                return 0;
            },
            (std::string (PROCESS_TAG) + "_tcp_thread").c_str ());

            // Don't wait for completion:
            SDL_DetachThread (pThread);
        }

        // Poll for incoming packets:
        while (SDLNet_UDP_Recv (udp_socket, in))
        {
            OnUDPPackage (in->address, in->data, in->len);
        }

        // Get time passed since last iteration:
        ticks = SDL_GetTicks();
        Update (ticks - ticks0);
        ticks0 = ticks;

        SDL_Delay (100); // sleep to allow the other thread to run
    }

    return 0;
}

Server server;

#ifdef IMPL_UNIX_DEAMON

#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>

SyslogAppender::SyslogAppender ()
{
    openlog (PROCESS_TAG, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
}
SyslogAppender::~SyslogAppender ()
{
    closelog ();
}
void SyslogAppender::Message (MessageType type, const char *format, va_list args)
{
    int priority;
    switch (type)
    {
    case SERVER_MSG_DEBUG:
        priority = LOG_DEBUG;
    break;
    case SERVER_MSG_INFO:
        priority = LOG_INFO;
    break;
    case SERVER_MSG_ERROR:
        priority = LOG_ERR;
    break;
    }

    vsyslog (priority, format, args);
}
struct stat sts;
bool ProcessExists (const pid_t pid)
{
    char procPath [26];
    sprintf (procPath, "/proc/%d", pid);

    return (stat (procPath, &sts) == 0);
}
bool Server::Start (void)
{
    if (!(NetInit () && ResourceInit ()))
        return false;

    if (!Deamonize ())
        return false;

    return true;
}
bool Server::Status (void)
{
    pid_t pid = GetDeamonPID ();

    if (pid > 0 && ProcessExists (pid))
    {
        printf ("%s is running as process %d\n", PROCESS_TAG, pid);
        return true;
    }
    else
    {
        printf ("%s is not running\n", PROCESS_TAG);
        return false;
    }
}
bool Server::Stop (void)
{
    pid_t pid = GetDeamonPID ();

    if (pid <= 0 || !ProcessExists (pid))
    {
        Message (SERVER_MSG_ERROR, "deamon is not running");
        return false;
    }

    if (kill (pid, SIGINT) != 0)
    {
        Message (SERVER_MSG_ERROR, "failed to stop deamon: %s", std::strerror (errno));
        return false;
    }

    return true;
}
bool Server::Reload (void)
{
    pid_t pid = GetDeamonPID ();
    if (pid <= 0)
    {
        Message (SERVER_MSG_ERROR, "deamon is not running");
        return false;
    }

    if (kill (pid, SIGHUP) != 0)
    {
        Message (SERVER_MSG_ERROR, "failed to kick deamon: %s", std::strerror (errno));
        return false;
    }
    return true;
}
void Server::DeamonStopCallBack (int param)
{
    server.done = true;
}
void Server::DeamonKickCallBack (int param)
{
    server.NetCleanUp ();
    server.Configure ();
    server.NetInit ();
}
pid_t Server::GetDeamonPID (void)
{
    FILE *pFile;
    pid_t pid;
    pFile = fopen (pidPath.c_str (), "r");
    if (!pFile)
    {
        return -1;
    }

    if (1 != fscanf (pFile, "%d", &pid))
    {
        return -1;
    }

    fclose (pFile);

    return pid;
}
bool Server::StopCondition (void)
{
    return done;
}
bool Server::Deamonize (void)
{
    MessageAppender *pOldAppender;
    FILE *pFile;
    pid_t pid, sid;

    pid = fork ();
    if (pid < 0)
    {
        Message (SERVER_MSG_ERROR, "failed to fork: %s", std::strerror (errno));
        return false;
    }
    else if (pid > 0) // success, but I'm not the deamon process
        return true;

    sid = setsid ();
    if (sid < 0)
    {
        Message (SERVER_MSG_ERROR, "setsid failed: %s", std::strerror (errno));
        return false;
    }

    // ignore these signals:
    signal (SIGCHLD, SIG_IGN);

    // deamon gets kicked on these signals:
    signal (SIGHUP, DeamonKickCallBack);

    // deamon must stop if it gets these signals:
    signal (SIGINT,  DeamonStopCallBack);
    signal (SIGSEGV, DeamonStopCallBack);
    signal (SIGSTOP, DeamonStopCallBack);
    signal (SIGKILL, DeamonStopCallBack);

    pFile = fopen (pidPath.c_str (), "w");
    if (!pFile)
    {
        Message (SERVER_MSG_ERROR, "cannot write to %s: %s", pidPath.c_str (),
                         std::strerror (errno));
        return false;
    }

    fprintf (pFile, "%d", getpid ());
    fclose (pFile);

    // Change the current working directory
    if ((chdir("/")) < 0) {

        Message (SERVER_MSG_ERROR, "chdir to \'/\': %s", std::strerror (errno));
        return false;
    }

    // Close out the standard file descriptors
    fclose (stdin);
    fclose (stdout);
    fclose (stderr);

    // Begin working as a server:
    done = false;

    pOldAppender = pMessageAppender;
    pMessageAppender = new SyslogAppender;

    MainLoop ();

    delete pMessageAppender;
    pMessageAppender = pOldAppender;

    // Shut down Deamon when recieving the signal:
    if (std::remove (pidPath.c_str ()) != 0)
    {
        Message (SERVER_MSG_ERROR, "cannot remove %s: %s", pidPath.c_str (),
                         std::strerror (errno));
        return false;
    }

    return true;
}
int main (int argc, char** argv)
{
    if (!server.Configure ())
        return 1;

    if (argc > 1)
    {
        if (0 == strcmp (argv [1], "start"))
        {
            if (!server.Start ())
                return 1;
        }
        else if (0 == strcmp (argv [1], "stop"))
        {
            if (!server.Stop ())
                return 1;
        }
        else if (0 == strcmp (argv [1], "status"))
        {
            if (!server.Status ())
                return 1;
        }
        else if (0 == strcmp (argv [1], "reload"))
        {
            if (!server.Reload ())
                return 1;
        }
        else
        {
            printf ("unknown command: %s\n", argv [1]);
            return 1;
        }
    }
    else
        printf ("Usage: %s (start|stop|status|reload)\n", argv [0]);

    return 0;
}

#elif defined IMPL_WINDOWS_SERVICE

/*
    How to run (as admin) ..

    Install service with "server-install.cmd" script

    Uninstall with "server-uninstall.cmd" script

    (uninstall needs to be done, before installing anew)

    Stopping/Starting from Service Controller Manager:

        1. run "services.msc"
        2. locate "game-templates-server"
        3. press "start" or "stop"
 */

#include <Tchar.h>

SERVICE_STATUS serviceStatus = {0};
SERVICE_STATUS_HANDLE hStatus = NULL;

HANDLE hServiceStopEvent = INVALID_HANDLE_VALUE;

#define SERVICE_NAME _T(PROCESS_TAG)

int _tmain (int argc, TCHAR **argv)
{
    if (!server.Configure ())
        return 1;

    SERVICE_TABLE_ENTRY serviceTable [] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION) Server::ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher (serviceTable) == FALSE)
    {
        return GetLastError ();
    }

    return 0;
}

void Server::TrySetServiceStatus (SERVICE_STATUS_HANDLE, SERVICE_STATUS *pServiceStatus)
{
    if (SetServiceStatus (hStatus, pServiceStatus) == FALSE)
    {
        server.Message (SERVER_MSG_ERROR, "error setting service status: %s",
                        WindowsErrorString (GetLastError ()).c_str ());
    }
}

VOID WINAPI Server::ServiceMain (DWORD argc, LPTSTR *argv)
{
    MessageAppender *pOldAppender = server.pMessageAppender;
    server.pMessageAppender = new FileLogAppender (std::string (SDL_GetBasePath()) + "server.log");

    // Register our ServiceCtrlHandler function:

    hStatus = RegisterServiceCtrlHandler (SERVICE_NAME, ServiceControlHandler);

    // Tell Service controller we are starting:

    ZeroMemory (&serviceStatus, sizeof (serviceStatus));
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwWin32ExitCode = 0;
    serviceStatus.dwServiceSpecificExitCode = 0;
    serviceStatus.dwCheckPoint = 0;

    TrySetServiceStatus (hStatus, &serviceStatus);

    // Create a service stop event:
    hServiceStopEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
    if (hServiceStopEvent == NULL)
    {
        DWORD err = GetLastError ();

        server.Message (SERVER_MSG_ERROR, "error creating stop event: %s", WindowsErrorString (err).c_str ());

        // Tell service controller we stopped, then exit:

        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = err;
        serviceStatus.dwCheckPoint = 1;

        TrySetServiceStatus (hStatus, &serviceStatus);

        return;
    }

    // Init Networking & Resources:
    if (!(server.NetInit () && server.ResourceInit ()))
    {
        // Tell service controller we stopped, then exit:

        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = 1;
        serviceStatus.dwCheckPoint = 1;

        TrySetServiceStatus (hStatus, &serviceStatus);

        return;
    }

    // Tell service controller we started:
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    serviceStatus.dwWin32ExitCode = 0;
    serviceStatus.dwCheckPoint = 0;

    TrySetServiceStatus (hStatus, &serviceStatus);

    // Start Working as a server:
    HANDLE hThread = CreateThread (NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    server.Message (SERVER_MSG_INFO, "server is now running");

    // Wait until worker thread exits:
    WaitForSingleObject (hThread, INFINITE);

    // Clean up:
    CloseHandle (hServiceStopEvent);

    // Tell service controller we stopped
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwWin32ExitCode = 0;
    serviceStatus.dwCheckPoint = 3;

    TrySetServiceStatus (hStatus, &serviceStatus);

    delete server.pMessageAppender;
    server.pMessageAppender = pOldAppender;
}
VOID WINAPI Server::ServiceControlHandler (DWORD controlCode)
{
    switch (controlCode)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:

        if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        server.Message (SERVER_MSG_INFO, "stop command recieved from service control");

        // Change service status

        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        serviceStatus.dwWin32ExitCode = 0;
        serviceStatus.dwCheckPoint = 4;

        TrySetServiceStatus (hStatus, &serviceStatus);

        // Signal the worker thread to stop

        SetEvent (hServiceStopEvent);

    break;

    default:
    break;
    }
}
bool Server::StopCondition (void)
{
    return (WaitForSingleObject (hServiceStopEvent, 0) == WAIT_OBJECT_0);
}
DWORD WINAPI Server::ServiceWorkerThread (LPVOID lpParam)
{
    return server.MainLoop ();
}
#elif defined IMPL_CONSOLE_SERVER
bool Server::StopCondition (void)
{
    return done;
};
int Server::ConsoleRun (void)
{
    int result;
    SDL_Thread *pThread;

    if (!(NetInit () && ResourceInit ()))
        return 1;

    done = false;
    pThread = MakeSDLThread (
                [this]
                {
                    return this->MainLoop ();
                }
              , (std::string (PROCESS_TAG) + "_main_loop").c_str ());

    printf ("%s is now running, press enter to shut down.\n", PROCESS_TAG);
    fgetc (stdin);

    done = true;
    SDL_WaitThread (pThread, &result);

    return result;
}
int main (int argc, char** argv)
{
    if (!server.Configure ())
        return 1;

    return server.ConsoleRun ();
}
#endif
