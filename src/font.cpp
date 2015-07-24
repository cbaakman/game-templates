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


#include "font.h"
#include "str.h"
#include "util.h"
#include "err.h"

#include <string>
#include <cairo/cairo.h>
#include <cctype>
#include <math.h>
#include <algorithm>

const char *utf8_next(const char *pString, unicode_char *out)
{
    int take;
    if (pString[0] >> 5 == 0x6)
    {
        take = 2;
    }
    else if (pString[0] >> 4 == 0xe)
    {
        take = 3;
    }
    else if (pString[0] >> 3 == 0x1e)
    {
        take = 4;
    }
    else // assume ascii
    {
        take = 1;
    }

    *out = 0;
    for (int i = 0; i < take; i++)
    {
        *out = (*out << 8) | pString[i];
    }
    return pString + take;
}

bool ParseHtmlUnicode(const char *repr, unicode_char *out)
{
    size_t len = strlen(repr);
    if (len == 1) // ascii
    {
        *out = repr[0];
        return true;
    }
    else if (repr[0] == '&' || repr[1] == '#' || repr[len - 1] == ';' ) // html code
    {
        if (repr[2] == 'x') // hexadecimal

            sscanf (repr + 3, "%x;", out);

        else // decimal

            sscanf (repr + 2, "%d;", out);

        return true;
    }
    return false;
}

const char *SVGParsePathFloats (const int n, const char *text, float outs [])
{
    const char *tmp = text;

    int i;
    for (i = 0; i < n; i++)
    {
        if (!*text)
            return NULL;

        while (isspace (*text) || *text == ',')
        {
            text++;
            if (!*text)
                return NULL;
        }

        text = ParseFloat(text, &outs[i]);
        if (!text)
            return NULL;
    }

    return text;
}
/*
 * The following chunk of code, used to draw quadratic curves in cairo, was
 * borrowed from cairosvg (http://cairosvg.org/)
 */
