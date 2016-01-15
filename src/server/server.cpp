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


#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include "server.h"

#include "../ini.h"
#include "../ip.h"
#include "../str.h"
#include "../thread.h"

#define SETTINGS_FILE "settings.ini"

#define PORT_SETTING "port"
#define MAXLOGIN_SETTING "max-login"

#define ACCOUNT_DIR "accounts"
#define CONNECTION_PINGPERIOD 1000 // ticks

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

/**
 * This runs in a temporary thread.
 */
void Server::OnLogin (TCPsocket clientSocket, const IPaddress *pClientIP)
{
    int n_sent,
        n_recieved,
        keySize, keyDataSize,
        decryptedSize,
        maxflen;
    unsigned char *public_key_data, *public_key,
                  encrypted [PACKET_MAXSIZE],
                  *decrypted;
    RSA* key;
    BIGNUM *bn;
    Uint8 signal;

    // If the server is full at this point, don't bother.
    if (IsServerFull ())
    {
        signal = NETSIG_SERVERFULL;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            fprintf (stderr, "WARNING, could not send server full error to user: %s\n", SDLNet_GetError ());
        }
        return;
    }

    // Generate a RSA key to encrypt login password in
    bn = BN_new ();
    BN_set_word (bn, 65537);
    key = RSA_new ();
    bool keySuccess = RSA_generate_key_ex (key, 1024, bn, NULL) &&
                      RSA_check_key (key);
    BN_free (bn);

    if (!keySuccess)
    {
        fprintf (stderr, "rsa key generation failed\n");
        RSA_free (key);

        signal = NETSIG_INTERNALERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            fprintf (stderr, "WARNING, could not send internal error to user: %s\n", SDLNet_GetError ());
        }

        return;
    }

    // Convert public key to byte array and send to user:
    keySize = i2d_RSAPublicKey (key, NULL);
    keyDataSize = keySize + 1;
    public_key_data = new unsigned char [keyDataSize];
    public_key_data [0] = NETSIG_RSAPUBLICKEY;
    public_key = public_key_data + 1;
    i2d_RSAPublicKey (key, &public_key);

    n_sent = SDLNet_TCP_Send (clientSocket, public_key_data, keyDataSize);
    delete [] public_key_data;

    if (n_sent != keyDataSize)
    {
        fprintf (stderr, "error sending key: %s\n", SDLNet_GetError());
        RSA_free (key);
        return;
    }

    // Receive encrypted login parameters from user:
    n_recieved = SDLNet_TCP_Recv (clientSocket, encrypted, PACKET_MAXSIZE);
    if (n_recieved <= 0)
    {
        if (n_recieved < 0)
            fprintf (stderr, "error recieving encrypted login: %s\n", SDLNet_GetError());
        else
        {
            char ip [100];
            ip2String (*pClientIP, ip);
            fprintf (stderr, "client at %s hung up while recieving encrypted login\n", ip);
        }

        RSA_free (key);
        return;
    }

    // Decrypt login parameters:
    maxflen = maxFLEN (key);
    decrypted = new Uint8 [maxflen];

    decryptedSize = RSA_private_decrypt (n_recieved, encrypted, decrypted, key, SERVER_RSA_PADDING);
    RSA_free (key);

    if (decryptedSize != sizeof (LoginParams))
    {
        fprintf (stderr, "error decrypting login parameters, wrong size\n");
        delete [] decrypted;

        signal = NETSIG_AUTHENTICATIONERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            fprintf (stderr, "WARNING, could not send authentication error to user: %s\n", SDLNet_GetError ());
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
            fprintf (stderr, "WARNING, could not send already logged in error to user: %s\n", SDLNet_GetError ());
        }
    }
    else if (authenticate (accountsPath.c_str(), pParams->username, pParams->password))
    {
        // To keep it thread safe, we must copy these values before the user enters the list..
        IPaddress clientAddress;
        clientAddress.host = pClientIP->host;
        clientAddress.port = pParams->udp_port;

        UserParams userParams;
        userParams.hue = GetNextRand() % 360; // give the user a random color

        UserP pUser = new User (&clientAddress, pParams->username, &userParams);

        UserState startState = pUser->state;

        // Try to add user to the list:
        if (AddUser (pUser))
        {
            // Send client the message that login succeeded,
            // along with the user's first state and parameters:
            int len = 1 + sizeof (UserParams) + sizeof (UserState);
            Uint8* data = new Uint8 [len];
            data [0] = NETSIG_LOGINSUCCESS;
            memcpy (data + 1, &userParams, sizeof (UserParams));
            memcpy (data + 1 + sizeof (UserParams), &startState, sizeof (UserState));
            n_sent = SDLNet_TCP_Send (clientSocket, data, len);
            delete data;

            if (n_sent != len)
            {
                fprintf (stderr, "WARNING, could not send login conformation to user: %s\n", SDLNet_GetError ());
            }

            OnMessage (SERVER_MSG_INFO, "%s just logged in", pUser->accountName);

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
                fprintf (stderr, "Error telling users about new user, could not lock mutex: %s\n", SDL_GetError ());
        }
        else // AddUser failed, server full
        {
            signal = NETSIG_SERVERFULL;
            if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
            {
                fprintf (stderr, "WARNING, could not send server full error to user: %s\n", SDLNet_GetError ());
            }
        }
    }
    else // authentication failure
    {
        signal = NETSIG_AUTHENTICATIONERROR;
        if (SDLNet_TCP_Send (clientSocket, &signal, 1) != 1)
        {
            fprintf (stderr, "WARNING, could not send authentication error to user: %s\n", SDLNet_GetError ());
        }
    }

    delete [] decrypted;
}

