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


#include "textscroll.h"
#include "client.h"

#include <algorithm>

#include "../util.h"
#include "../str.h"

TextScroll::TextScroll (const float _frame_left_x, const float _top_y,
                        const float _frame_width, const float _strip_width, const float _height,
                        const float _spacing_text_frame, const float _spacing_frame_strip, const float _spacing_strip_bar,
                        const Font *_font, const int _textAlign)
: MenuObject(),
    frame_left_x (_frame_left_x),
    top_y (_top_y),
    frame_width (_frame_width),
    strip_width (_strip_width),
    height (_height),
    spacing_text_frame (_spacing_text_frame),
    spacing_frame_strip (_spacing_frame_strip),
    spacing_strip_bar (_spacing_strip_bar),

    pFont (_font), textAlign (_textAlign),
    dragging_bar (false), dragging_text (false),
    selectionStart (0), selectionEnd (0),
    scrolled_area_height (0),
    bar_height (0),
    text_selectable (false)
{
    // Bar top's highest position
    min_bar_y = bar_top_y = spacing_strip_bar + top_y;

    SetText ("");
}
void TextScroll::ClampBar ()
{
    // bar must stay withing min and max

    if (bar_top_y < min_bar_y)
        bar_top_y = min_bar_y;
    if (bar_top_y > max_bar_y)
        bar_top_y = max_bar_y;
}
const char *TextScroll::GetText()
{
    return text.c_str();
}
void TextScroll::SetText (const char *_text)
{
    if (pFont->glyphs.size () <= 0)
    {
        // A font with no glyphs has probably not been loaded yet.

        fprintf (stderr, "TextScroll::SetText: error, font isn\'t ready yet\n");
        return;
    }

    text = std::string (_text);

    // Text determines how big the scrolled area will be, and how big the bar will be
    DeriveDimensions ();
}
void TextScroll::DeriveDimensions ()
{
    float x1, x2, y1, y2,

          bar_frame_ratio,
          bar_domain,

          text_width = frame_width - 2 * spacing_text_frame,
          text_window_height = height - 2 * spacing_text_frame;

    /*
       Now that we know the width of the text area,
       find out how much height the text will need
     */

    DimensionsOfText (pFont, text.c_str(), x1, y1, x2, y2, textAlign, text_width);

    // Convert to absolute coords:
    x1 += frame_left_x;
    x2 += frame_left_x;

    y1 += top_y;
    y2 += top_y;

    scrolled_area_height = y2 - y1;

    // bar_domain: the entire area that the bar can maximally occupy
    bar_domain = height - 2 * spacing_strip_bar;

    // bar_frame_ratio: 1.0 is max, determines how big the bar will be
    bar_frame_ratio = text_window_height / scrolled_area_height;
    if (scrolled_area_height < text_window_height)
        bar_frame_ratio = 1.0f;

    bar_height = bar_domain * bar_frame_ratio;

    /*
        Determine which y positions the bar can have.
        It can move down until its bottom hits the strip's bottom.
     */
    max_bar_y = min_bar_y + height - 2 * spacing_strip_bar - bar_height;

    // Place bar within boundaries:
    ClampBar ();
}
TextScroll::~TextScroll()
{
}
void TextScroll::OnFocusLose ()
{
    dragging_text = dragging_bar = false;
}
void TextScroll::OnMouseClick (const SDL_MouseButtonEvent *event)
{
    const Uint8 *keystate = SDL_GetKeyboardState (NULL);

    if (text_selectable && MouseOverText (event->x, event->y))
    {
        // Mouse hits text area, need to start a text selection

        dragging_text = true;

        float offY = GetTextYOffset(),
              x1, y1, x2, y2;

        int cpos,
            prev_selectionStart = selectionStart;

        // Remember, WhichGlyphAt takes relative coords !!
        GetTextRect (x1, y1, x2, y2);

        cpos = WhichGlyphAt (pFont, text.c_str(), event->x - x1, event->y - (y1 + offY), textAlign, x2 - x1);
        if (cpos >= 0)
        {
            if (event->clicks > 1) // double click, select entire word

                WordAt (text.c_str(), cpos, selectionStart, selectionEnd);
            else
                selectionStart = selectionEnd = cpos;

            // if shift key is down, then just add to the selection, instead of replacing it
            if(keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT])
                selectionStart = prev_selectionStart;
        }
    }

    last_mouseclick_x = event->x;
    last_mouseclick_y = event->y;
    last_mouseclick_bar_y = bar_top_y;

    if (MouseOverBar (event->x, event->y))
    {
        // mouse hits the bar, start dragging it

        dragging_bar = true;
    }

    // if the mouse hits the strip below or above the bar, then move the bar down or up
    int dy = MouseOverStrip (event->x, event->y);
    const float DY = 0.1f * (max_bar_y - min_bar_y);
    if (dy < 0)
    {
        bar_top_y -= DY;
        OnScroll (-DY);
    }
    else if (dy > 0)
    {
        bar_top_y += DY;
        OnScroll (DY);
    }

    // When the bar changes position, make sure it stays within boundaries:
    ClampBar ();
}
void TextScroll::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    const bool lbutton = event->state & SDL_BUTTON_LMASK;

    // Releasing the mouse stops drag
    if (!lbutton)
    {
        dragging_bar = false;
        dragging_text = false;
    }

    if (dragging_bar)
    {
        // move the bar as much as the mouse moved
        bar_top_y = last_mouseclick_bar_y + event->y - last_mouseclick_y;

        ClampBar ();
    }
    else if (dragging_text && MouseOverText (event->x, event->y))
    {
        // text is being selected, include the new mouse position in the selection.

        // if the mouse cursor approaches the bottom or top of the frame while dragging, then scroll a bit:
        float charH = GetLineSpacing (pFont),
              DY = 0.05f * (max_bar_y - min_bar_y),

              x1, y1, x2, y2,

              offY;

        GetTextRect (x1, y1, x2, y2);

        if (event->y < (y1 + charH)) // mouse over top of frame
        {
            bar_top_y -= DY;
            OnScroll (-DY);
        }
        else if (event->y > (y2 - charH)) // mouse over bottom
        {
            bar_top_y += DY;
            OnScroll (DY);
        }

        // If the bar moved, it may not go outside boundaries
        ClampBar();

        offY = GetTextYOffset();

        // Remember, WhichGlyphAt takes relative coords!

        int cpos = WhichGlyphAt (pFont, text.c_str(),
                                 event->x - x1, event->y - (y1 + offY),
                                 textAlign, x2 - x1);
        if (cpos >= 0)
        {
            /*
                The movement direction of the mouse decides
                whether to include the glyph under the cursor.
             */

            if (cpos < text.size() && event->xrel > 0)
                selectionEnd = cpos + 1;
            else
                selectionEnd = cpos;
        }
    }
}
void TextScroll::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    int mX, mY;
    const bool lbutton = SDL_GetMouseState(&mX, &mY) & SDL_BUTTON(SDL_BUTTON_LEFT);

    // Releasing the mouse stops drag
    if (!lbutton)
    {
        dragging_bar = false;
        dragging_text = false;
    }

    // Mouse wheel scrolled vertically means scrolling the text vertically
    const float prev_bar_y = bar_top_y,
                wheel_delta = -0.2f * GetLineSpacing (pFont);

    bar_top_y += wheel_delta * event->y;

    // If the bar moved, it may not go outside boundaries
    ClampBar ();

    OnScroll (bar_top_y - prev_bar_y);

    if (dragging_text && MouseOverText (mX, mY))
    {
        // user is still selecting text, move selection to new cursor position

        float y1, x1, y2, x2,

              offY = GetTextYOffset();

        GetTextRect (x1, y1, x2, y2);

        // Remember, WhichGlyphAt takes relative coords!

        int cpos = WhichGlyphAt (pFont, text.c_str(), float (mX) - x1, float (mY) - (y1 + offY), textAlign, x2 - x1);
        if (cpos >= 0)
        {
            // Move the selection to current cursor position

            selectionEnd = cpos;
        }
    }
}
void TextScroll::SetScroll (const float _bar_top_y)
{
    // Scroll was set by something other than the mouse

    int mX, mY;
    const bool lbutton = SDL_GetMouseState(&mX, &mY) & SDL_BUTTON(SDL_BUTTON_LEFT);

    // Releasing the mouse stops drag
    if (!lbutton)
    {
        dragging_bar = false;
        dragging_text = false;
    }

    float prev_bar_y = bar_top_y;
    bar_top_y = _bar_top_y;

    // If the bar moved, it may not go outside boundaries
    ClampBar ();

    OnScroll (bar_top_y - prev_bar_y);

    if (dragging_text && MouseOverText (mX, mY))
    {
        // user is still selecting text, move selection to new cursor position

        float y1, x1, y2, x2,

              offY = GetTextYOffset();

        GetTextRect (x1, y1, x2, y2);

        // Remember, WhichGlyphAt takes relative coords!

        int cpos = WhichGlyphAt (pFont, text.c_str(), float (mX) - x1, float (mY) - (y1 + offY), textAlign, x2 - x1);
        if (cpos >= 0)
        {
            // Move the selection to current cursor position

            selectionEnd = cpos;
        }
    }
}
void TextScroll::OnKeyPress (const SDL_KeyboardEvent *event)
{
    const SDL_Keycode sym = event->keysym.sym;
    const SDL_Scancode scn = event->keysym.scancode;
    const Uint16 mod = event->keysym.mod;

    if (mod & KMOD_CTRL && sym == SDLK_c && text_selectable)
    { // Ctrl + C

        CopySelectedText();
    }
}
GLfloat TextScroll::GetBarHeight() const
{
    return bar_height;
}
GLfloat TextScroll::GetTextYOffset() const
{
    if (max_bar_y <= min_bar_y)
        return 0.0f;

    const float window_height = height - 2 * spacing_text_frame;

    /*
        When the bar is at max_bar_y, the bottom of
        the text is visible. When the bar is at min_bar_y,
        the top of the text is visible.
     */

    return -(scrolled_area_height - window_height) * (bar_top_y - min_bar_y) / (max_bar_y - min_bar_y);
}
void TextScroll::GetTextRect (float &x1, float &y1, float &x2, float &y2) const
{
    /*
        Return the area where text is visible.
        It's simply the frame, shrunk by the spacing.
     */

    x1 = frame_left_x + spacing_text_frame;
    x2 = frame_left_x + frame_width - spacing_text_frame;
    y1 = top_y + spacing_text_frame;
    y2 = top_y + height - spacing_text_frame;
}
void TextScroll::GetFrameRect (float& x1, float& y1, float& x2, float& y2) const
{
    x1 = frame_left_x;
    x2 = x1 + frame_width;
    y1 = top_y;
    y2 = y1 + height;
}
void TextScroll::GetStripRect (float &x1, float &y1, float &x2, float &y2) const
{
    if (strip_width == 0) // there can be no strip this way
    {
        x1 = 0; x2 = 0;
    }
    else if (strip_width < 0.0f) //  strip is on left side
    {
        x2 = frame_left_x - spacing_frame_strip;
        x1 = x2 + strip_width;
    }
    else if (strip_width > 0.0f) // strip is on right side
    {
        x1 = frame_left_x + frame_width + spacing_frame_strip;
        x2 = x1 + strip_width;
    }

    y1 = top_y;
    y2 = y1 + height;
}
void TextScroll::GetBarRect(float& x1, float& y1, float& x2, float& y2) const
{
    // Bar is on strip. Strip determines x position:

    GetStripRect (x1, y1, x2, y2);

    x1 += spacing_strip_bar;
    x2 -= spacing_strip_bar;

    y1 = bar_top_y;
    y2 = y1 + GetBarHeight ();
}
void TextScroll::CopySelectedText() const
{
    int start = std::min (selectionStart, selectionEnd),
        end = std::max (selectionStart, selectionEnd);

    if (start >= end) // no selection
        return;

    // Get selected substring:
    std::string ctxt = text.substr (start, end - start);

    // let SDL fill up the clipboard for us
    SDL_SetClipboardText (ctxt.c_str());
}
bool TextScroll::MouseOver(GLfloat mX, GLfloat mY) const
{
    // Mouse must be over either of the three:

    return (MouseOverBar (mX, mY) || MouseOverFrame (mX, mY) || MouseOverStrip (mX, mY) != 0);
}
bool TextScroll::MouseOverText (float mX, float mY) const
{
    // Mouse must be over the area where text is visible:

    float x1, x2, y1, y2;
    GetTextRect (x1, y1, x2, y2);

    return (mX > x1 && mX < x2 && mY > y1 && mY < y2);
}
bool TextScroll::MouseOverBar(GLfloat mX, GLfloat mY) const
{
    // Mouse must be over bar rectangular area:

    float barX1, barX2, barY1, barY2;
    GetBarRect (barX1, barY1, barX2, barY2);

    return (mX > barX1 && mX < barX2 && mY > barY1 && mY < barY2);
}
int TextScroll::MouseOverStrip(float mX, float mY) const
{
    /*
        Mouse must be over strip rectangular area,
        below, or above bar rectangular area.
     */

    float stripX1, stripX2, stripY1, stripY2;
    GetStripRect (stripX1, stripY1, stripX2, stripY2);

    float barX1, barX2, barY1, barY2;
    GetBarRect (barX1, barY1, barX2, barY2);

    if (mX > stripX1 && mX < stripX2)
    {
        if (mY > stripY1 && mY < barY1)
            return -1;
        else if (mY < stripY2 && mY > barY2)
            return 1;
    }

    return 0;
}
bool TextScroll::MouseOverFrame (float mX, float mY) const
{
    // Mouse must be over frame rectangular area:

    float x1, x2, y1, y2;
    GetFrameRect (x1, y1, x2, y2);

    return (mX > x1 && mX < x2 && mY > y1 && mY < y2);
}
void TextScroll::RenderText ()
{
    float x, y, x2, y2;
    GetTextRect (x, y, x2, y2);

    y += GetTextYOffset(); // offset from scrolling

    // glRenderText starts at the origin, so we transform:

    glTranslatef (x, y, 0);
    glRenderText (pFont, text.c_str(), textAlign, x2 - x);
    glTranslatef (-x, -y, 0);
}
void TextScroll::RenderTextSelection ()
{
    float x, y, x2, y2;
    GetTextRect (x, y, x2, y2);

    y += GetTextYOffset(); // offset from scrolling

    // Determine selection substring:
    int start = std::min (selectionStart, selectionEnd),
        end = std::max (selectionStart, selectionEnd);

    // glRenderTextAsRects starts at the origin, so we transform:

    glTranslatef (x, y, 0);
    glRenderTextAsRects (pFont, text.c_str(), start, end, textAlign, x2 - x);
    glTranslatef (-x, -y, 0);
}