void Quadratic2Bezier (float *px1, float *py1, float *px2, float *py2, const float x, const float y)
{
    float xq1 = (*px2) * 2 / 3 + (*px1) / 3,
          yq1 = (*py2) * 2 / 3 + (*py1) / 3,
          xq2 = (*px2) * 2 / 3 + x / 3,
          yq2 = (*py2) * 2 / 3 + y / 3;

    *px1 = xq1;
    *py1 = yq1;
    *px2 = xq2;
    *py2 = yq2;
}
bool Cairo2GLTex (cairo_surface_t *surface, GLuint *pTex)
{
    char errorString[128];
    cairo_status_t status;

    cairo_format_t format = cairo_image_surface_get_format (surface);
    if (format != CAIRO_FORMAT_ARGB32)
    {
        SetError ("unsupported format detected on cairo surface, only CAIRO_FORMAT_ARGB32 is suppported");
        return false;
    }

    int w = cairo_image_surface_get_width (surface),
        h = cairo_image_surface_get_height (surface);

    unsigned char *pData = cairo_image_surface_get_data (surface);

    status = cairo_surface_status (surface);
    if (status != CAIRO_STATUS_SUCCESS)
    {
        SetError ("error accessing cairo surface data: %s", cairo_status_to_string(status));
        return false;
    }

    if (!pData)
    {
        SetError ("cairo_image_surface_get_data returned a NULL pointer");
        return false;
    }

    glGenTextures(1, pTex);
    if(!*pTex)
    {
        SetError ("Error generating GL texture for glyph");

        return false;
    }

    glBindTexture(GL_TEXTURE_2D, *pTex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    /*
     * Turn transparent black into transparent white.
     * Looks prettier!
     */
    for (int y = 0; y < h; y++)
    {
        unsigned char *pRow = pData + y * w * 4;
        for (int x = 0; x < w; x++)
        {
            unsigned char *pPixel = pRow + x * 4;

            pPixel [0] = 255;
            pPixel [1] = 255;
            pPixel [2] = 255;
        }
    }
    cairo_surface_mark_dirty (surface);

    // fill the texture:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 (GLsizei)w, (GLsizei)h,
                 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pData);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

/*
 * The following chunk of code, used for drawing an svg arc in cairo, was
 * borrowed from cairosvg (http://cairosvg.org/)
 */
void cairo_svg_arc (cairo_t *cr, float current_x, float current_y,
                    float rx, float ry,
                    const float x_axis_rotation,
                    const bool large_arc, const bool sweep,
                    float x, float y)
{
    if (rx == 0 || ry == 0) // means straight line
    {
        cairo_line_to (cr, x, y);
        return;
    }

    /* if (print)
        printf ("at %.1f, %.1f arc %.1f,%.1f %.1f %d %d %.1f,%.1f\n", current_x, current_y, rx, ry, x_axis_rotation, large_arc, sweep, x,y); */

    float radii_ratio = ry / rx,

          dx = x - current_x,
          dy = y - current_y,

          // Transform the target point from elipse to circle
          xe = dx * cos (-x_axis_rotation) - dy * sin (-x_axis_rotation),
          ye =(dy * cos (-x_axis_rotation) + dx * sin (-x_axis_rotation)) / radii_ratio,

          // angle between the line from current to target point and the x axis
          angle = atan2 (ye, xe);

    // Move the target point onto the x axis
    // The current point was already on the x-axis
    xe = sqrt(sqr(xe) + sqr(ye));
    ye = 0.0f;

    // Update the first radius if it is too small
    rx = std::max(rx, xe / 2);

    // Find one circle centre
    float xc = xe / 2,
          yc = sqrt(sqr(rx) - sqr(xc));

    // fix for a glitch, appearing on some machines:
    if (rx == xc)
        yc = 0.0f;

    // Use the flags to pick a circle center
    if (!(large_arc != sweep))
        yc = -yc;

    // Rotate the target point and the center back to their original circle positions

    float sin_angle = sin (angle),
          cos_angle = cos (angle);

    ye = xe * sin_angle;
    xe = xe * cos_angle;

    float ax = xc * cos_angle - yc * sin_angle,
          ay = yc * cos_angle + xc * sin_angle;
    xc = ax;
    yc = ay;

    // Find the drawing angles, from center to current and target points on circle:
    float angle1 = atan2 (0.0f - yc, 0.0f - xc), // current is shifted to 0,0
          angle2 = atan2 (  ye - yc,   xe - xc);

    cairo_save (cr);
    cairo_translate (cr, current_x, current_y);
    cairo_rotate (cr, x_axis_rotation);
    cairo_scale (cr, 1.0f, radii_ratio);

    if (sweep)
        cairo_arc (cr, xc, yc, rx, angle1, angle2);
    else
        cairo_arc_negative (cr, xc, yc, rx, angle1, angle2);

    cairo_restore (cr);

    //cairo_line_to (cr, x, y);

    /*if (print)
        printf ("%c %c from 0,0 around (%.1f, %.1f) to %.1f,%.1f with r=%.1f angle from %.1f to %.1f\n",
            sweep?'+':'-', large_arc? '>':'<', xc, yc, xe, ye, rx, angle1 * (180/M_PI), angle2 * (180/M_PI)); */
}

#define GLYPH_BBOX_MARGE 10.0f

bool ParseGlyphPath (const char *d, const BBox *pBox, const float multiply, Glyph *pGlyph)
{
    const char *nd, *sd = d;
    cairo_status_t status;
    cairo_surface_t *surface;
    cairo_t *cr;

    /* Cairo surfaces and OpenGL textures have integer dimensions, but
     * glyph bouding boxes consist floating points. Make sure the bounding box fits onto the texture:
     */
    int tex_w = (int) ceil (pBox->right - pBox->left),
        tex_h = (int) ceil (pBox->top - pBox->bottom);

    pGlyph->tex = NULL;

    // bounding box is already multiplied at this point
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tex_w, tex_h);
    status = cairo_surface_status (surface);
    if (status != CAIRO_STATUS_SUCCESS)
    {
        SetError ("error creating cairo surface for glyph: %s", cairo_status_to_string(status));
        return false;
    }

    cr = cairo_create (surface);

    // Set background color to white transparent:
    cairo_rectangle (cr, -GLYPH_BBOX_MARGE, -GLYPH_BBOX_MARGE, tex_w + GLYPH_BBOX_MARGE, tex_h + GLYPH_BBOX_MARGE);
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
    cairo_fill (cr);

    // within the cairo surface, move to the glyph's coordinate system
    cairo_translate(cr, -pBox->left, -pBox->bottom);

    // modify glyph to chosen scale, bounding box is presumed to already be scaled
    cairo_scale (cr, multiply, multiply);

    float f[6],
          x1, y1, x2, y2,
          x = 0, y = 0,
          qx1, qy1, qx2, qy2;

    char prev_symbol, symbol = 'm';
    bool upper = false;

    while (*d)
    {
        prev_symbol = symbol;

        while (isspace (*d))
            d++;

        upper = isupper(*d);
        symbol = tolower(*d);
        d++;

        switch (symbol)
        {
        case 'z':
            cairo_close_path(cr);
            break;

        case 'm':

            while ((nd = SVGParsePathFloats (2, d, f)))
            {
                if (upper)
                {
                    x = f[0];
                    y = f[1];
                }
                else
                {
                    x += f[0];
                    y += f[1];
                }

                cairo_move_to (cr, x, y);

                d = nd;
            }
            break;

        case 'l':

            while ((nd = SVGParsePathFloats (2, d, f)))
            {
                if (upper)
                {
                    x = f[0];
                    y = f[1];
                }
                else
                {
                    x += f[0];
                    y += f[1];
                }

                cairo_line_to (cr, x, y);

                d = nd;
            }
            break;

        case 'h':

            while ((nd = SVGParsePathFloats (1, d, f)))
            {
                if (upper)
                {
                    x = f[0];
                }
                else
                {
                    x += f[0];
                }

                cairo_line_to (cr, x, y);

                d = nd;
            }
            break;

        case 'v':

            while ((nd = SVGParsePathFloats (1, d, f)))
            {
                if (upper)
                {
                    y = f[0];
                }
                else
                {
                    y += f[0];
                }

                cairo_line_to (cr, x, y);

                d = nd;
            }
            break;

        case 'c':

            while ((nd = SVGParsePathFloats (6, d, f)))
            {
                if (upper)
                {
                    x1 = f[0]; y1 = f[1];
                    x2 = f[2]; y2 = f[3];
                    x = f[4]; y = f[5];
                }
                else
                {
                    x1 = x + f[0]; y1 = y + f[1];
                    x2 = x + f[2]; y2 = y + f[3];
                    x += f[4]; y += f[5];
                }

                cairo_curve_to(cr, x1, y1, x2, y2, x, y);

                d = nd;
            }
            break;

        case 's':

            while ((nd = SVGParsePathFloats (4, d, f)))
            {
                if (prev_symbol == 's' || prev_symbol == 'c')
                {
                    x1 = x + (x - x2); y1 = y + (y - y2);
                }
                else
                {
                    x1 = x; y1 = y;
                }
                prev_symbol = symbol;

                if (upper)
                {
                    x2 = f[0]; y2 = f[1];
                    x = f[2]; y = f[3];
                }
                else
                {
                    x2 = x + f[0]; y2 = y + f[1];
                    x += f[2]; y += f[3];
                }

                cairo_curve_to(cr, x1, y1, x2, y2, x, y);

                d = nd;
            }
            break;

        case 'q':

            while ((nd = SVGParsePathFloats (4, d, f)))
            {
                x1 = x; y1 = y;
                if (upper)
                {
                    x2 = f[0]; y2 = f[1];
                    x = f[2]; y = f[3];
                }
                else
                {
                    x2 = x + f[0]; y2 = y + f[1];
                    x += f[2]; y += f[3];
                }

                qx1 = x1; qy1 = y1; qx2 = x2; qy2 = y2;
                Quadratic2Bezier (&qx1, &qy1, &qx2, &qy2, x, y);
                cairo_curve_to (cr, qx1, qy1, qx2, qy2, x, y);

                d = nd;
            }
            break;

        case 't':

            while ((nd = SVGParsePathFloats (2, d, f)))
            {
                x1 = x; y1 = y;
                if (prev_symbol == 't' || prev_symbol == 'q')
                {
                    x2 = x + (x - x2); y2 = y + (y - y2);
                }
                else
                {
                    x2 = x; y2 = y;
                }

                if (upper)
                {
                    x = f[0]; y = f[1];
                }
                else
                {
                    x += f[0]; y += f[1];
                }

                qx1 = x1; qy1 = y1; qx2 = x2; qy2 = y2;
                Quadratic2Bezier (&qx1, &qy1, &qx2, &qy2, x, y);
                cairo_curve_to(cr, qx1, qy1, qx2, qy2, x, y);

                d = nd;
            }
            break;

        case 'a':

            while (isspace(*d))
                d++;

            while (isdigit (*d) || *d == '-')
            {
                nd = SVGParsePathFloats (3, d, f);
                if (!nd)
                {
                    SetError ("arc incomplete. Missing first 3 floats in %s", d);
                    return false;
                }

                float rx = f[0], ry = f[1],
                      a = f[2] * M_PI / 180; // convert degrees to radians

                while (isspace(*nd) || *nd == ',')
                    nd++;

                if (!isdigit(*nd))
                {
                    SetError ("arc incomplete. Missing large arc digit in %s", d);
                    return false;
                }

                bool large_arc = *nd != '0';

                do {
                    nd++;
                } while (isspace(*nd) || *nd == ',');

                if (!isdigit(*nd))
                {
                    SetError ("arc incomplete. Missing sweep digit in %s", d);
                    return false;
                }

                bool sweep = *nd != '0';

                do {
                    nd++;
                } while (isspace(*nd) || *nd == ',');

                nd = SVGParsePathFloats (2, nd, f);
                if (!nd)
                {
                    SetError ("arc incomplete. Missing last two floats in %s", d);
                    return false;
                }

                while (isspace(*nd))
                    nd++;

                d = nd;

                // parsing round done, now draw the elipse

                // starting points:
                x1 = x; y1 = y;

                // end points:
                if (upper)
                {
                    x = f[0]; y = f[1];
                }
                else
                {
                    x += f[0]; y += f[1];
                }

                cairo_svg_arc (cr, x1, y1,
                    rx, ry,
                    a,
                    large_arc, sweep,
                    x, y);

                // cairo_line_to(cr, x, y);

            } // end loop

            break;
        }
    }

    // Fill it in
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
    cairo_fill (cr);
    cairo_surface_flush (surface);

    // Convert the cairo surface to a GL texture:

    bool success = Cairo2GLTex(surface, &pGlyph->tex);

    // Don't need this anymore:
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    pGlyph->tex_w = tex_w;
    pGlyph->tex_h = tex_h;

    return success;
}

