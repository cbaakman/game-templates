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
void Menu::OnEvent (const SDL_Event* event)
{
    EventListener::OnEvent (event);
    if (focussed && inputEnabled)
    {
        focussed -> OnEvent (event);
    }
}
void Menu::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (!inputEnabled)
        return;

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
    default: return;
    }
}
void Menu::OnMouseClick (const SDL_MouseButtonEvent *event)
{
    if(!inputEnabled)
        return;

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
void Menu::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    if(!inputEnabled)
        return;

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
    lastCharKey=NULL;
    lastCharKeyChar=NULL;
}
void TextInputBox::UpdateShowText()
{
    if (textMask) // only used when masking
    {
        int i;
        for(i=0; text[i]; i++)
            showText[i] = textMask;
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

    // There might be no selection:
    if (start >= end)
        return;

    // If a text mask was used, the masked text is copied:
    std::string ctxt;
    if (textMask)
        ctxt = std::string (showText + start, end - start);
    else
        ctxt = std::string (text + start, end - start);

    // SDL must fill the clipboard for us
    SDL_SetClipboardText (ctxt.c_str());
}
void TextInputBox::PasteText()
{
    // Determine where selection starts and where it ends:

    int start = (cursorPos < fixedCursorPos) ? cursorPos : fixedCursorPos,
        end = (cursorPos > fixedCursorPos) ? cursorPos : fixedCursorPos,
        n, i,
        space,
        charsPasted;

    if(end > start) // Some text was selected, clear it first
    {
        ClearText (start, end);
    }

    /*
        Inserted text might exceed the max length.
        We therefore only insert the text that still fits in.
     */
    n = strlen (text);
    space = maxTextLength - n; // space remaining for fill

    // move the right side of the cursor maximally to the right:
    // (including null)
    for(i=n+1; i>=start; i--)
    {
        text [i + space] = text[i];
    }

    // Let sdl fetch the clipboard contents for us, then insert it:
    strncpy (text + start, SDL_GetClipboardText (), space);
    charsPasted = std::min ((int)strlen (SDL_GetClipboardText ()), space);

    // move the cursor after the inserted part:
    cursorPos = fixedCursorPos = start + charsPasted;

    // Move the right half of the original text back to the left,
    // to follow after the inserted text.
    if (charsPasted < space)
    {
        int holeSize = space - charsPasted;
        start = start + charsPasted;
        end = maxTextLength - holeSize;

        // move back to the left:
        for(i = start; i <= end; i++)
        {
            text[i] = text[i + holeSize];
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
void TextInputBox::OnDelChar (char) {}
void TextInputBox::OnAddChar (char) {}
void TextInputBox::OnBlock () {}
void TextInputBox::OnMoveCursor (int direction) {}
void TextInputBox::ClearText (int start, int end)
{
    // clamp start to the string's fist character.
    // clamp end to the string's last character.
    int n = strlen (text), i, d;

    if (start < 0)
        start = 0;

    if (end > n)
        end = n;

    d = end - start;
    if (d < 0)
        return; // invalid function arguments

    // Move characters to the left:
    for(i = start; i < (n - d + 1); i++)
    {
        text [i] = text [i + d];
    }

    // If the text changes, then the masked text should also change:
    UpdateShowText();
}
void TextInputBox::BackspaceProc()
{
    int n = strlen(text);
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

            OnDelChar (text [cursorPos - 1]);

            ClearText (cursorPos - 1, cursorPos);
            cursorPos--;
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
    int n = strlen (text);
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

            OnDelChar (text [cursorPos]);

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
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
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
    int n = strlen(text);

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
void TextInputBox::AddCharProc(char c)
{
    // clear the selection first, if any
    if (fixedCursorPos > cursorPos)
    {
        ClearText (cursorPos,fixedCursorPos);
        fixedCursorPos = cursorPos;
    }
    else if (fixedCursorPos < cursorPos)
    {
        ClearText (fixedCursorPos,cursorPos);
        cursorPos = fixedCursorPos;
    }

    int n = strlen (text), i;

    if (n < maxTextLength) // room for more?
    {
        // move everything on the right of the cursor to the right
        for(i = n + 1; i>cursorPos; i--)
        {
            text[i]=text[i-1]; // also moves the null
        }
        text[cursorPos] = c;

        // move the cursor to the right, also
        cursorPos++;
        fixedCursorPos = cursorPos;

        OnAddChar(c);
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
void TextInputBox::Update(const float dt)
{
    if (IsFocussed ())
    {
        cursor_time += dt; // for cursor blinking

        if (button_time < 0)
            button_time += dt;

        const Uint8 *keystate = SDL_GetKeyboardState (NULL);

        if (button_time >= 0)
        {
            /*
                If a button is held down for a certain amount of time
                it must be repeated.
             */

            if (keystate [SDL_SCANCODE_BACKSPACE] )
            {
                BackspaceProc ();
                button_time = -TEXT_BUTTON_INTERVAL;
            }
            else if (keystate [SDL_SCANCODE_DELETE] )
            {
                DelProc ();
                button_time = -TEXT_BUTTON_INTERVAL;
            }
            else if (keystate[SDL_SCANCODE_LEFT] )
            {
                LeftProc ();
                button_time = -TEXT_BUTTON_INTERVAL;
            }
            else if (keystate [SDL_SCANCODE_RIGHT] )
            {
                RightProc ();
                button_time = -TEXT_BUTTON_INTERVAL;
            }

            if (keystate [lastCharKey])
            {
                AddCharProc (lastCharKeyChar);
                button_time = -TEXT_BUTTON_INTERVAL;
            }
            else
            {
                lastCharKey = NULL;
                lastCharKeyChar = NULL;
            }
        }

        // If the text changes, then the masked text should also change:
        UpdateShowText ();
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
    else if (bCTRL && sym == SDLK_c)
    {
        // CTRL + C

        CopySelectedText();
    }
    else if (bCTRL && sym == SDLK_v)
    {
        // CTRL + V

        PasteText();
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
    else
    {
        char c = GetKeyChar (event);

        if (c) // character key pressed
        {
            button_time = -FIRST_TEXT_BUTTON_INTERVAL;
            lastCharKey = scn;
            lastCharKeyChar = c;
            AddCharProc (c);
        }
        else
            return;
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
