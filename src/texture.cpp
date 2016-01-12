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


#include "texture.h"

#include "err.h"

#define PNG_ERROR 1

void PNGReadCallback (png_structp png_ptr, png_bytep data, png_size_t length)
{
    /*
        Image data comes from an SDL_RWops input stream.
        This callback function gets the requested bytes from it.
     */

    png_voidp pio = png_get_io_ptr (png_ptr);
    SDL_RWops *io = (SDL_RWops *)pio;

    size_t n;
    if ((n = io->read (io, data, 1, length)) < length)
    {
        SetError ("%d bytes requested, only %d read", length, n);

        longjmp (png_jmpbuf (png_ptr), PNG_ERROR);
    }
}
void PNGErrorCallback(png_structp png_ptr, png_const_charp msg)
{
    /*
       PNG errors must be copied to the error string,
       not just stderr.
     */

    SetError ("Error from png: %s", (const char *)msg);

    longjmp (png_jmpbuf (png_ptr), PNG_ERROR);
}
void PNGWarningCallback(png_structp png_ptr, png_const_charp msg)
{
    // Not decided what to do with warnings yet.
}
bool LoadPNG (SDL_RWops *io, Texture *pTex)
{
    /*
        We use libpng to read in a png file.

        For further information:
        http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
     */

    png_info* info_ptr = 0, *end_info = 0;
    png_struct* png_ptr = 0;
    png_bytep *row_pointers = 0;
    png_bytep png_data = 0;
    png_uint_32 rowbytes;
    int number_of_passes;
    png_byte num_channels;

    // make a read structure:
    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, (png_voidp)NULL, NULL, NULL);
    if (!png_ptr)
    {
        SetError ("png: error creating read struct");
        goto pngload_failure;
    }

    // info and end_info structures are also neccesary:
    info_ptr=png_create_info_struct (png_ptr);
    if (!info_ptr)
    {
        SetError ("png: error creating info struct");
        goto pngload_failure;
    }
    end_info = png_create_info_struct (png_ptr);
    if (!end_info)
    {
        SetError ("png: error creating end info");
        goto pngload_failure;
    }

    // Catch libPNG's errors and warnings:
    png_set_error_fn (png_ptr, png_get_error_ptr (png_ptr), PNGErrorCallback, PNGWarningCallback);

    // if an error occurs, jump to the failure section:
    if (setjmp (png_jmpbuf (png_ptr)))
    {
        goto pngload_failure;
    }

    // Tell libpng that it must take its data from a callback function:
    png_set_read_fn (png_ptr, (png_voidp *)io, PNGReadCallback);

    // read info from the header:
    png_read_info (png_ptr, info_ptr);

    // Get info about the data in the image:
    png_uint_32 png_width, png_height;
    int bit_depth,
        color_type,
        interlace_type;

    png_get_IHDR (png_ptr, info_ptr, &png_width, &png_height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

    // how many channels does the image have?
    num_channels = png_get_channels(png_ptr, info_ptr);

    // We want RGB colors, no palette:
    if (color_type == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb (png_ptr);
        color_type = PNG_COLOR_TYPE_RGB;
    }

    // make sure the pixel depth is 8:
    if (bit_depth < 8)
    {
        png_set_packing (png_ptr);
        bit_depth = 8;
    }
    else if (bit_depth == 16)
    {
        png_set_strip_16 (png_ptr);
        bit_depth = 8;
    }

    // Make sure the transparency is set to an alpha channel, not tRNS:
    if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png_ptr);

    // Handle expansion of interlaced image:
    number_of_passes = png_set_interlace_handling (png_ptr);

    // Update  png_info structure after setting transformations:
    png_read_update_info (png_ptr, info_ptr);

    // get bytes per row:
    rowbytes = png_get_rowbytes (png_ptr, info_ptr);

    // allocate memory to store pixel data in:
    png_data = (png_bytep) png_malloc (png_ptr, png_height * rowbytes);
    if (!png_data)
    {
        SetError ("png_malloc failed");
        goto pngload_failure;
    }

    // make row pointers for reading:
    row_pointers = new png_bytep[png_height];

    // place the row pointers correctly:
    for(png_uint_32 i = 0; i < png_height; i++)
        row_pointers [png_height - 1 - i] = png_data + i * rowbytes;

    png_read_image (png_ptr, row_pointers);

    // Don't need row pointers anymore, png_data has been filled up.
    delete [] row_pointers;

    // read the end of the file:
    png_read_end (png_ptr, end_info);

    if (png_data) // did we get data?
    {
        /*
         * Here, we must tell OpenGL what type
         * of pixels are stored in the data.
         */
        GLenum format;
        switch (color_type)
        {
        case PNG_COLOR_TYPE_GRAY:
            format = GL_LUMINANCE8;
            break;
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            format = GL_LUMINANCE8_ALPHA8;
            break;
        case PNG_COLOR_TYPE_RGB:
            format = GL_RGB;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            format = GL_RGBA;
            break;
        }

        // Create an OpenGL texture:
        GLuint tex;
        glGenTextures (1, &tex);
        if (!tex)
        {
            SetError ("Error generating GL texture for png");
            goto pngload_failure;
        }

        glBindTexture (GL_TEXTURE_2D, tex);

        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

        glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Make OpenGL fill the texture:
        gluBuild2DMipmaps ( GL_TEXTURE_2D, format, (GLsizei)png_width, (GLsizei)png_height, format, GL_UNSIGNED_BYTE, png_data);
        //glTexImage2D( GL_TEXTURE_2D, 0, format, (GLsizei)png_width, (GLsizei)png_height, 0, format, GL_UNSIGNED_BYTE, data);

        pTex->tex = tex;
        pTex->w = (GLsizei)png_width;
        pTex->h = (GLsizei)png_height;

        png_free(png_ptr, png_data);
        png_data = 0;

        // clean up (also frees whatever was allocated with 'png_malloc'):
        png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
        png_ptr = 0;
        info_ptr = 0;
        end_info = 0;

        glBindTexture (GL_TEXTURE_2D, 0);
    }
    else goto pngload_failure;

    return true; // report succesful loading

pngload_failure: // things to do when an error occurs during reading:

    if (png_data)
        png_free (png_ptr, png_data);

    delete [] row_pointers; // free allocated row-pointers, if any
    png_destroy_read_struct (&png_ptr, &info_ptr, &end_info); // clean up the read structure, info and end_info

    return false;
}

#include "io.h"
bool LoadPNG (const char *zip_path,
              const char *png_path,
              Texture *pTex)
{
    SDL_RWops *f = SDL_RWFromZipArchive (zip_path, png_path);
    if (!f) // file or archive missing
        return false;

    bool success = LoadPNG (f, pTex);
    f->close (f);

    if (!success)
    {
        SetError ("error loading PNG image %s: %s", png_path, GetError ());
        return false;
    }

    return true;
}
bool LoadPNG (const std::string &zip_path,
              const std::string &png_path,
              Texture *pTex)
{
    return LoadPNG (zip_path.c_str (), png_path.c_str (), pTex);
}

LoadFunc LoadPNGFunc (const char *zip_path,
                      const char *png_path,
                      Texture *pTex)
{
    return LoadPNGFunc (std::string (zip_path), std::string (png_path), pTex);
}
LoadFunc LoadPNGFunc (const std::string &zip_path,
                      const std::string &png_path,
                      Texture *pTex)
{
    // Load func is asynchronous, so pass pathnames by value:
    return [zip_path, png_path, pTex] ()
    {
        return LoadPNG (zip_path, png_path, pTex);
    };
}