void Server::OnMessage (Server::MessageType type, const char *format, ...)
{
    va_list args;
    va_start (args, format);

    switch (type)
    {
    case SERVER_MSG_INFO:
        fprintf (stdout, "INFO: ");
        vfprintf (stdout, format, args);
        fprintf (stdout, "\n");
    break;
    case SERVER_MSG_ERROR:
        fprintf (stderr, "ERROR: ");
        vfprintf (stderr, format, args);
        fprintf (stderr, "\n");
    break;
    }

    va_end (args);
}

Server::Server() :
    done(false),
    pUsersMutex (NULL),
    maxUsers(0),
    loopThread(NULL),
    in(NULL), out(NULL),
    udpPackets(NULL),
    tcp_socket(NULL), udp_socket(NULL)
{
    settingsPath[0]=NULL;

    pUsersMutex = SDL_CreateMutex ();
}
Server::~Server()
{
    SDL_DestroyMutex (pUsersMutex);
}
bool Server::Init()
{
    settingsPath = std::string(SDL_GetBasePath()) + SETTINGS_FILE;
    accountsPath = std::string(SDL_GetBasePath()) + ACCOUNT_DIR;

    // load the maxUsers setting from the config
    maxUsers = LoadSetting(settingsPath.c_str(), MAXLOGIN_SETTING);
    if (maxUsers <= 0)
    {
        fprintf (stderr, "Max Login not found in %s\n", settingsPath.c_str());
        return false;
    }

    // Load the port number from config
    port = LoadSetting (settingsPath.c_str(), PORT_SETTING);
    if (port <= 0)
    {
        fprintf (stderr,"Port not set in %s\n", settingsPath.c_str());
        return false;
    }

    // Init SDL_net
    if (SDLNet_Init() < 0)
    {
        fprintf (stderr,"SDLNet_Init: %s\n", SDLNet_GetError());
        return false;
    }

    // Open a socket and listen at the configured port:
    if (!(udp_socket = SDLNet_UDP_Open (port)))
    {
        fprintf (stderr,"SDLNet_UDP_Open: %s\n", SDLNet_GetError());
        return false;
    }

    if (SDLNet_ResolveHost (&mAddress, NULL, port) < 0)
    {
        fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
        return false;
    }

    // Open a socket and listen at the configured port:
    if (!(tcp_socket = SDLNet_TCP_Open (&mAddress)))
    {
        fprintf (stderr,"SDLNet_TCP_Open: %s\n", SDLNet_GetError());
        return false;
    }

    // Start the request handling thread
    loopThread = MakeSDLThread (
        [this]
        {
            return this->LoopThreadFunc ();
        }
        ,"server_request_handler_thread");
    if(!loopThread)
    {
        fprintf (stderr,"Server loop thread not started\n");
        return false;
    }

    // Allocate packets of the size we need, one for incoming, one for outgoing
    if (!(udpPackets = SDLNet_AllocPacketV (2, PACKET_MAXSIZE)))
    {
        fprintf (stderr, "SDLNet_AllocPacketV: %s\n", SDLNet_GetError());
        return false;
    }
    in = udpPackets [0];
    out = udpPackets [1];

    done = false;
    return true;
}
/**
 * Can be used from any thread.
 */
