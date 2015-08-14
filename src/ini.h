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


#ifndef INI_H
#define INI_H

/*
 Syntax of settings files:
    <setting1> = <value1>
    <setting2> = <value2>
    ...

 Whitespaces are ignored.
 Settings can be either loaded / saved as string or integer value.
 Use 0/1 integers for booleans.
 */

int LoadSetting (const char *filename, const char *setting);
bool LoadSettingString (const char *filename, const char *setting, char *value);
void SaveSetting (const char *filename, const char *setting, const int value);
void SaveSettingString (const char *filename, const char *setting, const char *value);

#endif // INI_H
