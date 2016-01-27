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
#include <stdio.h>

const char *next_from_utf8 (const char *pBytes, unicode_char *out)
{
    int n_bytes, i;

    /*
        The first bits of the first byte determine the length
        of the utf-8 character. The remaining first byte bits
        are considered coding.
     */

    char first = pBytes [0];

    *out = 0x00000000;
    if ((first & 0xe0) == 0xc0) // 110????? means 2 bytes
    {
        *out = first & 0x1f; // ???????? & 00011111
        n_bytes = 2;
    }
    else if ((first & 0xf0) == 0xe0) // 1110???? means 3 bytes
    {
        *out = first & 0x0f; // ???????? & 00001111
        n_bytes = 3;
    }
    else if ((first & 0xf8) == 0xf0) // 11110??? means 4 bytes
    {
        *out = first & 0x07; // ???????? & 00000111
        n_bytes = 4;
    }
    else // assume ascii
    {
        if ((first & 0xc0) == 0x80)
        {
            fprintf (stderr, "WARNING: utf-8 byte 1 starting in 10?????? !\n");
        }

        *out = first;
        return pBytes + 1;
    }

    // Complement the unicode identifier from the remaining encoding bits.
    for (i = 1; i < n_bytes; i++)
    {
        if ((pBytes [i] & 0xc0) != 0x80)
        {
            fprintf (stderr, "WARNING: utf-8 byte %d not starting in 10?????? !\n", i + 1);
        }

        *out = (*out << 6) | (pBytes [i] & 0x3f); // ???????? & 00111111
    }

    return pBytes + n_bytes; // move to the next utf-8 character pointer
}
const char *prev_from_utf8 (const char *pBytes, unicode_char *out)
{
    int n = 1;
    *out = 0;

    while ((*(pBytes - n) & 0xc0) == 0x80) // still 10??????
    {
        *out |= (*(pBytes - n) & 0x3f) << (6 * (n - 1));

        n ++;
        if (n >= 4)
        {
            fprintf (stderr, "WARNING: 10?????? byte sequence of length %d encountered in utf-8!\n", n);
        }
    }

    // Include the first utf-8 byte:
    *out |= (*(pBytes - n) & (0x000000ff >> (n + 1))) << (6 * (n - 1));;

    return pBytes - n;
}
const char *pos_utf8 (const char *pBytes, const std::size_t n)
{
    std::size_t i = 0;
    unicode_char ch;
    while (i < n)
    {
        pBytes = next_from_utf8 (pBytes, &ch);
        i ++;
    }
    return pBytes;
}
std::size_t strlen_utf8 (const char *pBytes, const char *end)
{
    std::size_t n = 0;
    unicode_char ch;
    while (*pBytes)
    {
        pBytes = next_from_utf8 (pBytes, &ch);
        n ++;
        if (end and pBytes >= end)
            return n;
    }
    return n;
}
const char *ParseFloat(const char *in, float *out)
{
    float f = 10.0f;
    int digit, ndigit = 0;

    // start from zero
    *out = 0;

    const char *p = in;
    while (*p)
    {
        if (isdigit (*p))
        {
            digit = (*p - '0');

            if (f > 1.0f) // left from period
            {
                *out *= f;
                *out += digit;
            }
            else // right from period, decimal
            {
                *out += f * digit;
                f *= 0.1f;
            }
            ndigit++;
        }
        else if (tolower (*p) == 'e')
        {
            // exponent

            // if no digits precede the exponent, assume 1
            if (ndigit <= 0)
                *out = 1.0f;

            p++;
            if (*p == '+') // '+' just means positive power, default
                p++; // skip it, don't give it to atoi

            int e = atoi (p);

            *out = *out * pow(10, e);

            // read past exponent number
            if (*p == '-')
                p++;

            while (isdigit(*p))
                p++;

            return p;
        }
        else if (*p == '.')
        {
            // expect decimal digits after this

            f = 0.1f;
        }
        else if (*p == '-')
        {
            // negative number

            float v;
            p = ParseFloat(p + 1, &v);

            *out = -v;

            return p;
        }
        else
        {
            // To assume success, must have read at least one digit

            if (ndigit > 0)
                return p;
            else
                return NULL;
        }
        p++;
    }

    return p;
}
bool isnewline (const int c)
{
    return (c=='\n' || c=='\r');
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
    // Start on the back and move left:

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
