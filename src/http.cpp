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
  2. Altered source version`s must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "http.h"
#include <cstring>
#include <stdio.h>

std::string HTTPResponseOK (const void *pBytes, const size_t data_len, const char *content_type)
{
    char line [100];

    std::string response = "HTTP/1.1 200 OK\n";

    sprintf (line, "Content-Type: %s\n", content_type);
    response += line;

    sprintf (line, "Content-Length: %u\n", data_len);
    response += line;

    response += "Accept-Ranges: bytes\n"
                "Connection: close\n"
                "\n"; // NEEDS DOUBLE NEWLINE HERE !!

    response.append ((const char *)pBytes, data_len);

    return response;
}
std::string HTTPResponseFound (const char *url)
{
    char line [100];

    std::string response = "HTTP/1.1 302 Found\n";

    sprintf (line, "Location: %s\n", url);
    response += line;

    response += "Connection: close\n"
                "\n"; // NEEDS DOUBLE NEWLINE HERE !!

    return response;
}
std::string HTTPResponseNotFound ()
{
    char line [100];

    std::string response = "HTTP/1.1 404 Not Found\n";

    response += "\n";

    return response;
}
bool isnewline (const char c)
{
    return (c == '\n' || c == '\r');
}

#include <syslog.h>
bool ParseHttpRequest (const void *pBytes, const size_t data_len,
                       std::string &method, std::string &path, std::string &host)
{
    const char *text = (const char *)pBytes, // NOT null terminated !
               *p, *q,
               *line;
    int len = 0;

    // Parse first line (always same syntax)

    // Read up to first space
    p = text;
    while (!isspace (*p))
    {
        p ++;
        if (p >= (text + data_len) || isnewline (*p)) // beyond first line
            return false;
    }

    if (p == text) // WRONG! Line begins in a whitespace!
        return false;

    method.assign (text, p - text);

    // Read past successive spaces
    while (isspace (*p))
    {
        p ++;
        if (p >= (text + data_len) || isnewline (*p)) // beyond first line
            return false;
    }

    q = p;
    while (!isspace (*q))
    {
        q ++;
        if (q >= (text + data_len) || isnewline (*q)) // beyond first line
            return false;
    }

    path.assign (p, q - p);

    // Find the host line..

    // Go past the first line:
    line = q;
    while (!isnewline (*line))
        line ++;
    if (line < (text + data_len))
        line ++;

    while (line < (text + data_len))
    {
        // Determine line length..
        len = 0;
        while (!isnewline (line [len]) && line [len])
            len ++;

        // Check contents of line..
        if ((p = strchr (line, ':')) != NULL)
        {
            if (strncmp (line, "Host", p - line) == 0)
            {
                /*
                    Take Everything after the colon on this line,
                    except leading spaces..
                 */
                p ++;
                while (isspace (*p) && p < (line + len))
                    p ++;

                host.assign (p, (line + len) - p);

                return true;
            }
        }

        // Go to next line
        line += len;
        if (line >= (text + data_len))
            return false; // Last line

        line++;
    }

    return false;
}
