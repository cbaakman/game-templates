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

typedef int unicode_char; // these codes are backbards compatible with ascii chars

struct Glyph { /* these objects are mapped one-to-one with character codes */

    // (stored for convenience)
    unicode_char ch;

    // Texture that contains the glyph's image
    GLuint tex;
    GLsizei tex_h, tex_w;

    /*
        horiz_origin_x, horiz_origin_y & horiz_adv_x may be negative,
        in which case they are ignored and the font's default is used:
     */
    float horiz_origin_x, horiz_origin_y,
          horiz_adv_x;
};

struct BBox { /* all the font's glyphs should fit inside this */

    float left, bottom, right, top; // parse order
};

struct Font {

    float size, // ems per unit
          horiz_origin_x, horiz_origin_y,
          horiz_adv_x;

    BBox bbox;

    // One glyph per character code:
    std::map <unicode_char, Glyph> glyphs;

    // One kern value per character pair:
    std::map <unicode_char, std::map<unicode_char, float> > hKernTable;
};

/**
 *  This takes the values from an svg document and creates a font with them:
 *
 * :param size: the desired ems per unit
 * :param pDoc: input svg document
 * :param pFont: font object to store data in
 *
 * :returns: true is successful, falso on error
 */
bool ParseSVGFont (const xmlDocPtr pDoc, const int size, Font *pFont);

// This gives the distance between the bottoms of two successive lines of text:
float GetLineSpacing (const Font *pFont);

/*
    In the text rendering and sizing functions I chose to use relative coordinates.
    Thus, all X and Y values are based on the assumption that the first glyph starts at (0,0).
    This reduces the number of function arguments needed.

    To place text at different locations than (0,0),
    one must transform the coordinates.

    align and maxWidth may be set, but have default values also.
    If maxWidth is negative, there is assumed to be no maximum text width.
 */

/**
 * Renders with origin point (0,0,0). Use transforms to render text at a different position.
 * If maxWidth is set, then line breaks are added between some of the words so that the lines don't get wider.
 */
void glRenderText (const Font *pFont, const char *pUTF8, const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/**
 * glRenderTextAsRects is handy for text selections.
 *
 * :param from: character index to start rendering from
 * :param to: character index where rendering stops
 */
void glRenderTextAsRects (const Font *pFont, const char *pUTF8, const int from, const int to, const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/**
 * Tells which glyph is at the requested point.
 *
 * :param px, py: Requested point's coordinates, relative to origin point of the text.
 *
 * if (px,py) lies within a glyph's box, returns its index position.
 * if (px,py) lies on the same line, but hits no glyph, returns either the rightmost or leftmost index position.
 * If (px,py) is not in line, returns -1
 */
int WhichGlyphAt (const Font *pFont, const char *pUTF8,
                  const float px, const float py,
                  const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

/**
 * Tells coordinates of a glyph relative to origin point of text.
 *
 * :param pos: charater index, whose glyph we want the coordinates from.
 * :param outX, outY: output coordinates of the requested glyph
 */
void CoordsOfGlyph (const Font *pFont, const char *pUTF8, const int pos,
                         float &outX, float &outY,
                         const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);
/**
 * Tells how big the text is, by giving back a bounding box.
 * Returned bounding box area is relative to the origin point of the text.
 *
 * :param outX1, outY1, outX2, outY2: output relative bounding box.
 */
void DimensionsOfText (const Font *pFont, const char *pUTF8,
                       float &outX1, float &outY1, float &outX2, float &outY2,
                       const int align = TEXTALIGN_LEFT, float maxWidth = -1.0f);

#endif // FONT_H