bool ParseSvgFontHeader(const xmlNodePtr pFnt, const int size, Font *pFont, float *pMultiply)
{
    xmlChar *pAttrib;
    xmlNodePtr pFace = NULL, pChild;

    for (pChild = pFnt->children; pChild; pChild = pChild -> next)
    {
        if (StrCaseCompare((const char *) pChild->name, "font-face") == 0) {

            pFace = pChild;
            break;
        }
    }
    if (!pFace)
    {
        SetError ("cannot find a font-face tag, it\'s required");
        return false;
    }

    pAttrib = xmlGetProp(pFace, (const xmlChar *)"units-per-em");
    if (!pAttrib)
    {
        SetError ("units-per-em is not set on font-face tag, it\'s required");
        return false;
    }
    int units_per_em = atoi ((const char *)pAttrib);
    xmlFree (pAttrib);

    *pMultiply = float(size) / units_per_em;

    pAttrib = xmlGetProp(pFace, (const xmlChar *)"bbox");
    if (!pAttrib)
    {
        SetError ("bbox is not set on font-face tag, it\'s required");
        return false;
    }

    if (!SVGParsePathFloats (4, (const char *)pAttrib, (float *)&pFont->bbox))
    {
        SetError ("unable to parse 4 bbox numbers from \'%s\'\n", (const char *)pAttrib);
        xmlFree (pAttrib);
        return false;
    }
    xmlFree (pAttrib);

    pFont->bbox.left *= *pMultiply;
    pFont->bbox.bottom *= *pMultiply;
    pFont->bbox.right *= *pMultiply;
    pFont->bbox.top *= *pMultiply;

    pAttrib = xmlGetProp(pFnt, (const xmlChar *)"horiz-adv-x");
    if (pAttrib)
    {
        pFont->horiz_adv_x = *pMultiply * atoi((const char *)pAttrib);
        xmlFree(pAttrib);
    }
    else
        pFont->horiz_adv_x = 0;

    pAttrib = xmlGetProp(pFnt, (const xmlChar *)"horiz-origin-x");
    if (pAttrib)
    {
        pFont->horiz_origin_x = *pMultiply * atoi((const char *)pAttrib);
        xmlFree(pAttrib);
    }
    else
        pFont->horiz_origin_x = 0;

    pAttrib = xmlGetProp(pFnt, (const xmlChar *)"horiz-origin-y");
    if (pAttrib)
    {
        pFont->horiz_origin_y = *pMultiply * atoi((const char *)pAttrib);
        xmlFree(pAttrib);
    }
    else
        pFont->horiz_origin_y = 0;

    return true;
}

