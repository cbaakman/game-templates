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


#ifndef TEXTSCROLL_H
#define TEXTSCROLL_H

#include "gui.h"

#include <string>

/*
 * A vertically scrollable window of text.
 * Composed of a frame, accompanied by a strip with a shiftable bar in it.
 * Depends on a font and text string to know its dimensions.
 * Text can be set mouse-selectable. (false by default) Selected text can be copied to clipboard with Ctrl + C.
 * Classes that inherit this must implement their own Render function.
 */
class TextScroll : public MenuObject
{
private:

    std::string text; // text to be shown

    /*
        Font and alignment need to be set
        for the scroll to know the text's
        dimensions.
     */
    const Font *pFont;
    int textAlign;

    bool text_selectable; // if false, text is not selectable by mouse

    float frame_left_x,
          frame_width,

          top_y, // of frame and strip
          height,

          strip_width, // negative if on the left side of the frame, positive if on the right, zero if no barstrip

          spacing_text_frame, // spacing between text and frame bounding boxes
          spacing_frame_strip, // 0 for the strip to be right next to the frame
          spacing_strip_bar, // can be negative if the bar is wider

          // Variables to remember the state of the scroll at teh moment the mouse was clicked:
          last_mouseclick_x,
          last_mouseclick_y,
          last_mouseclick_bar_y,

          bar_top_y, // changes when scrolling, determines text offset

          // These parameters are determined when text is set:
          // (using DeriveDimensions function)
          min_bar_y,
          max_bar_y,
          bar_height,
          scrolled_area_height;

    // These booleans describe what's happening at the moment:
    bool dragging_bar,
         dragging_text;

    /*
       These variables remember what part of the text
       is selected. If selecting is enabled of coarse.
     */
    int selectionStart,
        selectionEnd;

    void DeriveDimensions (); // derives dimensions from newly set text
    void ClampBar (); // keeps bar within frame

    void CopySelectedText () const; // copies to clipboard

protected:

    // Some MenuObject functions that need to be overridden:
    void OnFocusLose ();
    void OnMouseClick (const SDL_MouseButtonEvent *event);
    void OnMouseMove (const SDL_MouseMotionEvent *event);
    void OnKeyPress (const SDL_KeyboardEvent *event);
    void OnMouseWheel (const SDL_MouseWheelEvent *event);

    /*
       These two subroutines can be used to render the text at the correct position.
       The position of text is determined by scrolling and the frame's set position,
       when a render function calls these subroutines, it'll be responsible for
       clipping or alpha masking the text.
     */
    void RenderText ();
    void RenderTextSelection (); // only needed when text is selectable

    // Overridable, called when scroll state changes.
    virtual void OnScroll (float bar_delta_y) {};

    /*
     * false by default
     */
    void SetSelectable (bool b) { text_selectable = b; }

public:

    // These can be useful when rendering:
    void GetFrameRect (float &x1, float &y1, float &x2, float &y2) const;
    void GetStripRect (float &x1, float &y1, float &x2, float &y2) const;
    void GetBarRect (float &x1, float &y1, float &x2, float &y2) const;
    void GetTextRect (float &x1, float &y1, float &x2, float &y2) const; // area where text is visible

    float GetBarY () const { return bar_top_y; }
    float GetMinBarY () const { return min_bar_y; }
    float GetMaxBarY () const { return max_bar_y; }
    float GetBarHeight () const;

    float GetTextYOffset () const; // 0 when scrolled to the top
    void SetScroll (const float bar_top_y); // in bar-strip space

    int MouseOverStrip (float mX, float mY) const; // -1 (yes, under bar), 0 (not on strip, or over bar), 1 (yes, above bar)
    bool MouseOverBar (float mX, float mY) const;
    bool MouseOverText (float mX, float mY) const;
    bool MouseOverFrame (float mX, float mY) const;

    bool MouseOver (GLfloat mX, GLfloat mY) const;

    void SetText (const char *);
    const char *GetText ();

    TextScroll (const float frame_left_x, const float top_y,
                const float frame_width, const float strip_width, const float height,
                const float spacing_text_frame, const float spacing_frame_strip, const float spacing_strip_bar,
                const Font *, const int textAlign);

    virtual ~TextScroll();

    virtual void Render () = 0;
    virtual void Update (const float dt) {}
};

#endif // TEXTSCROLL_H
