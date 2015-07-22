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


#include "connection.h"
#include "../server/server.h"

#define CONNECTION_PINGPERIOD 1.0 // seconds

Uint32 ConnectedScene::GetPing() const
{
    return ping;
}
void ConnectedScene::SendPing()
{
    // Send a ping with a client signature
    ping0 = SDL_GetTicks();
    pinging = true;
    Uint8 ping = NETSIG_PINGCLIENT;

    if (!pClient->SendToServer (&ping, 1))
    {
        OnConnectionLoss();
    }
}
ConnectedScene::~ConnectedScene()
{
    if (logged_in)
        Logout ();
}
void ConnectedScene::Logout ()
{
    Uint8 msg = NETSIG_LOGOUT;
    pClient ->SendToServer(&msg, 1);
    logged_in = false;
}
ConnectedScene::ConnectedScene(Client *p) : Client::Scene (p),
    logged_in (false)
{
    timeSinceLastPing = timeSinceLastServerMessage=0;
    pinging = false;
    ping0 = ping = 0;
}
void ConnectedScene::Update (const float dt)
{
    timeSinceLastServerMessage += dt;
    timeSinceLastPing+=dt;

    if(!pinging && timeSinceLastPing > CONNECTION_PINGPERIOD)
    {
        SendPing();
    }
    if(logged_in && timeSinceLastServerMessage > CONNECTION_TIMEOUT)
    {
        OnConnectionLoss ();
        logged_in = false;
    }
}
void ConnectedScene::OnServerMessage(const Uint8 *data, int len)
{
    // Any maessage is a sign of life from server!
    timeSinceLastServerMessage = 0;

    Uint8 signature = data[0];
    if(signature == NETSIG_PINGSERVER) // server requests a sign of life
    {
        // ping back immediatly
        pClient->SendToServer(&signature,1);
    }
    else if (signature == NETSIG_PINGCLIENT) // server pings back, so reset the ping
    {
        timeSinceLastPing = 0;
        ping = SDL_GetTicks() - ping0;
        pinging = false;
    }
}
void ConnectedScene::OnShutDown ()
{
    Logout ();
}