bool ParseSVGFont (const xmlDocPtr pDoc, const int size, Font *pFont)
{
    bool success;
    xmlNodePtr pRoot = xmlDocGetRootElement(pDoc), pList, pTag;
    if(!pRoot) {

        SetError ("no root element in svg doc");
        return false;
    }

    if (StrCaseCompare((const char *) pRoot->name, "svg") != 0) {

        SetError ("document of the wrong type, found \'%s\' instead of \'svg\'", pRoot->name);
        return false;
    }

    // Find the defs, containing the font
    xmlNode *pDefs = NULL,
            *pFnt = NULL,
            *pChild = pRoot->children,
            *pGlyphTag = NULL;

    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "defs") == 0) {

            pDefs = pChild;
            break;
        }
        pChild = pChild->next;
    }

    if (!pDefs)
    {
        SetError ("no defs element found in svg doc, expecting font to be there\n");
        return false;
    }

    pChild = pDefs->children;
    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "font") == 0) {

            pFnt = pChild;
            break;
        }
        pChild = pChild->next;
    }

    if (!pFnt)
    {
        SetError ("no font element found in defs section, expecting it to be there\n");
        return false;
    }

    float multiply;
    if (!ParseSvgFontHeader(pFnt, size, pFont, &multiply))
        return false;

    pFont->size = size;

    std::map<std::string, unicode_char> glyph_name_lookup;

    for (pChild = pFnt->children; pChild; pChild = pChild->next)
    {
        if (StrCaseCompare((const char *) pChild->name, "glyph") == 0)
        {
            xmlChar *pAttrib;
            unicode_char ch;

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"unicode");
            if (!pAttrib)
            {
                fprintf (stderr, "warning: encountered a glyph without an unicode id\n");

                // Pretty useless glyph this is :(
                continue;
            }

            if (!ParseHtmlUnicode((const char *)pAttrib, &ch))
            {
                SetError ("failed to interpret unicode id: %s", (const char *)pAttrib);
                xmlFree (pAttrib);
                return false;
            }
            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"glyph-name");
            if (pAttrib)
            {
                std::string name = std::string((const char *)pAttrib);
                xmlFree (pAttrib);

                glyph_name_lookup[name] = ch;
            }

            pFont->glyphs.end();

            pFont->glyphs.find(ch);

            if (pFont->glyphs.find(ch) != pFont->glyphs.end())
            {
                SetError ("error: duplicate unicode id 0x%X", ch);
                return false;
            }

            pFont->glyphs[ch] = Glyph();
            pFont->glyphs[ch].ch = ch;

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"d");

            if (pAttrib)
            {
                success = ParseGlyphPath ((const char *)pAttrib, &pFont->bbox, multiply, &pFont->glyphs[ch]);
            }
            else// path may be an empty string, whitespace for example
            {
                pFont->glyphs[ch].tex = NULL;
                success = true;
            }
            xmlFree (pAttrib);

            if (!success)
            {
                SetError ("failed to parse glyph path");
                return false;
            }

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"horiz-adv-x");
            if (pAttrib)

                pFont->glyphs[ch].horiz_adv_x = atoi ((const char *)pAttrib) * multiply;
            else
                pFont->glyphs[ch].horiz_adv_x = -1.0f;

            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"horiz-origin-x");
            if (pAttrib)

                pFont->glyphs[ch].horiz_origin_x = atoi ((const char *)pAttrib) * multiply;
            else
                pFont->glyphs[ch].horiz_origin_x = -1.0f;

            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"horiz-origin-y");
            if (pAttrib)

                pFont->glyphs[ch].horiz_origin_y = atoi ((const char *)pAttrib) * multiply;
            else
                pFont->glyphs[ch].horiz_origin_y = -1.0f;

            xmlFree (pAttrib);
        }
    }

    // Now look for kerning data
    for (pChild = pFnt->children; pChild; pChild = pChild->next)
    {
        if (StrCaseCompare((const char *) pChild->name, "hkern") == 0)
        {
            xmlChar *pAttrib;
            std::list<std::string> g1, g2, su1, su2;
            std::list<unicode_char> u1, u2;

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"k");
            if (!pAttrib)
            {
               SetError ("One of the hkern tags misses its \'k\' value");
                return false;
            }
            float k = atoi ((const char *)pAttrib) * multiply;
            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"g1");
            if (pAttrib)
                split ((const char *)pAttrib, ',', g1);
            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"u1");
            if (pAttrib)
                split ((const char *)pAttrib, ',', su1);
            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"g2");
            if (pAttrib)
                split ((const char *)pAttrib, ',', g2);
            xmlFree (pAttrib);

            pAttrib = xmlGetProp(pChild, (const xmlChar *)"u2");
            if (pAttrib)
                split ((const char *)pAttrib, ',', su2);
            xmlFree (pAttrib);

            if (g1.size() == 0 && su1.size() == 0)
            {
                SetError ("no first group found on one of the hkern entries");
                return false;
            }

            if (g2.size() == 0 && su2.size() == 0)
            {
                SetError ("no second group found on one of the hkern entries");
                return false;
            }

            for (std::list<std::string>::const_iterator it = g1.begin(); it != g1.end(); it++)
            {
                std::string name = *it;
                if (glyph_name_lookup.find(name) == glyph_name_lookup.end())
                {
                    SetError ("undefined glyph name: %s", name.c_str());
                    return false;
                }
                u1.push_back(glyph_name_lookup.at(name));
            }
            for (std::list<std::string>::const_iterator it = g2.begin(); it != g2.end(); it++)
            {
                std::string name = *it;
                if (glyph_name_lookup.find(name) == glyph_name_lookup.end())
                {
                    SetError ("undefined glyph name: %s", name.c_str());
                    return false;
                }
                u2.push_back(glyph_name_lookup.at(name));
            }
            for (std::list<std::string>::const_iterator it = su1.begin(); it != su1.end(); it++)
            {
                std::string s = *it;
                unicode_char c;

                if (!ParseHtmlUnicode(s.c_str(), &c))
                {
                    SetError ("unparsable unicode char: %s", s.c_str());
                    return false;
                }
                u1.push_back(c);
            }
            for (std::list<std::string>::const_iterator it = su2.begin(); it != su2.end(); it++)
            {
                std::string s = *it;
                unicode_char c;

                if (!ParseHtmlUnicode(s.c_str(), &c))
                {
                    SetError ("unparsable unicode char: %s", s.c_str());
                    return false;
                }
                u2.push_back(c);
            }

            for (std::list<unicode_char>::const_iterator it = u1.begin(); it != u1.end(); it++)
            {
                unicode_char c1 = *it;

                if (pFont->hKernTable.find(c1) == pFont->hKernTable.end())
                {
                    pFont->hKernTable[c1] = std::map<unicode_char, float>();
                }

                for (std::list<unicode_char>::const_iterator jt = u2.begin(); jt != u2.end(); jt++)
                {
                    unicode_char c2 = *jt;

                    pFont->hKernTable[c1][c2] = k;
                }
            }
        }
    }

    return true;
}

