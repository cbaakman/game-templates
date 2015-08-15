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


#ifndef ACCOUNT_H
#define ACCOUNT_H

#define USERNAME_MAXLENGTH 14
#define PASSWORD_MAXLENGTH 18

/*
    For using these functions in an application,
    it's necessary that dirPath points to a valid
    directory where account files are to be stored.
 */

/**
 * :returns: true on success, false otherwise.
 */
bool MakeAccount (const char* dirPath, const char* username, const char* password);
void delAccount (const char* dirPath, const char* username);
bool authenticate (const char* dirPath, const char* username, const char* password);

#endif // ACCOUNT_H
