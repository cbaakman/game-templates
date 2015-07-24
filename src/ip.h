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


#ifndef IP_H
#define IP_H

#define IP_STRINGLENGTH 16

#include<SDL2/SDL_net.h>

inline void ip2String(const IPaddress& address,char* out)
{
    Uint8*bytes=(Uint8*)&address.host;
    sprintf(out,"%u.%u.%u.%u:%u",
            (unsigned int)bytes[0],
            (unsigned int)bytes[1],
            (unsigned int)bytes[2],
            (unsigned int)bytes[3],
            (unsigned int)address.port);
}
inline void string2IP(const char* str, IPaddress& out)
{
    Uint8*bytes=(Uint8*)&out.host;
    sscanf(str,"%u.%u.%u.%u:%u",
           (unsigned int*)&bytes[0],
           (unsigned int*)&bytes[1],
           (unsigned int*)&bytes[2],
           (unsigned int*)&bytes[3],
           (unsigned int*)&out.port);
}

#endif