void glRenderGlyph (const Font *pFont, const Glyph *pGlyph, const GLfloat x, const GLfloat y)
{
    const BBox *pBox = &pFont->bbox;
    const GLfloat w = pBox->right - pBox->left,
                  h = pBox->top - pBox->bottom,

    // The glyph bouding box might be a little bit smaller than the texture. Use fw and fh to correct it.
                  texcoord_fw = GLfloat (pGlyph->tex_w) / w,
                  texcoord_fh = GLfloat (pGlyph->tex_h) / h;

    glBindTexture(GL_TEXTURE_2D, pGlyph->tex);

    glBegin(GL_QUADS);

    glTexCoord2f (0, texcoord_fh);
    glVertex2f (x, y);

    glTexCoord2f (texcoord_fw, texcoord_fh);
    glVertex2f (x + w, y);

    glTexCoord2f (texcoord_fw, 0);
    glVertex2f (x + w, y + h);

    glTexCoord2f (0, 0);
    glVertex2f (x, y + h);

    glEnd();
}
float GetHKern (const Font *pFont, const unicode_char prev_c, const unicode_char c)
{
    if (prev_c && pFont->hKernTable.find(prev_c) != pFont->hKernTable.end())
    {
        const std::map<unicode_char, float> row = pFont->hKernTable.at(prev_c);
        if (row.find(c) != row.end())
        {
            return row.at(c);
        }
    }
    return 0.0f;
}
float GetHAdv (const Font *pFont, const Glyph *pGlyph)
{
    if (pGlyph && pGlyph->horiz_adv_x > 0)

        return pGlyph->horiz_adv_x;
    else
        return pFont->horiz_adv_x; // default value
}
float NextWordWidth (const Font *pFont, const char *pUTF8, unicode_char prev_c)
{
    float w = 0.0f;
    unicode_char c = prev_c;
    const Glyph *pGlyph;
    const char *nUTF8;

    // First include the spaces before the word
    while (*pUTF8)
    {
        prev_c = c;
        nUTF8 = utf8_next(pUTF8, &c);

        if (isspace (c))
        {
            if (pFont->glyphs.find (c) == pFont->glyphs.end ())
            {
                fprintf (stderr, "error: 0x%X, no such glyph\n", c);
                return 0.0f;
            }

            pGlyph = &pFont->glyphs.at (c);

            w += -GetHKern (pFont, prev_c, c) + GetHAdv (pFont, pGlyph);

            pUTF8 = nUTF8;
        }
        else
            break;
    }

    while (*pUTF8)
    {
        prev_c = c;
        nUTF8 = utf8_next(pUTF8, &c);

        if (isspace (c))
            break;
        else
        {
            if (pFont->glyphs.find (c) == pFont->glyphs.end ())
            {
                fprintf (stderr, "error: 0x%X, no such glyph\n", c);
                return 0.0f;
            }

            pGlyph = &pFont->glyphs.at (c);

            w += -GetHKern (pFont, prev_c, c) + GetHAdv (pFont, pGlyph);

            pUTF8 = nUTF8;
        }
    }

    return w;

    // now read till the next space
}
float GetLineSpacing (const Font *pFont)
{
    return pFont->bbox.top - pFont->bbox.bottom;
}
bool NeedNewLine (const Font *pFont, const unicode_char prev_c, const char *pUTF8, const float current_x, const float maxWidth)
{
    unicode_char c;
    utf8_next(pUTF8, &c);
    const Glyph *pGlyph;
    float adv;

    if (c == '\n' || c == '\r')
        return true;

    if (maxWidth > 0)
    {
        // Maybe the next char starts a new word that doesn't fit onto this line:
        if (isspace(prev_c) && !isspace(c) && current_x > 0 && maxWidth > 0 && maxWidth < (current_x + NextWordWidth (pFont, pUTF8, prev_c)))
            return true;

        if (pFont->glyphs.find(c) == pFont->glyphs.end())
        {
            return false;
        }
        pGlyph = &pFont->glyphs.at (c);

        adv = GetHAdv (pFont, pGlyph);

        if (current_x + adv > maxWidth)
        { // the word is so long that it doesn't fit on one line
            return true;
        }
    }

    return false;
}
const char *CleanLineEnd (const char *pUTF8, int *n_removed = NULL)
{
    // consumes characters that shouldn't be taken to the next line

    unicode_char c;
    const char *nUTF8;

    int n = 0;

    while (true)
    {
        nUTF8 = utf8_next(pUTF8, &c);

        if (c == '\n' || c == '\r')
        {
            if (n_removed)
                *n_removed = n + 1;

            // this will be the last character we take
            return nUTF8;
        }
        else if (isspace (c))
        {
            n++;
            pUTF8 = nUTF8; // go to next
        }
        else // Non-whitespace character, must not be taken away!
        {
            if (n_removed)
                *n_removed = n;

            return pUTF8;
        }
    }
}
void GetGlyphOrigin (const Font *pFont, const Glyph *pGlyph, float &ori_x, float &ori_y)
{
    ori_x = pFont->horiz_origin_x;
    ori_y = pFont->horiz_origin_y;

    if (pGlyph && pGlyph->horiz_origin_x > 0)
        ori_x = pGlyph->horiz_origin_x;
    if (pGlyph && pGlyph->horiz_origin_y > 0)
        ori_y = pGlyph->horiz_origin_y;
}
float NextLineWidth (const Font *pFont, const char *pUTF8, const float maxLineWidth)
{
    float x = 0.0f, w = 0.0f;
    unicode_char c = NULL, prev_c = NULL;
    const Glyph *pGlyph;

    while (*pUTF8)
    {
        prev_c = c;

        if (NeedNewLine (pFont, prev_c, pUTF8, x, maxLineWidth))
        {
            return w;
        }

        pUTF8 = utf8_next (pUTF8, &c);

        if (pFont->glyphs.find(c) == pFont->glyphs.end())
        {
            fprintf (stderr, "error: 0x%X, no such glyph\n", c);
            return 0.0f;
        }
        pGlyph = &pFont->glyphs.at (c);

        if (x > 0.0f)
            x -= GetHKern (pFont, prev_c, c);

        x += GetHAdv (pFont, pGlyph);

        if (!isspace (c))
            w = x;
    }

    return w;
}
/*
 * A ThroughTextGlyphFunc should return true if it wants the next glyph.
 * Glyph pointer is NULL for the terminating null character!
 */
