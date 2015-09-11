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
#include <string.h>
#include <cstdarg>

#include "server.h"

#include "../ini.h"
#include "../ip.h"
#include "../str.h"

#define SETTINGS_FILE "settings.ini"

#define PORT_SETTING "port"
#define MAXLOGIN_SETTING "max-login"

#define ACCOUNT_DIR "accounts"
#define CONNECTION_PINGPERIOD 1000 // ticks

const int CONNECTION_TIMEOUT_TICKS = CONNECTION_TIMEOUT * 1000;

void Server::OnMessage (Server::MessageType type, const char *format, ...)
{
    va_list args;
    va_start (args, format);

    switch (type)
    {
    case INFO:
        fprintf (stdout, "INFO: ");
        vfprintf (stdout, format, args);
        fprintf (stdout, "\n");
    break;
    case ERROR:
        fprintf (stderr, "ERROR: ");
        vfprintf (stderr, format, args);
        fprintf (stderr, "\n");
    break;
    }

    va_end (args);
}
Server::Server() :
    done(false),
    users (NULL),
    maxUsers(0),
    in(NULL), out(NULL),
    udpPackets(NULL),
    socket(NULL)
{
    settingsPath[0]=NULL;
}
Server::~Server()
{
    CleanUp ();
}
bool Server::Init()
{
    settingsPath = std::string(SDL_GetBasePath()) + SETTINGS_FILE;
    accountsPath = std::string(SDL_GetBasePath()) + ACCOUNT_DIR;

    // load the maxUsers setting from the config
    maxUsers = LoadSetting(settingsPath.c_str(), MAXLOGIN_SETTING);
    if (maxUsers > 0)
    {
        // Allocate memory for users to log in:
        users = new UserP [maxUsers];
        for (Uint64 i = 0; i < maxUsers; i++)
            users[i] = NULL;
    }
    else
    {
        OnMessage (ERROR, "Max Login not found in %s\n", settingsPath.c_str());
        return false;
    }

    // Load the port number from config
    port = LoadSetting (settingsPath.c_str(), PORT_SETTING);
    if (port <= 0)
    {
        OnMessage (ERROR,"Port not set in %s\n", settingsPath.c_str());
        return false;
    }

    // Init SDL_net
    if (SDLNet_Init () < 0)
    {
        OnMessage (ERROR,"SDLNet_Init: %s\n", SDLNet_GetError ());
        return false;
    }

    // Open a socket and listen at the configured port:
    if (!(socket = SDLNet_UDP_Open (port)))
    {
        OnMessage (ERROR,"SDLNet_UDP_Open: %s\n", SDLNet_GetError());
        return false;
    }

    // Allocate packets of the size we need, one for incoming, one for outgoing
    if (!(udpPackets = SDLNet_AllocPacketV (2, PACKET_MAXSIZE)))
    {
        OnMessage (ERROR, "SDLNet_AllocPacketV: %s\n", SDLNet_GetError());
        return false;
    }
    in = udpPackets [0];
    out = udpPackets [1];

    // seed the random number generator:
    srand (time(0));

    done = false;
    return true;
}
void Server::CleanUp()
{
    if (udpPackets)
    {
        SDLNet_FreePacketV (udpPackets);
        udpPackets = NULL;
        in = out = NULL;
    }
    if (socket)
    {
        SDLNet_UDP_Close(socket);
        socket = NULL;
    }

    SDLNet_Quit();

    // Clean up memory
    for(Uint64 i=0; i < maxUsers; i++)
        delete users[i];
    delete [] users; users=NULL;
    maxUsers = 0;
}
Server::UserP Server::AddUser()
{
    for(Uint64 i = 0; i < maxUsers; i++)
    {
        if (!users[i]) // found an empty sport for a new user
        {
            users[i] = new User;
            users[i]->accountName [0] = NULL;
            users[i]->ticksSinceLastContact = 0;

            // Generate a RSA key to encrypt login password in
            BIGNUM *bn = BN_new (); BN_set_word (bn, 65537);
            users[i]->key = RSA_new ();
            bool keySuccess = RSA_generate_key_ex (users[i]->key, 1024, bn, NULL)
                && RSA_check_key (users[i]->key);

            BN_free (bn);

            if (!keySuccess)
            {
                RSA_free (users[i]->key);
                delete users[i]; users[i]=NULL;
                return NULL;
            }

            return users [i];
        }
    }

    return NULL;
}
Server::UserP Server::GetUser (const IPaddress& address)
{
    for(Uint64 i = 0; i < maxUsers; i++)
    {
        if (users[i]
            && users[i]->address.host == address.host
            && users[i]->address.port == address.port)
        {
            return users [i];
        }
    }
    return NULL;
}
Server::UserP Server::GetUser (const char* accountName)
{
    for(Uint64 i = 0; i < maxUsers; i++)
    {
        if (users[i]
            && strcmp (users[i]->accountName, accountName)==0 )
        {
            return users[i];
        }
    }
    return NULL;
}
bool Server::LoggedIn(Server::User* user)
{
    if (!user)
        return false;

    // If accountname is set, user is considered logged in
    return (bool) user->accountName [0];
}
void Server::OnPlayerRemove (Server::User* user)
{
    // Tell other players about this one's removal:
    int len = USERNAME_MAXLENGTH + 1;
    Uint8*data = new Uint8 [USERNAME_MAXLENGTH + 1];
    data [0] = NETSIG_DELPLAYER;
    memcpy (data + 1,user->accountName, USERNAME_MAXLENGTH);

    // Send the message to all clients:
    for(Uint64 i=0; i<maxUsers; i++)
    {
        if(users[i] && LoggedIn(users[i]) && users[i]!=user)
        {
            SendToClient (users[i]->address, data, len);
        }
    }
    delete[] data;
}
void Server::TellAboutLogout (Server::User* to, const char* loggedOutUsername)
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
void Server::TellUserAboutUser (Server::User* to, Server::User* about)
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
void Server::DelUser(const Uint64 i)
{
    RSA_free (users[i]->key);
    delete users[i];
    users [i] = NULL;
}
void Server::DelUser(Server::User* user)
{
    for(Uint64 i=0; i<maxUsers; i++)
    {
        if (users[i]==user)
        {
            DelUser(i);
        }
    }
}
void Server::Update(Uint32 ticks)
{
    for(Uint64 i=0; i<maxUsers; i++)
    {
        UserP user=users[i];
        if (user)
        {
            // periodically send pings to every user

            user->ticksSinceLastContact += ticks;
            if(!(user->pinging) && user->ticksSinceLastContact > CONNECTION_PINGPERIOD)
            {
                Uint8 ping = NETSIG_PINGSERVER;
                SendToClient (user->address,&ping,1);
                user->pinging = true;
            }
            if( user->ticksSinceLastContact > CONNECTION_TIMEOUT_TICKS )
            {
                // A connecton timeout

                OnPlayerRemove(user);
                DelUser(i);
            }
        }
    }
}
void Server::OnStateSet (User *user, UserState* state)
{
    user->state.ticks = SDL_GetTicks();
    user->state.pos = state->pos;

    SendUserStateToAll (user);
}
void Server::OnChatMessage (const User *user, const char *msg)
{
    ChatEntry e;
    strcpy (e.username, user->accountName);
    strcpy (e.message, msg);

    chat_history.push_back (e);

    // Tell everybody about this chat message:

    int len = sizeof (e) + 1;
    Uint8* data = new Uint8[len];
    data [0] = NETSIG_CHATMESSAGE;
    memcpy (data + 1, &e, sizeof (e));

    OnMessage (INFO, "%s said: %s", user->accountName, msg);

    SendToAll (data, len);

    delete [] data;
}
void Server::SendUserStateToAll (User* user)
{
    int len = 1 + USERNAME_MAXLENGTH + sizeof(UserState);
    Uint8* data = new Uint8[len];
    data[0] = NETSIG_USERSTATE;
    memcpy (data + 1, &user->accountName, USERNAME_MAXLENGTH);
    memcpy (data + 1 + USERNAME_MAXLENGTH, &(user->state), sizeof(UserState));

    // Sends the state to everyone EXCEPT the state's user himself
    for (Uint64 i = 0; i < maxUsers; i++)
    {
        if (LoggedIn (users[i]) && users[i]!=user)
        {
            SendToClient(users[i]->address, data, len);
        }
    }
    delete[] data;
}
void Server::SendToAll (const Uint8 *data, const int len)
{
    // Sends data package to all users in the list
    for (Uint64 i=0; i < maxUsers; i++)
    {
        if (LoggedIn(users[i]))
        {
            SendToClient (users[i]->address, data, len);
        }
    }
}
void Server::OnLogout(Server::User* user)
{
    // User requested logout, remove and tell other users

    OnPlayerRemove (user);
    DelUser (user);

    OnMessage (INFO, "%s just logged out", user->accountName);
}
void Server::OnRequest(const IPaddress& clientAddress, Uint8*data, int len)
{
    UserP user = GetUser (clientAddress);
    if (user)
    {
        // this is a sign of life, reset ping time
        user->ticksSinceLastContact = 0;
    }

    // See if we're dealing with a logged in user:
    bool logged_in = LoggedIn (user);

    Uint8 signature = data [0];
    data++; len--;

    // What we do now depends on the signature (first byte)
    switch(signature)
    {
    case NETSIG_LOGOUT:
        if(user && logged_in)
            OnLogout(user);
    break;
    case NETSIG_LOGINREQUEST:
        if(!user)
            OnLoginRequest(clientAddress);
    break;
    case NETSIG_RSAENCRYPTED:
        OnRSAEncrypted(clientAddress,data,len);
    break;
    case NETSIG_AUTHENTICATE:
        if (user && len == sizeof(LoginParams))
            OnAuthenticate(clientAddress,(LoginParams*)data);
    break;
    case NETSIG_PINGCLIENT:
        if (logged_in)
            SendToClient(clientAddress,&signature,1);
    break;
    case NETSIG_PINGSERVER:
        if (logged_in)
            user->pinging=false;
    break;
    case NETSIG_USERSTATE:
        if (logged_in && len == sizeof(UserState))
        {
            UserState* state=(UserState*)data;
            OnStateSet(user, state);
        }
    break;
    case NETSIG_CHATMESSAGE:

        if (logged_in)
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
        if (logged_in && len == USERNAME_MAXLENGTH)
        {
            User* other=GetUser((const char*)data);
            if(other)
            {
                TellUserAboutUser(user,other);
            }
            else // this user doesn't know yet about this other user logging out
            {
                TellAboutLogout(user,(const char*)data);
            }
        }
    default:
        return;
    }
}
void Server::OnRSAEncrypted(const IPaddress& clientAddress, Uint8*encrypted, int len)
{
    // Decrypt the message using the RSA key we remembered for this user

    UserP user = GetUser(clientAddress);
    if (!user)
        return;

    int maxflen=maxFLEN(user->key);

    Uint8* decrypted = new Uint8[maxflen];
    int decryptedSize = RSA_private_decrypt (len, encrypted, decrypted, user->key, SERVER_RSA_PADDING);
    OnRequest (clientAddress, decrypted, decryptedSize);
    delete[] decrypted;
}
void Server::OnAuthenticate(const IPaddress& clientAddress, LoginParams* params)
{
    User *pendingUser = GetUser(clientAddress),
         *prevUser;

    if ((prevUser = GetUser (params->username)))
    {
        // This username is already logged in

        // Tell the client that this user is already logged in:
        Uint8 response = NETSIG_ALREADYLOGGEDIN;
        SendToClient (clientAddress,&response,1);

        // Remove the user that requested the login:
        if (prevUser != pendingUser)
            DelUser (pendingUser);
    }
    else
    {
        UserP user = GetUser (clientAddress);
        if (user && authenticate (accountsPath.c_str(), params->username, params->password))
        {
            // username and password match

            // register username with ip-address:
            strcpy (user->accountName, params->username);

            user->params.hue = rand() % 360; // give the user a random color
            user->state.pos.x=-1000;
            user->state.pos.y=-1000;

            // Send user the message that login succeeded,
            // along with his first state and parameters:
            int len = 1 + sizeof(UserParams) + sizeof(UserState);
            Uint8* data = new Uint8 [len];
            data [0] = NETSIG_LOGINSUCCESS;
            memcpy(data + 1, &user->params, sizeof(UserParams));
            memcpy(data + 1 + sizeof(UserParams), &user->state, sizeof(UserState));
            SendToClient (clientAddress,data,len);
            delete data;

            // Tell other users about this new user:
            for (Uint64 i = 0; i < maxUsers; i++)
            {
                if (users[i] && LoggedIn (users[i]))
                {
                    if(users[i]!=user)
                    {
                        TellUserAboutUser (user, users[i]);
                        TellUserAboutUser (users[i], user);
                    }
                }
            }

            OnMessage (INFO, "%s just logged in", params->username);
        }
        else // authentication failed
        {
            Uint8 response = NETSIG_AUTHENTICATIONERROR;
            SendToClient (clientAddress, &response, 1);

            // Remove the user that requested the login:
            DelUser (pendingUser);
        }
    }
}
void Server::OnLoginRequest (const IPaddress& clientAddress)
{
    if (GetUser (clientAddress))
    {
        // We already have a user logged in under the requested username

        Uint8 response = NETSIG_ALREADYLOGGEDIN;
        SendToClient (clientAddress, &response, 1);
    }
    else
    {
        UserP user = AddUser ();
        if (user)
        {
            // Send the user a public key to encrypt password:

            int keySize = i2d_RSAPublicKey(user->key, NULL);
            Uint8* data = new Uint8 [keySize + 1];
            data [0] = NETSIG_RSAPUBLICKEY;
            unsigned char *pe = (unsigned char *)(data + 1);
            i2d_RSAPublicKey (user->key, &pe);

            SendToClient (clientAddress, data, keySize + 1);

            delete [] data;

            // Register IP address:
            user->address = clientAddress;
        }
        else // too many users already
        {
            Uint8 response = NETSIG_SERVERFULL;
            SendToClient (clientAddress, &response, 1);
        }
    }
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

    SDLNet_UDP_Send (socket, -1, out);

    if (out->status != len) // the wrong number has been returned
        return false;

    return true;
}
void Server::PrintChatHistory () const
{
    for (std::list<ChatEntry>::const_iterator it = chat_history.begin(); it != chat_history.end(); it++)
    {
        const ChatEntry *pEntry = &(*it);
        printf ("%s said: %s\n", pEntry->username, pEntry->message);
    }
}
void Server::PrintUsers() const
{
    bool printedHeader = false;
    char ipStr[IP_STRINGLENGTH];
    int nFound = 0;
    for(Uint64 i = 0; i < maxUsers; i++)
    {
        if (!users[i])
            continue;

        if (nFound <= 0)
        { // only print a header if there are users
            printf("IP-address:   \taccount-name:  \tlast contact(ms ago):\n");
        }
        nFound ++;

        ip2String(users[i]->address, ipStr);
        printf("%s\t%14s\t%u\n", ipStr, users[i]->accountName, users[i]->ticksSinceLastContact);
    }
    if (nFound <= 0)
        printf ("nobody is logged in right now\n");
}
void Server::ShutDown (void)
{
    done = true;
}
int Server::Loop ()
{
    // tell console that all went well:
    printf ("server started sucessfully\n");

    done = false;
    Uint32 ticks0 = SDL_GetTicks(),
           ticks;

    while (!done) // is the server still running?
    {
        // Poll for incoming packets:
        while (SDLNet_UDP_Recv (socket, in))
        {
            OnRequest (in->address, in->data, in->len);
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

void OnExit (void)
{
    pServer.CleanUp();
}

int main(int argc, char** argv)
{
    if (atexit (OnExit) != 0)
        return 1;

    if(!server.Init())
        return 1;

    int res = server.Loop ();

    server.CleanUp();

    return res;
}
