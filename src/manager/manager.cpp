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
#include <cstring>
#include <cctype>

#define ACCOUNT_DIR "accounts"

#include "../account.h" // MakeAccount

#include <string>
#include <curses.h>
#include "../str.h"

void PromptUsername (char *username, const int maxLength)
{
    // pretty basic, no input masking needed

    printw ("Enter a user name (max %d characters): ", maxLength - 1);

    getnstr (username, maxLength);
}
void PromptPassword (char *password, const int maxLength)
{
    int i = 0;
    unsigned char ch = 0;

    printw ("Enter a password (max %d characters): ", maxLength - 1);

    noecho (); // Disable echoing

    while (true)
    {
        ch=getch();
        if (ch == 10 || ch == 13) // return
        {
            password [i] = NULL;
            printw ("\n");

            echo(); // enable character echoing again
            return;
        }

        if (ch == 8 || ch == 127) // backspace
        {
            if (i > 0)
            {
                printw ("\b \b");
                i--;
            }
         }
        else if(ch != 27) // ignore 'escape' key
        {
            if ((i + 1) < maxLength)
            {
                password [i] = ch;
                i++;
                printw ("*");
            }
        }
    }
}
void OnMakeAccount (const std::string &exe_dir)
{
    char    username [USERNAME_MAXLENGTH],
            password [PASSWORD_MAXLENGTH],
            dirPath [FILENAME_MAX];

    bool usernameError;
    int i;
    do // keep asking for a user name until a correct input is recieved
    {
        usernameError = false;
        PromptUsername (username, USERNAME_MAXLENGTH);
        for (i=0; username[i]; i++)
        {
            if (!isalpha (username[i]))
            {
                printw ("\nerror, only letters allowed!\n");
                usernameError = true;
                break;
            }
        }
    }
    while(usernameError);

    PromptPassword (password, PASSWORD_MAXLENGTH);

    // Create an account file for the server to read
    if (MakeAccount ((exe_dir + ACCOUNT_DIR).c_str(), username, password))
    {
        printw ("created file successfully\n");
    }
    else
    {
        printw ("%s\n", GetAccountError ());
    }
}
bool OnCommand(const char* cmd, const std::string &exe_dir)
{
    if (strcmp (cmd, "quit") == 0)
    {
        return false;
    }
    else if (strcmp (cmd, "make-account")==0)
    {
        OnMakeAccount (exe_dir);
    }
    else if (emptyline (cmd) || strcmp (cmd,"help") == 0)
    {
        printw ("quit: exit application\n");
        printw ("make-account: create new account file\n");
        printw ("help: print this info\n");
    }
    else
    {
        printw ("unrecognized command: %s\n", cmd);
    }
    return true;
}
int main (int argc, char** argv)
{
    initscr (); // enable ncurses

    const char *x = strrchr (argv[0], PATH_SEPARATOR);
    std::string exe_dir = "." + PATH_SEPARATOR;
    if (x)
        exe_dir = std::string (argv[0], 1 + x - argv[0]);

    const int cmdlen = 256;
    int n;
    char cmd [cmdlen];
    bool running = true;
    while (running)
    {
        printw ("manager>");

        getnstr (cmd, cmdlen);
        stripr (cmd);

        // take input commands
        running = OnCommand(cmd, exe_dir);
    }

    endwin (); // disable ncurses
    return 0;
}