typedef bool (*ThroughTextGlyphFunc)(const Font *, const Glyph*, const float x, const float y, const int string_pos, void*);

void ThroughText (const Font *pFont, const char *pUTF8,
                  ThroughTextGlyphFunc GlyphFunc, void *pObject,
                  const int align, float maxWidth)
{
    int i = 0, n, halign = align & 0x0f;

    float x = 0.0f, y = 0.0f,
          lineWidth = NextLineWidth (pFont, pUTF8, maxWidth),
          glyph_x;
    unicode_char c = NULL, prev_c = NULL;
    const Glyph *pGlyph;
    while (*pUTF8)
    {
        prev_c = c;

        if (NeedNewLine (pFont, prev_c, pUTF8, x, maxWidth))
        {
            // start new line
            pUTF8 = CleanLineEnd (pUTF8, &n);
            i += n;
            prev_c = c = NULL;

            lineWidth = NextLineWidth (pFont, pUTF8, maxWidth);
            x = 0.0f;
            y += GetLineSpacing (pFont);

            continue;
        }

        pUTF8 = utf8_next (pUTF8, &c);

        if (pFont->glyphs.find(c) == pFont->glyphs.end())
        {
            return;
        }
        pGlyph = &pFont->glyphs.at (c);

        if (x > 0.0f)
            x -= GetHKern (pFont, prev_c, c);

        glyph_x = x;
        if (halign == TEXTALIGN_MID)
            glyph_x = x - lineWidth / 2;
        else if (halign == TEXTALIGN_RIGHT)
            glyph_x = x - lineWidth;

        if (!GlyphFunc (pFont, pGlyph, glyph_x, y, i, pObject))
            return;

        x += GetHAdv (pFont, pGlyph);
        i ++;
    }

    // Call GlyphFunc for the terminating null
    glyph_x = x;
    if (halign == TEXTALIGN_MID)
        glyph_x = x - lineWidth / 2;
    else if (halign == TEXTALIGN_RIGHT)
        glyph_x = x - lineWidth;

    GlyphFunc (pFont, NULL, glyph_x, y, i, pObject);
}
bool glRenderTextCallBack (const Font *pFont, const Glyph *pGlyph, const float x, const float y, const int string_pos, void *p)
{
    if (!pGlyph)
        return true;

    float ori_x, ori_y;
    GetGlyphOrigin (pFont, pGlyph, ori_x, ori_y);

    if (pGlyph->tex)
        glRenderGlyph (pFont, pGlyph, x - ori_x, y - ori_y);

    return true;
}
void glRenderText (const Font *pFont, const char *pUTF8, const int align, float maxWidth)
{
    glFrontFace (GL_CW);
    glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);

    ThroughText (pFont, pUTF8,
                 glRenderTextCallBack, NULL,
                 align, maxWidth);
}
struct TextRectObject {
    int from, to;
    float start_x, current_y, end_x;
};
bool glRenderTextAsRectsCallBack (const Font *pFont, const Glyph *pGlyph, const float x, const float y, const int string_pos, void *p)
{
    TextRectObject *pO = (TextRectObject *)p;
    if (string_pos < pO->from)
    {
        return true; // no rects yet
    }

    float ori_x, ori_y;
    GetGlyphOrigin (pFont, pGlyph, ori_x, ori_y);

    const GLfloat w = GetHAdv (pFont, pGlyph),
                  h = GetLineSpacing (pFont);

    bool end_render = string_pos >= pO->to,
         first_glyph = string_pos <= pO->from,
         new_line = abs ((y - ori_y) - pO->current_y) >= h,

         end_prev_rect = end_render || new_line,
         start_new_rect = new_line || first_glyph;

    if (end_prev_rect)
    {
        //printf ("end at %d\n", string_pos);

        GLfloat x1, y1, x2, y2;

        x1 = pO->start_x;
        x2 = pO->end_x;
        y1 = pO->current_y;
        y2 = y1 + h;

        glBegin (GL_QUADS);
        glVertex2f (x1, y1);
        glVertex2f (x2, y1);
        glVertex2f (x2, y2);
        glVertex2f (x1, y2);
        glEnd ();
    }
    if (start_new_rect)
    {
        //printf ("start at %d\n", string_pos);

        pO->start_x = x - ori_x;
        pO->current_y = y - ori_y;
    }

    if (end_render)
        return false;

    pO->end_x = x - ori_x + w;

    return true;
}
void glRenderTextAsRects (const Font *pFont, const char *pUTF8,
                          const int from, const int to,
                          const int align, float maxWidth)
{
    if (from >= to || from < 0)
        return; // nothing to do

    TextRectObject o;
    o.from = from;
    o.to = to;

    glFrontFace (GL_CW);
    glActiveTexture (GL_TEXTURE0);

    ThroughText (pFont, pUTF8,
                 glRenderTextAsRectsCallBack, &o,
                 align, maxWidth);
}