int Server::GetNextRand (void)
{
    int r = randstock.front ();
    randstock.pop_front ();

    return r;
}
void Server::CleanUp()
{
    int status;

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

    /*
        Tell the request handling thread that it should return
        and wait for it to finish.
     */
    done = true;
    if (loopThread)
    {
        SDL_WaitThread (loopThread, &status);
        loopThread = NULL;
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
        fprintf (stderr, "Error deleting users, could not lock mutex!\n");
}
bool Server::IsServerFull (void)
{
    if (SDL_LockMutex (pUsersMutex) != 0)
    {
        fprintf (stderr, "Error checking server full, could not lock mutex: %s\n", SDL_GetError ());
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
            fprintf (stderr, "Error adding user, could not lock mutex: %s\n", SDL_GetError ());
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
        fprintf (stderr, "Error getting user, could not lock mutex: %s\n", SDL_GetError ());
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
        fprintf (stderr, "Error getting user, could not lock mutex: %s\n", SDL_GetError ());
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
        fprintf (stderr, "OnPlayerRemove, error locking mutex: %s", SDL_GetError ());

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
        fprintf (stderr, "Error removing user, could not lock mutex: %s\n", SDL_GetError ());
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
        fprintf (stderr, "Error updating users, could not lock mutex: %s\n", SDL_GetError ());
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
            OnMessage (SERVER_MSG_INFO, "%s timed out", pUser->accountName);

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
        fprintf (stderr, "Error setting user state, could not lock mutex: %s\n", SDL_GetError ());

    SendUserStateToAll (user);
}
void Server::OnChatMessage (const UserP pUser, const char *msg)
{
    ChatEntry e;
    strcpy (e.username, pUser->accountName);
    strcpy (e.message, msg);

    chat_history.push_back (e);

    OnMessage (SERVER_MSG_INFO, "%s said: %s", pUser->accountName, msg);

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
        fprintf (stderr, "WARNING, failed to lock mutex while sending a message to all: %s", SDL_GetError ());
}
void Server::OnLogout (Server::User* user)
{
    // User requested logout, remove and tell other users

    OnPlayerRemove (user);
    DelUser (user);

    OnMessage (SERVER_MSG_INFO, "%s just logged out", user->accountName);
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
void Server::OnTCPConnection (TCPsocket clientSocket)
{
    if (!clientSocket)
        return;

    IPaddress *pClientIP = SDLNet_TCP_GetPeerAddress (clientSocket);
    if (!pClientIP)
    {
        fprintf (stderr, "SDLNet_TCP_GetPeerAddress: %s\n", SDLNet_GetError());
        return;
    }

    Uint8 signal;
    if (SDLNet_TCP_Recv (clientSocket, &signal, 1) == 1)
    {
        if (signal == NETSIG_LOGINREQUEST)

            OnLogin (clientSocket, pClientIP);
    }
    else
        fprintf (stderr, "Recieved no signal from newly opened tcp socket: %s\n", SDLNet_GetError());

    SDLNet_TCP_Close (clientSocket);
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
void Server::PrintChatHistory () const
{
    for (const ChatEntry &entry : chat_history)
    {
        printf ("%s said: %s\n", entry.username, entry.message);
    }
}
void Server::PrintUsers () const
{
    bool printedHeader = false;
    char ipStr[IP_STRINGLENGTH];
    int nFound = 0;

    if (SDL_LockMutex (pUsersMutex) == 0)
    {
        for (UserP pUser : users)
        {
            if (nFound <= 0)
            { // only print a header if there are users
                printf("IP-address:   \taccount-name:  \tlast contact(ms ago):\n");
            }
            nFound ++;

            ip2String (pUser->address, ipStr);
            printf ("%s\t%14s\t%u\n", ipStr, pUser->accountName, pUser->ticksSinceLastContact);
        }

        SDL_UnlockMutex (pUsersMutex);

        if (nFound <= 0)
            printf ("nobody is logged in right now\n");
    }
    else
        fprintf (stderr, "error printing users, couldn't lock mutex: %s\n", SDL_GetError ());
}
int Server::LoopThreadFunc (void)
{
    // This function continually runs in a separate thread to handle client requests

    // seed the random number generator (works for this thread only):
    srand (time (NULL));

    Uint32 ticks0 = SDL_GetTicks(), ticks;
    TCPsocket clientSocket;

    while (!done) // is the server still running?
    {
        // Keep a stock of random numbers:
        while (randstock.size() < RANDSTOCK_SIZE)
            randstock.push_back (rand ());

        // Poll for incoming tcp connections:
        while ((clientSocket = SDLNet_TCP_Accept (tcp_socket)))
        {
            SDL_Thread *pThread = MakeSDLThread (
            [&]
            {
                OnTCPConnection (clientSocket);
                return 0;
            },
            "server_tcp_thread");

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

void exit (void)
{
    server.CleanUp ();
}
int main (int argc, char** argv)
{
    if(!server.Init())
        return 1;

    atexit (exit);

    printf ("Server is now running, press enter to shut down.\n");
    fgetc (stdin);

    server.CleanUp();

    return 0;
}
