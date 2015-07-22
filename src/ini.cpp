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

#include "ini.h"
#include "err.h"
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>

bool LoadSettingString(const char* filename, const char* setting, char* val)
{
    char var[100], c;
    int i;
    bool match=false;

    FILE* f = fopen (filename,"r");
    if (!f)
        return false;

    while(!feof(f))
    {
        c = fgetc (f);
        while (!feof(f) && isspace(c))
            c = fgetc (f);

        i = 0;
        while (!feof (f) && c != '=' && !isspace(c))
        {
            var [i] = c;
            i++;
            c = fgetc (f);
        }
        var[i] = NULL;

        while (!feof (f) && isspace (c))
            c = fgetc (f);

        if (c == '=' && fscanf (f, "%s", val) == 1)
        {
            if (strcmp (var, setting) == 0)
            {
                match = true;
                break;
            }
        }
        else if (feof (f))
            break;
        else
            continue;
    }
    if (!match)
        val [0] = NULL;

    fclose (f);

    return match;
}
void SaveSettingString(const char* filename, const char* setting, const char* value)
{
    unsigned long long size=0;
    char    buf[1000],
            var[100],
            val[100],
            c;
    int i;
    bool set=false;

    FILE* f = fopen(filename,"r");
    if(!f) return;

    while(!feof(f))
    {
        c=fgetc(f); while( !feof(f) && isspace(c) ) c=fgetc(f);

        i=0; while( !feof(f) && c!='=' && !isspace(c) ) { var[i]=c; i++; c=fgetc(f); } var[i]=NULL;
        while( !feof(f) && isspace(c)) c=fgetc(f);

        if( c=='=' && fscanf(f,"%s",val)==1 )
        {
            if(strcmp(var,setting)==0)
            {
                strcpy(val,value);
                set=true;
            }

            size+=sprintf(buf+size,"%s=%s\n",var,val);
        }
        else if(feof(f))    break;
        else                continue;
    }
    fclose(f);

    if(!set) size+=sprintf(buf+size,"%s=%s\n",setting,value);

    f=fopen(filename,"w");
    if(!f) return;
    fwrite(buf,size,1,f);
    fclose(f);

}
int LoadSetting(const char* filename, const char* setting)
{
    char value [100];
    LoadSettingString (filename, setting, value);
    return atoi (value);
}
void SaveSetting(const char* filename, const char* setting, const int value)
{
    char val[100];
    sprintf(val,"%d",value);
    SaveSettingString(filename,setting,val);
}
