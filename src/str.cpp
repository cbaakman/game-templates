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


#include "str.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <math.h>

const char *ParseFloat(const char *in, float *out)
{
    float f = 10.0f;
    int digit, ndigit = 0;
    *out = 0;
    const char *p = in;
    while (*p)
    {
        if (isdigit(*p))
        {
            digit = (*p - '0');
            if (f > 1.0f)
            {
                *out *= f;
                *out += digit;
            }
            else
            {
                *out += f * digit;
                f *= 0.1f;
            }
            ndigit++;
        }
        else if (tolower(*p) == 'e')
        {
            if (ndigit <= 0)
                *out = 1.0f;

            p++;
            if (*p == '+')
                p++;

            int e = atoi (p);

            *out = *out * pow(10, e);

            if (*p == '-') p++;
            while (isdigit(*p)) p++;
            return p;
        }
        else if (*p == '.')
        {
            f = 0.1f;
        }
        else if (*p == '-')
        {
            float v;
            p = ParseFloat(p + 1, &v);

            *out = -v;

            return p;
        }
        else
        {
            if (ndigit > 0)
                return p;
            else
                return NULL;
        }
        p++;
    }

    return p;
}
bool isnewline(const int c)
{
    return (c=='\n'||c=='\r');
}
bool emptyline (const char *line)
{
    while (*line)
    {
        if (!isspace (*line))
            return false;
        line ++;
    }
    return true;
}
void stripr (char *s)
{
    int n = strlen (s);
    while (true)
    {
        n --;
        if (isspace (s[n]))
        {
            s[n] = NULL;
        }
        else break;
    }
}
int StrCaseCompare(const char *s1, const char *s2)
{
    int i = 0, n = 0;

    while (s1[i] && s2[i])
    {

        n = toupper(s1[i]) - toupper(s2[i]);

        if(n != 0)
            return n;

        i++;
    }

    if(s1[i] && !s2[i])
        return 1;
    else if(!s1[i] && s2[i])
        return -1;
    else
        return 0;
}
void split (const char *s, const char b, std::list<std::string> &out)
{
    std::string str = "";
    while (*s)
    {
        if (*s == b)
        {
            if (str.size() > 0)
                out.push_back(str);

            str = "";
        }
        else
            str += *s;
        s++;
    }
    if (str.size() > 0)
        out.push_back(str);
}

void WordAt (const char *s, const int pos, int &start, int &end)
{
    bool bSpace = isspace (s[pos]);

    start = pos;
    while (start > 0)
    {
        if (isspace (s [start - 1]) != bSpace)
            break;
        start--;
    }

    end = pos;
    while (s[end] && isspace (s [end]) == bSpace)
        end++;
}
