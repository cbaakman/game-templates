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


#ifndef FONT_H
#define FONT_H

#include <GL/glew.h>
#include <GL/gl.h>

#include <map>
#include <libxml/tree.h>

#define TEXTALIGN_LEFT 0x00
#define TEXTALIGN_MID 0x03
#define TEXTALIGN_RIGHT 0x06

typedef int unicode_char;

struct Glyph {

    unicode_char ch;
    GLuint tex;
    GLsizei tex_h, tex_w;
    float horiz_origin_x, horiz_origin_y,
          horiz_adv_x;
};

struct BBox {
    float left, bottom, right, top;
};

struct Font {

    float size,
          horiz_origin_x, horiz_origin_y,
          horiz_adv_x;

    BBox bbox;

    std::map<unicode_char, Glyph> glyphs;
    std::map<unicode_char, std::map<unicode_char, float> > hKernTable;
};

float GetLineSpacing (const Font *pFont);

bool ParseSVGFont (const xmlDocPtr pDoc, const int size, Font *pFont);

/*
 * Renders with origin point (0,0,0). Use transforms to render text at a different position.
 * If maxWidth is set, then line breaks are added between some of the words so that the lines don't get wider.
 */
void glRenderText (const Font *pFont, const char *pUTF8, const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/*
 * This is handy for text selections:
 */
void glRenderTextAsRects (const Font *pFont, const char *pUTF8, const int from, const int to, const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/*
 * px and py must be relative to origin point of the text.
 * if (px,py) lies within a glyph's box, returns its index position.
 * if (px,py) lies on the same line, but hits no glyph, returns either the rightmost or leftmost index position.
 * If (px,py) is not in line, returns -1
 */
int WhichGlyphAt (const Font *pFont, const char *pUTF8,
                  const float px, const float py,
                  const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/*
 * Returns coordinates relative to origin point of text.
 */
void CoordsOfGlyph (const Font *pFont, const char *pUTF8, const int pos,
                         float &outX, float &outY,
                         const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

void DimensionsOfText (const Font *pFont, const char *pUTF8,
                       float &outX1, float &outY1, float &outX2, float &outY2,
                       const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

#endif // FONT_H
