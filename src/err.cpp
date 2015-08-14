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


#include "err.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#define ERRSTR_LEN 512

// String to store the error in:
char error [ERRSTR_LEN] = "";

const char *GetError ()
{
    return error;
}
void SetError (const char* format, ...)
{
    // use a temporary buffer, because 'error' might be one of the args
    char tmp [ERRSTR_LEN];

    // Insert args:
    va_list args; va_start (args, format);

    vsprintf (tmp, format, args);

    va_end (args);

    // copy from temporary buffer:
    strcpy (error, tmp);
}
