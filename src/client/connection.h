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


#ifndef CONNECTION_H
#define CONNECTION_H

#include "client.h"
#include "../server/server.h"

/*
 * A scene that keeps contact with the server by sending pings during Update calls.
 */
class ConnectedScene : public Client::Scene
{
private:
    float timeSinceLastPing,
          timeSinceLastServerMessage;
    bool pinging;
    Uint32 ping0, ping;

    void SendPing();

    bool logged_in;

protected:

    void Logout (); // voluntary disconnect, tell server about logout

    virtual void OnConnectionLoss () {}; // involuntary, when something wnet wrong

public:

    Uint32 GetPing () const; // in ticks
    bool IsLoggedIn () const { return logged_in; }
    ConnectedScene (Client *);
    virtual ~ConnectedScene ();

    virtual void Update(const float dt);

    virtual void OnServerMessage (const Uint8* data, int len);
    virtual void OnLogin(const char* username, UserParams*, UserState*) { logged_in = true; };
    virtual void OnShutDown ();
};

#endif // CONNECTION_H