struct GlyphAtObject
{
    int pos,
        leftmost_pos,
        rightmost_pos;
    float px, py;
};
bool WhichGlyphAtCallBack (const Font *pFont, const Glyph *pGlyph, const float x, const float y, const int string_pos, void *p)
{
    GlyphAtObject *pO = (GlyphAtObject *)p;

    float ori_x, ori_y, h_adv;
    GetGlyphOrigin (pFont, pGlyph, ori_x, ori_y);

    h_adv = GetHAdv (pFont, pGlyph);

    float x1 = x - ori_x, x2 = x1 + h_adv,
          y1 = y - ori_y, y2 = y1 + GetLineSpacing (pFont);

    if (pO->py > y1 && pO->py < y2)
    {
        //printf ("%d (%f) on line between %f and %f\n", string_pos, pO->py, y1, y2);

        // point is on this line

        if (pO->px >= x1 && pO->px < x2) // point is inside the glyph's box
        {
            pO->pos = string_pos;

            return true; // we might want the next glyph, after kerning
        }
        else if (pO->px < x1 && pO->leftmost_pos < 0) // leftmost_pos not yet set and point is to the left of the glyph
        {
            pO->leftmost_pos = string_pos;

            return true;
        }
        else if (pO->px > x2 && pO->pos < 0) // pos is not yet set and point is to the right of the glyph
        {
            pO->rightmost_pos = string_pos;

            return true;
        }
    }

    return (pO->pos < 0); // as long as pos id -1, keep checking the next glyph
}
int WhichGlyphAt (const Font *pFont, const char *pUTF8,
                  const float px, const float py,
                  const int align, float maxWidth)
{
    GlyphAtObject o;
    o.pos = -1;
    o.leftmost_pos = -1;
    o.rightmost_pos = -1;
    o.px = px;
    o.py = py;

    ThroughText (pFont, pUTF8,
                 WhichGlyphAtCallBack, &o,
                 align, maxWidth);

    if (o.pos < 0) // no exact match, but maybe a very close one
    {
        if (o.leftmost_pos >= 0)

            return o.leftmost_pos;

        else if (o.rightmost_pos >= 0)

            return o.rightmost_pos;
    }

    return o.pos;
}
bool CoordsOfGlyphCallBack (const Font *pFont, const Glyph *pGlyph, const float x, const float y, const int string_pos, void *p)
{
    GlyphAtObject *pO = (GlyphAtObject *)p;

    if (pO->pos == string_pos)
    {
        float ori_x,
              ori_y;
        GetGlyphOrigin (pFont, pGlyph, ori_x, ori_y);

        pO->px = x - ori_x;
        pO->py = y - ori_y;

        return false;
    }
    return true;
}
void CoordsOfGlyph (const Font *pFont, const char *pUTF8, const int pos,
                         float &outX, float &outY,
                         const int align, float maxWidth)
{
    GlyphAtObject o;
    o.pos = pos;
    o.px = 0;
    o.py = 0;

    ThroughText (pFont, pUTF8,
                 CoordsOfGlyphCallBack, &o,
                 align, maxWidth);

    outX = o.px;
    outY = o.py;
}
struct DimensionsTracker
{
    float minX, maxX, minY, maxY;
};
bool DimensionsOfTextCallBack (const Font *pFont, const Glyph *pGlyph, const float x, const float y, const int string_pos, void *p)
{
    DimensionsTracker *pT = (DimensionsTracker *)p;

    float ori_x, ori_y, h_adv;
    GetGlyphOrigin (pFont, pGlyph, ori_x, ori_y);

    h_adv = GetHAdv (pFont, pGlyph);

    float x1 = x - ori_x, x2 = x1 + h_adv,
          y1 = y - ori_y, y2 = y1 + GetLineSpacing (pFont);

    pT->minX = std::min (x1, pT->minX);
    pT->minY = std::min (y1, pT->minY);
    pT->maxX = std::max (x2, pT->maxX);
    pT->maxY = std::max (y2, pT->maxY);

    return true;
}
void DimensionsOfText (const Font *pFont, const char *pUTF8,
                       float &outX1, float &outY1, float &outX2, float &outY2,
                       const int align, float maxWidth)
{
    if (pFont->glyphs.size() <= 0)
    {
        outX1 = outY1 = outX2 = outY2 = 0.0;
        return;
    }

    DimensionsTracker t;
    t.minX = t.minY = 1.0e+15f;
    t.maxX = t.maxY = -1.0e+15f;

    ThroughText (pFont, pUTF8,
                 DimensionsOfTextCallBack, &t,
                 align, maxWidth);

    outX1 = t.minX;
    outX2 = t.maxX;
    outY1 = t.minY;
    outY2 = t.maxY;
}
