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


#include <SDL2/SDL.h>

#include "gui.h"

#include <algorithm>
#include <string>
#include "math.h"
#include "../util.h"
#include "../str.h"

#include "client.h"

MenuObject::MenuObject()
{
    focussed = false;
    enabled = true;
}
bool MenuObject::IsInputEnabled() const
{
    if (menu && !menu->IsInputEnabled())
        return false;

    return enabled;
}
MenuObject* MenuObject::NextInLine() const
{
    return NULL;
}
void MenuObject::RenderCursor(const int mX, const int mY)
{
    if (menu)
        menu->RenderDefaultCursor (mX, mY);
}
void MenuObject::SetFocus (const bool b)
{
    if (b && !focussed)
        OnFocusGain ();
    if (!b && focussed)
        OnFocusLose ();

    focussed = b;
}
Menu::Menu (Client *p) : pClient (p)
{
    focussed = NULL;
    inputEnabled = true;
}
Menu::~Menu ()
{
    for(std::list <MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *obj = *it;
        delete obj;
    }
    objects.clear ();
}
void Menu::DisableInput ()
{
    /*
    if(focussed)
    {
        focussed->focussed=false;
        focussed=NULL;
    }
    */
    inputEnabled = false;
}
void Menu::EnableInput ()
{
    inputEnabled = true;
}
void Menu::DisableObjectFocus (MenuObject* obj)
{
    // disables the underlying menu object

    for(std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *o = *it;

        if (o == obj)
        {
            obj->enabled = false;
            return;
        }
    }
}
void Menu::EnableObjectFocus (MenuObject* obj)
{
    // enables the underlying menu object and cancels focus

    for(std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *o = *it;

        if(o == obj)
        {
            if (obj == focussed)
            {
                obj->focussed=false;
                focussed=NULL;
            }
            obj->enabled=true;
            return;
        }
    }
}
void Menu::AddMenuObject (MenuObject* o)
{
    o->menu = this;
    objects.push_back (o);
}
void Menu::FocusMenuObject (MenuObject* obj)
{
    if (obj != focussed)
    {
        // unset the old
        if (focussed)
        {
            focussed->SetFocus (false);
        }
        // set the new
        focussed = obj;
        if (obj) // may be NULL
        {
            focussed->SetFocus (true);
        }
    }
}
void Menu::OnEvent (const SDL_Event *event)
{
    if (inputEnabled)
        EventListener::OnEvent (event);
}
void Menu::OnTextInput (const SDL_TextInputEvent *event)
{
    focussed->OnTextInput (event);
}
void Menu::OnKeyPress (const SDL_KeyboardEvent *event)
{
    switch (event->keysym.sym)
    {
    case SDLK_TAB:
        if (focussed)
        {
            // tab key is pressed, switch to next menu object

            MenuObject* next = focussed->NextInLine();
            if (next)
                FocusMenuObject (next);
        }
    break;
    default:
        focussed->OnKeyPress (event);
        return;
    }
}
void Menu::OnMouseClick (const SDL_MouseButtonEvent *event)
{
    /*
        When multiple objects are hit by the mouse cursor,
        the last one in the list gets the focus. Because that
        object is rendered on top.
     */
    MenuObject* mouseOverObj=NULL;
    for (std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *pObj = *it;

        if(pObj->enabled && pObj->MouseOver((GLfloat)event->x, (GLfloat)event->y))
        {
            mouseOverObj = pObj;
        }
    }
    if (mouseOverObj)
    {
        FocusMenuObject (mouseOverObj);
        mouseOverObj->OnMouseClick (event);
    }
}
void Menu::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    /*
        Send this through to all enabled objects.
     */

    for (MenuObject *pObj : objects)
    {
        if (pObj->enabled)
        {
            pObj->OnMouseMove (event);
        }
    }
}
void Menu::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    int mX, mY;
    SDL_GetMouseState (&mX, &mY);

    /*
        Mouse wheel events are sent through to anything under the mouse.
        (also if not focussed) They might be scroll events.
     */
    for (std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *pObj = *it;

        if(pObj->enabled && pObj->MouseOver((GLfloat)mX, (GLfloat)mY))
        {
            pObj->OnMouseWheel (event);
        }
    }
}
void Menu::Update(const float dt)
{
    // All menu objects get updated, not just the focussed one

    for (std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *pObj = *it;

        pObj->Update (dt);
    }
}
void Menu::Render()
{
    int mX, mY;
    SDL_GetMouseState (&mX, &mY);

    /*
        Determine over which object the mouse rolls, focussed has priority.
        The chosen object gets to render the cursor.
    */
    MenuObject* mouseOverObj = NULL;
    if (focussed && focussed->MouseOver ((GLfloat) mX, (GLfloat) mY))
    {
        mouseOverObj = focussed;
    }
    for (std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *pObj = *it;

        if(!mouseOverObj && pObj->MouseOver((GLfloat)mX, (GLfloat)mY))
        {
            mouseOverObj = pObj;
        }

        pObj->Render();
    }

    int w, h;
    SDL_GL_GetDrawableSize (pClient->GetMainWindow (), &w, &h);

    // Render the mouse cursor, if in the window.
    if (mX > 0 && mX < w && mY > 0 && mY < h)
    {
        if (mouseOverObj && inputEnabled && mouseOverObj->enabled)
        {
            mouseOverObj->RenderCursor (mX, mY);
        }
        else
        {
            RenderDefaultCursor (mX, mY);
        }
    }
}
TextInputBox::TextInputBox(const GLfloat x, const GLfloat y,
                           const Font* f,
                           const int maxTextLength,
                           const int textAlign,
                           const char mask ) : MenuObject(),
    pFont (f)
{
    this->maxTextLength = maxTextLength;
    text = new char [maxTextLength + 1];
    text [0] = NULL;
    cursorPos = fixedCursorPos = 0;
    this->textAlign = textAlign;
    this->x = x;
    this->y = y;

    textMask = mask;
    if (textMask)
    {
        // only allocate this string when a text mask is used
        showText = new char [maxTextLength+1];
        showText [0] = NULL;
    }
    else showText=NULL;

    cursor_time=button_time=0;
}
void TextInputBox::UpdateShowText()
{
    if (textMask) // only used when masking
    {
        // one mask char per utf-8 char
        int i;
        const char *p;
        unicode_char ch;
        for (i = 0, p = text; *p; p = next_from_utf8 (p, &ch), i++)
            showText [i] = textMask;
        showText [i] = NULL;
    }
}
TextInputBox::~TextInputBox()
{
    delete [] showText;
    delete [] text;
}
int TextInputBox::GetCursorPos() const
{
    return cursorPos;
}
void TextInputBox::GetCursorCoords(GLfloat& cx, GLfloat& cy) const
{
    if (pFont)
    {
        char *t = textMask ? showText : text;

        CoordsOfGlyph (pFont, t, cursorPos, cx, cy, textAlign);
        cx += x; cy += y;
    }
}
void TextInputBox::GetFixedCursorCoords(GLfloat& cx, GLfloat& cy) const
{
    if(pFont)
    {
        char* t = textMask ? showText : text;

        CoordsOfGlyph (pFont, t, fixedCursorPos, cx, cy, textAlign);
        cx += x; cy += y;
    }
}
const char* TextInputBox::GetText() const
{
    return text;
}
void TextInputBox::CopySelectedText() const
{
    // Determine where selection starts and where it ends:

    int start = (cursorPos < fixedCursorPos) ? cursorPos : fixedCursorPos,
        end = (cursorPos > fixedCursorPos) ? cursorPos : fixedCursorPos;
    const char *pstart, *pend, *t;

    // There might be no selection:
    if (start >= end)
        return;

    // If a text mask was used, the masked text is copied:
    if (textMask)
        t = showText;
    else
        t = text;

    pstart = pos_utf8 (t, start);
    pend = pos_utf8 (t, end);
    std::string ctxt (pstart, pend - pstart);

    // SDL must fill the clipboard for us
    SDL_SetClipboardText (ctxt.c_str ());
}
void TextInputBox::PasteText ()
{
    // Determine where selection starts and where it ends:

    const char *c = SDL_GetClipboardText ();
    unicode_char ch;
    int n, i, f,
        space,
        bytesPasted,
        cursorLeft = std::min (cursorPos, fixedCursorPos),
        cursorRight = std::max (cursorPos, fixedCursorPos),

        start = pos_utf8 (text, cursorLeft) - text,
        end = pos_utf8 (text, cursorRight) - text;

    if (cursorRight > cursorLeft) // Some text was selected, clear it first
    {
        ClearText (cursorLeft, cursorRight);
    }

    /*
        Inserted text might exceed the max length.
        We therefore only insert the text that still fits in.
     */
    n = strlen (text);
    space = maxTextLength - n; // space remaining for fill

    /*
        move the bytes on the right side of the cursor maximally to the right:
        (including null)
     */
    for (i = n; i >= start; i--)
    {
        text [i + space] = text [i];
    }

    /*
        Insert as much of the clipboard contents as possible,
        Without cutting a utf-8 character in half.
     */
    i = f = 0;
    while (c [i])
    {
        i = f;
        f = (int) (next_from_utf8 (c + i, &ch) - c);
        if (f > space || !ch)
            break;

        strncpy (text + start + i, c + i, f - i);
        bytesPasted = f;
    }

    // put the cursor after the inserted part:
    cursorPos = fixedCursorPos = strlen_utf8 (text, text + start + bytesPasted);

    // Move the right half of the original text back to the left,
    // to follow after the inserted text.
    if (bytesPasted < space)
    {
        int gapSize = space - bytesPasted;
        start += bytesPasted;
        end = maxTextLength - gapSize;

        // move back to the left:
        for (i = start; i <= end; i++)
        {
            text [i] = text [i + gapSize];
        }
    }

    // update the masked text with the new state
    UpdateShowText();
}
void TextInputBox::RenderText() const
{
    char* t=text;
    if (textMask)
    {
        t = showText;
    }

    // glRenderText renders at the origin, so translate to x, y.

    glTranslatef (x, y, 0.0f);
    glRenderText (pFont, t, textAlign);
    glTranslatef (-x, -y, 0.0f);
}
void TextInputBox::RenderTextCursor() const
{
    /*
        The text cursor has a fluctuating alpha value.
        This alpha value is determined from the cursor_time field in the object.
     */

    glPushAttrib (GL_TEXTURE_BIT | GL_CURRENT_BIT);
    glDisable (GL_TEXTURE_2D);

    GLfloat alpha = 0.6f + 0.3f * sin (10 * cursor_time);

    GLfloat clr [4];
    glGetFloatv (GL_CURRENT_COLOR, clr);

    glColor4f (clr[0], clr[1], clr[2], clr[3] * alpha);

    int selectionStart = std::min (cursorPos, fixedCursorPos),
        selectionEnd = std::max (cursorPos, fixedCursorPos);

    if (selectionStart >= selectionEnd)
    // no text selected, just render a narrow rectangle at the cursor position
    {
        GLfloat h1,h2,cx,cy,cx2;

        GetCursorCoords (cx,cy);

        h1 = -pFont->horiz_origin_y;
        h2 = h1 + GetLineSpacing (pFont);

        glBegin(GL_QUADS);
            glVertex2f (cx - 2.0f, cy + h1);
            glVertex2f (cx + 2.0f, cy + h1);
            glVertex2f (cx + 2.0f, cy + h2);
            glVertex2f (cx - 2.0f, cy + h2);
        glEnd();
    }
    else // render a rectangle over the selected text
    {
        char* t=text;
        if(textMask)
        {
            t=showText;
        }

        glTranslatef (x, y, 0.0f);
        glRenderTextAsRects (pFont, t, selectionStart, selectionEnd, textAlign);
        glTranslatef (-x, -y, 0.0f);
    }

    glPopAttrib();
}
void TextInputBox::OnFocusGain()
{
}
void TextInputBox::OnFocusLose()
{
    // place cursor at the farmost right

    char* t = text;
    if (textMask)
    {
        t = showText;
    }

    fixedCursorPos = cursorPos = strlen (t);
}
#define TEXTSELECT_XMARGE 15.0f
bool TextInputBox::MouseOver (GLfloat mX, GLfloat mY) const
{
    // See if the cursor hits the text:

    char* t = textMask ? showText : text;

    int cpos = WhichGlyphAt (pFont, t, (GLfloat)(mX - x),(GLfloat)(mY - y), textAlign),
        n = strlen(t);

    if (cpos >= 0)
    {
        GLfloat cx1,cy1,cx2,cy2;

        CoordsOfGlyph (pFont, t, 0, cx1, cy1, textAlign);
        cx1 += x; cy1 += y;

        CoordsOfGlyph (pFont, t, n, cx2, cy2, textAlign);
        cx2 += x; cy2 += y;

        /*
            When mX, mY is located left or right from the text,
            WhichGlyphAt returns the index of the leftmost or rightmost character, respectively.
            If it's very close to the text, we will consider it a hit, else not.
         */

        if (mX > (cx1 - TEXTSELECT_XMARGE) && mX < (cx2 + TEXTSELECT_XMARGE))
        {
            return true;
        }
    }
    return false;
}
void TextInputBox::OnDelChar (const unicode_char) {}
void TextInputBox::OnAddChar (const unicode_char) {}
void TextInputBox::OnBlock () {}
void TextInputBox::OnMoveCursor (int direction) {}
void TextInputBox::ClearText (const std::size_t start, const std::size_t end)
{
    // clamp start to the utf-8 string's fist character.
    // clamp end to the utf-8 string's last character.
    int n = strlen_utf8 (text),
        dist, i, m;
    const char *pstart = pos_utf8 (text, start),
               *pend = pos_utf8 (text, end);

    if (start < 0)
        pstart = 0;

    if (end > n)
        pend = text + strlen (text); // position of terminating null

    dist = pend - pstart;

    if (dist <= 0)
        return; // invalid function arguments

    // Move bytes to the left. (including terminating null)
    m = strlen (text) + 1 - dist;
    for (i = pstart - text; i < m; i++)
    {
        text [i] = text [i + dist];
    }

    // If the text changes, then the masked text should also change:
    UpdateShowText ();
}
void TextInputBox::BackspaceProc()
{
    int n = strlen_utf8 (text);
    if (n > 0)
    {
        /*
            Either remove the selected text if any,
            or the first character before the cursor.
         */
        if (fixedCursorPos > cursorPos)
        {
            ClearText (cursorPos, fixedCursorPos);
            fixedCursorPos = cursorPos;
        }
        else if (fixedCursorPos < cursorPos)
        {
            ClearText (fixedCursorPos, cursorPos);
            cursorPos = fixedCursorPos;
        }
        else if (cursorPos > 0)
        {
            // Remove a single character:

            unicode_char ch;
            const char *prev = prev_from_utf8 (text + cursorPos, &ch);

            OnDelChar (ch);

            ClearText (cursorPos - 1, cursorPos);
            cursorPos --;
            fixedCursorPos = cursorPos;
        }
        else OnBlock (); // cannot go further
    }
    else OnBlock (); // cannot go further

    // If the text changes, then the masked text should also change:
    UpdateShowText ();
}
void TextInputBox::DelProc()
{
    int n = strlen_utf8 (text);
    if (n > 0)
    {
        /*
            Either remove the selected text if any,
            or the first character after the cursor.
         */

        if (fixedCursorPos > cursorPos)
        {
            ClearText (cursorPos, fixedCursorPos);
            fixedCursorPos = cursorPos;
        }
        else if (fixedCursorPos < cursorPos)
        {
            ClearText (fixedCursorPos, cursorPos);
            cursorPos = fixedCursorPos;
        }
        else if (cursorPos < n)
        {
            // Remove a single character:
            unicode_char ch;
            const char *next = next_from_utf8 (text + cursorPos, &ch);

            OnDelChar (ch);

            ClearText (cursorPos, cursorPos + 1);
            fixedCursorPos = cursorPos;
        }
        else OnBlock(); // cannot go further
    }
    else OnBlock(); // cannot go further

    // If the text changes, then the masked text should also change:
    UpdateShowText();
}
void TextInputBox::LeftProc()
{
    const Uint8 *keystate = SDL_GetKeyboardState (NULL);
    const bool shift = keystate [SDL_SCANCODE_LSHIFT] || keystate [SDL_SCANCODE_RSHIFT];

    // See if we can move the cursor one to the left:
    cursorPos--;

    if(cursorPos < 0)
    {
        // cannot go further

        cursorPos = 0;
        OnBlock ();
    }
    else
        OnMoveCursor (-1);

    // if shift is down, keep fixedCursorPos the same, else move it along.
    if (!shift)
        fixedCursorPos = cursorPos;

    // If the text changes, then the masked text should also change:
    UpdateShowText ();
}
void TextInputBox::RightProc()
{
    int n = strlen_utf8 (text);

    const Uint8 *keystate = SDL_GetKeyboardState (NULL);
    const bool shift = keystate [SDL_SCANCODE_LSHIFT] || keystate [SDL_SCANCODE_RSHIFT];

    // See if we can move the cursor one to the right:
    cursorPos++;

    if (cursorPos > n)
    {
        // cannot go further

        cursorPos = n;
        OnBlock ();
    }
    else
        OnMoveCursor (1);

    // if shift is down, keep fixedCursorPos the same, else move it along.
    if(!shift)
        fixedCursorPos = cursorPos;

    // If the text changes, then the masked text should also change:
    UpdateShowText ();
}
void TextInputBox::AddCharProc (const char *c)
{
    // clear the selection first, if any
    if (fixedCursorPos > cursorPos)
    {
        ClearText (cursorPos, fixedCursorPos);
        fixedCursorPos = cursorPos;
    }
    else if (fixedCursorPos < cursorPos)
    {
        ClearText (fixedCursorPos, cursorPos);
        cursorPos = fixedCursorPos;
    }

    const char *pcursor = pos_utf8 (text, cursorPos);
    int n = strlen (text),
        m = strlen (c),
        cursorOffset = pcursor - text;
    char *p;

    if ((n + m) <= maxTextLength) // room for more?
    {
        /*
            Move everything on the right of the cursor to the right,
            including the terminating null.
         */
        for (p = text + n + 1; p > (text + cursorOffset); p--)
        {
            *p = *(p - m);
        }
        // insert:
        strncpy (text + cursorOffset, c, m);

        // move the cursor to the right, also
        cursorPos ++;
        fixedCursorPos = cursorPos;

        unicode_char ch;
        next_from_utf8 (c, &ch);
        OnAddChar (ch);
    }
    else
    {
        // Cannot add more characters

        OnBlock();
    }

    // If the text changes, then the masked text should also change:
    UpdateShowText();
}
#define TEXT_BUTTON_INTERVAL 0.03f
#define FIRST_TEXT_BUTTON_INTERVAL 0.3f
void TextInputBox::Update (const float dt)
{
    if (IsFocussed ())
    {
        cursor_time += dt; // for cursor blinking
    }
}
void TextInputBox::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    if (event->state & SDL_BUTTON_LMASK)
    {
        /*
            Mouse key is pressed down while moving the cursor.
            This means text can be selected. Determine which
            glyph is under the cursor:
         */

        char* t = text;
        if (textMask)
        {
            t=showText;
        }
        int cpos = WhichGlyphAt (pFont, t, (GLfloat)(event->x - x), (GLfloat)(event->y - y), textAlign);

        if (cpos >= 0)
        {
            if (cpos == strlen (t))

                cursorPos = cpos;
            else
            {
                /* The movement direction and position of the mouse determine whether we
                   include the pointed character in the selection or not */

                float textY, leftBound, rightBound;
                CoordsOfGlyph (pFont, t, cpos, leftBound, textY, textAlign);
                CoordsOfGlyph (pFont, t, cpos + 1, rightBound, textY, textAlign);

                // make leftBound and rightBound absolute:
                leftBound += x;
                rightBound += x;

                if (event->x > leftBound && event->x < rightBound)
                {
                    if (event->xrel > 0)
                        cursorPos = cpos + 1;
                    else
                        cursorPos = cpos;
                }
                else
                    cursorPos = cpos;
            }
        }
    }
}
void TextInputBox::OnMouseClick (const SDL_MouseButtonEvent *event)
{
    /*
        See if a glyph was hit by the click. If so,
        move the cursor and perhaps the selection.
     */

    char *t = text;
    if (textMask)
    {
        t = showText;
    }
    int cpos,
        prev_fixedCursorPos = fixedCursorPos;

    const Uint8 *keystate = SDL_GetKeyboardState (NULL);

    cpos = WhichGlyphAt (pFont, t, event->x - x, event->y - y, textAlign);
    if (cpos >= 0)
    {
        if (event->clicks > 1) // double click, select entire word

            WordAt (t, cpos, fixedCursorPos, cursorPos);
        else
            // place the cursor at the left side of the selected character
            fixedCursorPos = cursorPos = cpos;

        // If shift is down, the selection start should stay where it was.
        if (keystate [SDL_SCANCODE_LSHIFT] || keystate [SDL_SCANCODE_RSHIFT])
            fixedCursorPos = prev_fixedCursorPos;
    }
}
void TextInputBox::OnTextInput (const SDL_TextInputEvent *event)
{
    AddCharProc (event->text);
}
void TextInputBox::OnKeyPress (const SDL_KeyboardEvent *event)
{
    const SDL_Keycode sym = event->keysym.sym;
    const SDL_Scancode scn = event->keysym.scancode;
    const Uint16 mod = event->keysym.mod;

    bool bCTRL = mod & KMOD_CTRL;

    /*
        Take the approriate action, according to
        key pressed.
     */

    if (sym == SDLK_TAB || sym == SDLK_RETURN)
    {
        // This input box only works for single lined text,
        // ignore '\t' and '\n'.
        return;
    }
    else if (bCTRL && sym == SDLK_a)
    {
        // CTRL + A

        cursorPos = 0;
        fixedCursorPos = strlen_utf8 (text);
    }
    else if (bCTRL && sym == SDLK_c)
    {
        // CTRL + C

        CopySelectedText();
    }
    else if (bCTRL && sym == SDLK_v)
    {
        // CTRL + V

        PasteText ();
    }
    else if (sym == SDLK_BACKSPACE)
    {
        button_time = -FIRST_TEXT_BUTTON_INTERVAL;
        BackspaceProc();
    }
    else if (sym == SDLK_DELETE)
    {
        button_time = -FIRST_TEXT_BUTTON_INTERVAL;
        DelProc();
    }
    else if (sym == SDLK_LEFT)
    {
        button_time = -FIRST_TEXT_BUTTON_INTERVAL;
        LeftProc();
    }
    else if (sym == SDLK_RIGHT)
    {
        button_time = -FIRST_TEXT_BUTTON_INTERVAL;
        RightProc();
    }

    // When the text changes, the masked text should also change:
    UpdateShowText();
}
void TextInputBox::Render ()
{
    glPushAttrib(
        GL_CURRENT_BIT|
        GL_COLOR_BUFFER_BIT|
        GL_DEPTH_BUFFER_BIT|
        GL_LIGHTING_BIT|
        GL_POLYGON_BIT|
        GL_TEXTURE_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderText();

    if (IsFocussed ())
        RenderTextCursor();

    glPopAttrib();
}
void TextInputBox::SetText (const char* text)
{
    // copy text
    strncpy (this->text, text, maxTextLength - 1);
    this->text [maxTextLength] = NULL;

    // set cursor position to the rightmost end:
    int n = strlen (this->text);
    cursorPos = fixedCursorPos = n;

    // When the text changes, the masked text should also change:
    UpdateShowText ();
}
