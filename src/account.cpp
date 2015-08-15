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
#include <openssl/sha.h>
#include <cctype>
#include <cstring>
#include <string>

#include "account.h"
#include "str.h"
#include "err.h"
#include <errno.h>

#define HASHSTRING_LENGTH 20
void getHash(const char* username, const char* password, unsigned char* hash)
{
    /*
        This function is used for checking passwords.
        If it changes, all account files must be created anew.
     */

    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx,(void*)username,strlen(username));
    SHA1_Update(&ctx,(void*)"wfuihf23780vsb",9);
    SHA1_Update(&ctx,(void*)password,strlen(password));
    SHA1_Final(hash,&ctx);
}

void AccountFileName (const char *directory, const char *username, char *out)
{
    // <username>.account

    sprintf(out,"%s%c%s.account", directory, PATH_SEPARATOR, username);
}

#define LINEID_ACCOUNT "ACCOUNT"
bool MakeAccount (const char* dirPath, const char* username, const char* password)
{
    char    filepath[FILENAME_MAX],
            _username[USERNAME_MAXLENGTH],
            _password[PASSWORD_MAXLENGTH];
    unsigned char hash[HASHSTRING_LENGTH];

    /*
        username is case insensitive, thus always converted to lowercase
        password is case sensitive
     */
    int i;
    for (i=0;username[i];i++)
        _username[i]=tolower(username[i]);
    _username[i]=NULL;

    strcpy (_password, password);

    // Open the account file
    AccountFileName (dirPath, _username, filepath);

    FILE* f = fopen (filepath, "wb");
    if (!f)
    {
        std::string _errorstring = std::string ("cannot open output file ") + filepath + ": " + strerror (errno);
        SetError (_errorstring.c_str ());
        return false;
    }

    /*
        Write a single line, including
        the username and password hash.
     */

    getHash (_username, _password, hash);
    fprintf (f, "%s %s ", LINEID_ACCOUNT, _username);

    // write each hash byte
    for (i=0; i<HASHSTRING_LENGTH; i++)
    {
        fprintf (f, "%.2X", hash[i]);
    }

    fclose(f);

    return true;
}
void delAccount(const char* dirPath, const char* username)
{
    /*
        username is case insensitive, thus always converted to lowercase
     */

    char    filepath[FILENAME_MAX],
            _username[USERNAME_MAXLENGTH];
    int i;
    for(i=0;username[i];i++)
        _username[i]=tolower(username[i]);
    _username[i]=NULL;

    // Remove the file, associated with this username

    AccountFileName (dirPath, _username, filepath);

    remove (filepath);
}
bool authenticate(const char* dirPath, const char* username, const char* password)
{
    const int lineLength=128;
    char    filepath [FILENAME_MAX],
            line [lineLength],
            _username [USERNAME_MAXLENGTH],
            _password [PASSWORD_MAXLENGTH];

    unsigned char b;
    unsigned char hash [HASHSTRING_LENGTH];

    /*
        username is case insensitive, thus always converted to lowercase
        password is case sensitive
     */
    int i;
    for (i = 0; username[i] && i < (USERNAME_MAXLENGTH - 1); i++)
        _username[i] = tolower (username[i]);
    _username[i]=NULL;

    strcpy (_password, password);

    getHash (_username, _password, hash);

    AccountFileName (dirPath, _username, filepath);

    FILE* f = fopen (filepath, "rb");
    if (!f) // the user probably doesn't exist
    {
        return false;
    }

    while (!feof(f))
    {
        fgets (line, lineLength, f);

        // Check for the recognition bytes at the beginning of the file:
        if (strncmp (LINEID_ACCOUNT, line, strlen (LINEID_ACCOUNT)) == 0)
        {
            int i = 0, n, j;

            // Read past the recognition bytes and the spaces after it.
            while (line[i] && !isspace (line[i])) i++;
            while (line[i] && isspace (line[i])) i++;

            // Read all the non-whitespace characters, presumed to be username
            n = i;
            while (line[n] && !isspace (line[n])) n++;

            if (n == i) // no more non-whitespace characters found
                return false;

            // Verify the username in the file:
            if (strncmp (_username, line + i, n - i) != 0)
                return false;

            // Read the spaces past the username
            i = n;
            while (line[i] && isspace (line [i])) i++;

            // compare each hash byte:
            for (j = 0; j < HASHSTRING_LENGTH; j++)
            {
                // Read in the hash byte:
                if (sscanf(line + i + j * 2, "%2X", &b) != 1)
                {
                    fprintf (stderr, "%s: cannot read byte %d of hash\n", filepath, j);
                    return false;
                }

                if(b != hash[j])
                {
                    // mismatch in hash

                    return false;
                }
            }
            // went through the whole hash without mismatches
            return true;
        }
    }

    fclose(f);

    return false;
}
