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


#ifndef STR_H
#define STR_H

#include <string>
#include <list>

/*
    Here, the PATH_SEPARATOR macro is used
    to join paths. Could also use boost for this:

    http://www.boost.org/doc/libs/1_53_0/libs/filesystem/doc/reference.html
 */
#ifdef _WIN32
    #define PATH_SEPARATOR '\\'
#else
    #define PATH_SEPARATOR '/'
#endif

bool isnewline (const int c);
bool emptyline (const char *line);

/**
 * Places a NULL character so that trailing whitespaces
 * are removed.
 */
void stripr (char *s);

/**
 * atof and scanf are locale dependent. ParseFloat isn't,
 * decimals always assumed to have dots here, never commas.
 *
 * :returns: the string after the parsed number on success,
 *           NULL on failure.
 */
const char *ParseFloat(const char *in, float *out);

/**
 * Like strcmp, but ignores case.
 */
int StrCaseCompare(const char *s1, const char *s2);

/**
 * splits a string by given character.
 */
void split (const char *s, const char by, std::list<std::string> &out);

/**
 * Gives the start and end position of the word at pos in the string.
 */
void WordAt (const char *s, const int pos, int &start, int &end);

#endif // STR_H
