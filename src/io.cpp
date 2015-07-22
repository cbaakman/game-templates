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


#include "io.h"

#include "err.h"

#include <stdio.h>

size_t NoWriteCallBack(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    return 0;
}

Sint64 NoSeekCallBack(SDL_RWops *context, Sint64 offset, int whence)
{
    return -1;
}
Sint64 ZipArchiveSizeCallBack(SDL_RWops *context)
{
    unzFile zip = (unzFile)context->hidden.unknown.data1;

    unz_file_info info;

    int res = unzGetCurrentFileInfo (zip, &info, NULL, 0, NULL, 0, NULL, 0);
    if (res != UNZ_OK)
    {
        SetError ("unzip: error getting current file info");
        return -1;
    }

    return info.uncompressed_size;
}

size_t ZipArchiveReadCallBack(SDL_RWops *context, void *data, size_t size, size_t maxnum)
{
    unzFile zip = (unzFile)context->hidden.unknown.data1;

    int res = unzReadCurrentFile (zip, data, size*maxnum);

    return res / size;
}
int ZipArchiveCloseCallBack (SDL_RWops *context)
{
    unzFile zip = (unzFile)context->hidden.unknown.data1;

    unzCloseCurrentFile (zip);
    unzClose (zip);

    return 0;
}
SDL_RWops *SDL_RWFromZipArchive (const char *_archive, const char *_entry)
{
    SDL_RWops *c=SDL_AllocRW();
    if (!c)
        return NULL;


    unzFile zip = unzOpen(_archive);
    if (!zip)
    {
        SetError ("failed to open %s", _archive);
        return NULL;
    }

    int result = unzLocateFile (zip, _entry, 1);

    if (result != UNZ_OK)
    {
        SetError ("not found in archive: %s", _entry);
        unzClose (zip);

        return NULL;
    }

    unzOpenCurrentFile (zip);

    c->size = ZipArchiveSizeCallBack;
    c->seek = NoSeekCallBack;
    c->read = ZipArchiveReadCallBack;
    c->write = NoWriteCallBack;
    c->close = ZipArchiveCloseCallBack;
    c->type = SDL_RWOPS_UNKNOWN;
    c->hidden.unknown.data1 = (voidp)zip;

    return c;
}
