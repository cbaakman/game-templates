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
Menu::Menu(Client *p) : pClient (p)
{
    focussed = NULL;
    inputEnabled = true;
}
Menu::~Menu()
{
    for(std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *obj = *it;
        delete obj;
    }
    objects.clear();
}
void Menu::DisableInput()
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
void Menu::EnableInput()
{
    inputEnabled = true;
}
void Menu::DisableObjectFocus(MenuObject* obj)
{
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
void Menu::EnableObjectFocus(MenuObject* obj)
{
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
void Menu::AddMenuObject(MenuObject* o)
{
    o->menu = this;
    objects.push_back (o);
}
void Menu::FocusMenuObject(MenuObject* obj)
{
    if (obj != focussed)
    {
        // uset the old
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

    // The last object Rendered, is the focussed object
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

    // Mouse wheel events are sent through to anything under the mouse
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
    for (std::list<MenuObject*>::iterator it = objects.begin(); it != objects.end(); it++)
    {
        MenuObject *pObj = *it;

        pObj->Update (dt);
    }
}
void Menu::Render()
{
    int mX,mY;
    SDL_GetMouseState (&mX, &mY);

    // Determine over which object the mouse rolls:
    MenuObject* mouseOverObj = NULL;
    if (focussed && focussed->MouseOver ((GLfloat)mX, (GLfloat)mY))
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

    // Rendering the mouse cursor
    if (pClient->WindowFocus())
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

    textMask=mask;
    if(textMask)
    {
        showText=new char[maxTextLength+1];
        showText[0]=NULL;
    }
    else showText=NULL;

    cursor_time=button_time=0;
    lastCharKey=NULL;
    lastCharKeyChar=NULL;
}
void TextInputBox::UpdateShowText()
{
    if(textMask)
    {
        int i;
        for(i=0; text[i]; i++)
            showText[i] = textMask;
        showText [i] = NULL;
    }
}
TextInputBox::~TextInputBox()
{
    delete[] showText;
    delete[] text;
}
int TextInputBox::GetCursorPos() const
{
    return cursorPos;
}
void TextInputBox::GetCursorCoords(GLfloat& cx, GLfloat& cy) const
{
    if(pFont)
    {
        char* t = textMask ? showText : text;

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
    int start = (cursorPos<fixedCursorPos) ? cursorPos : fixedCursorPos,
        end = (cursorPos>fixedCursorPos) ? cursorPos : fixedCursorPos;

    if (start >= end)
        return;

    std::string ctxt;
    if (textMask)
        ctxt = std::string (showText + start, end - start);
    else
        ctxt = std::string (text + start, end - start);

    SDL_SetClipboardText (ctxt.c_str());
}
void TextInputBox::PasteText()
{
    int start = (cursorPos < fixedCursorPos)? cursorPos : fixedCursorPos,
        end = (cursorPos > fixedCursorPos)? cursorPos : fixedCursorPos,
        n, i,
        space,
        charsPasted;

    if(end > start) // Some text was selected
    {
        ClearText (start, end);
    }

    n = strlen(text);
    space = maxTextLength - n; // space remaining for fill

    // move everything maximally to the right:
    // (including null)
    for(i=n+1; i>=start; i--)
    {
        text[i + space] = text[i];
    }

    strncpy (text + start, SDL_GetClipboardText (), space);
    charsPasted = std::min ((int)strlen (SDL_GetClipboardText ()), space);

    cursorPos = fixedCursorPos = start + charsPasted;

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

    UpdateShowText();
}
void TextInputBox::RenderText() const
{
    char* t=text;
    if(textMask)
    {
        t=showText;
    }

    glTranslatef (x, y, 0.0f);
    glRenderText (pFont, t, textAlign);
    glTranslatef (-x, -y, 0.0f);
}
void TextInputBox::RenderTextCursor() const
{
    glPushAttrib (GL_TEXTURE_BIT | GL_CURRENT_BIT);

    GLfloat alpha = 0.6f + 0.3f * sin (10 * cursor_time);

    glDisable (GL_TEXTURE_2D);

    GLfloat clr [4];
    glGetFloatv (GL_CURRENT_COLOR, clr);

    glColor4f (clr[0], clr[1], clr[2], clr[3] * alpha);

    int selectionStart = std::min (cursorPos, fixedCursorPos),
        selectionEnd = std::max (cursorPos, fixedCursorPos);

    if (selectionStart == selectionEnd)
    // no text selected, just render a single square at the cursor position
    {
        GLfloat h1,h2,cx,cy,cx2;

        GetCursorCoords (cx,cy);

        h1 = -pFont->horiz_origin_y;
        h2 = h1 + GetLineSpacing (pFont);

        glBegin(GL_QUADS);
            glVertex2f(cx - 2.0f, cy + h1);
            glVertex2f(cx + 2.0f, cy + h1);
            glVertex2f(cx + 2.0f, cy + h2);
            glVertex2f(cx - 2.0f, cy + h2);
        glEnd();
    }
    else
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
bool TextInputBox::MouseOver(GLfloat mX, GLfloat mY) const
{
    char* t=textMask?showText:text;

    int cpos = WhichGlyphAt (pFont, t, (GLfloat)(mX - x),(GLfloat)(mY - y), textAlign),
        n = strlen(t);

    if (cpos >= 0)
    {
        GLfloat cx1,cy1,cx2,cy2;

        CoordsOfGlyph (pFont, t, 0, cx1, cy1, textAlign);
        cx1 += x; cy1 += y;

        CoordsOfGlyph (pFont, t, n, cx2, cy2, textAlign);
        cx2 += x; cy2 += y;

        if (mX > (cx1 - TEXTSELECT_XMARGE) && mX < (cx2 + TEXTSELECT_XMARGE))
        {
            return true;
        }
    }
    return false;
}
void TextInputBox::OnDelChar(char){}
void TextInputBox::OnAddChar(char){}
void TextInputBox::OnBlock(){}
void TextInputBox::OnMoveCursor(int direction){}
void TextInputBox::ClearText(int start, int end)
{
    int n = strlen(text),i,d;
    if(end>n) end=n;

    d=end-start;
    if(d<0) return;

    for(i=start; i<(n-d+1); i++)
    {
        text[i]=text[i+d];
    }
    UpdateShowText();
}
void TextInputBox::BackspaceProc()
{
    int n = strlen(text);
    if(n>0)
    {
        if(fixedCursorPos>cursorPos)
        {
            ClearText(cursorPos,fixedCursorPos);
            fixedCursorPos=cursorPos;
        }
        else if(fixedCursorPos<cursorPos)
        {
            ClearText(fixedCursorPos,cursorPos);
            cursorPos=fixedCursorPos;
        }
        else if(cursorPos>0)
        {
            OnDelChar(text[cursorPos-1]);

            ClearText(cursorPos-1,cursorPos);
            cursorPos--;
            fixedCursorPos=cursorPos;
        }
        else OnBlock();
    }
    else OnBlock();

    UpdateShowText();
}
void TextInputBox::DelProc()
{
    int n = strlen(text);
    if(n>0)
    {
        if(fixedCursorPos>cursorPos)
        {
            ClearText(cursorPos,fixedCursorPos);
            fixedCursorPos=cursorPos;
        }
        else if(fixedCursorPos<cursorPos)
        {
            ClearText(fixedCursorPos,cursorPos);
            cursorPos=fixedCursorPos;
        }
        else if(cursorPos<n)
        {
            OnDelChar(text[cursorPos]);

            ClearText(cursorPos,cursorPos+1);
            fixedCursorPos=cursorPos;
        }
        else OnBlock();
    }
    else OnBlock();

    UpdateShowText();
}
void TextInputBox::LeftProc()
{
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    const bool shift = keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT];

    cursorPos--;

    if(cursorPos<0)
    {
        cursorPos=0;
        OnBlock();
    }
    else
        OnMoveCursor(-1);

    if (!shift)
        fixedCursorPos=cursorPos;

    UpdateShowText();
}
void TextInputBox::RightProc()
{
    int n = strlen(text);

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    const bool shift = keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT];

    cursorPos++;
    if(!shift)
        fixedCursorPos=cursorPos;

    if(cursorPos>n)
    {
        cursorPos=n;
        OnBlock();
    }
    else
        OnMoveCursor(1);

    if(!shift)
        fixedCursorPos=cursorPos;

    UpdateShowText();
}
void TextInputBox::AddCharProc(char c)
{
    // clear the selection first
    if(fixedCursorPos > cursorPos)
    {
        ClearText (cursorPos,fixedCursorPos);
        fixedCursorPos = cursorPos;
    }
    else if(fixedCursorPos < cursorPos)
    {
        ClearText (fixedCursorPos,cursorPos);
        cursorPos = fixedCursorPos;
    }

    int n = strlen(text), i;

    if (n < maxTextLength) // room for more?
    {
        // move everything on the right of the cursor to the right
        for(i = n + 1; i>cursorPos; i--)
        {
            text[i]=text[i-1]; // also moves the null
        }
        text[cursorPos] = c;

        cursorPos++;
        fixedCursorPos = cursorPos;

        OnAddChar(c);
    }
    else
    {
        OnBlock();
    }

    UpdateShowText();
}
#define TEXT_BUTTON_INTERVAL 0.03f
#define FIRST_TEXT_BUTTON_INTERVAL 0.3f
void TextInputBox::Update(const float dt)
{
    if(IsFocussed())
    {
        cursor_time += dt;

        if (button_time<0)
            button_time += dt;

        const Uint8 *keystate = SDL_GetKeyboardState(NULL);

        if (button_time >= 0)
        {
            if ( keystate[SDL_SCANCODE_BACKSPACE] )
            {
                BackspaceProc();
                button_time=-TEXT_BUTTON_INTERVAL;
            }
            else if( keystate[SDL_SCANCODE_DELETE] )
            {
                DelProc();
                button_time=-TEXT_BUTTON_INTERVAL;
            }
            else if( keystate[SDL_SCANCODE_LEFT] )
            {
                LeftProc();
                button_time=-TEXT_BUTTON_INTERVAL;
            }
            else if( keystate[SDL_SCANCODE_RIGHT] )
            {
                RightProc();
                button_time=-TEXT_BUTTON_INTERVAL;
            }

            if (keystate [lastCharKey])
            {
                AddCharProc(lastCharKeyChar);
                button_time=-TEXT_BUTTON_INTERVAL;
            }
            else
            {
                lastCharKey = NULL;
                lastCharKeyChar = NULL;
            }
        }

        UpdateShowText();
    }
}
void TextInputBox::OnMouseMove(const SDL_MouseMotionEvent *event)
{
    if (event->state & SDL_BUTTON_LMASK)
    {
        char* t = text;
        if (textMask)
        {
            t=showText;
        }
        int cpos = WhichGlyphAt (pFont, t, (GLfloat)(event->x - x), (GLfloat)(event->y - y), textAlign);
//        int cpos = font->IndexCursorPos (x, y, t, textAlign, (GLfloat)event->x, (GLfloat)event->y);

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
    char* t = text;
    if (textMask)
    {
        t = showText;
    }
    int cpos, prev_fixedCursorPos = fixedCursorPos;

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    cpos = WhichGlyphAt (pFont, t, event->x - x, event->y - y, textAlign);
    if (cpos >= 0)
    {
        if (event->clicks > 1)
            WordAt (t, cpos, fixedCursorPos, cursorPos);
        else
            // place the cursor at the left side of the selected character
            fixedCursorPos = cursorPos = cpos;

        if(keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT])
            fixedCursorPos = prev_fixedCursorPos;
    }
}
void TextInputBox::OnKeyPress (const SDL_KeyboardEvent *event)
{
    const SDL_Keycode sym = event->keysym.sym;
    const SDL_Scancode scn = event->keysym.scancode;
    const Uint16 mod = event->keysym.mod;

    bool bCTRL = mod & KMOD_CTRL;

    if (sym == SDLK_TAB || sym == SDLK_RETURN)
    {
        // ignore '\t' and '\n'
        return;
    }
    else if (bCTRL && sym == SDLK_c)
    {
        CopySelectedText();
    }
    else if (bCTRL && sym == SDLK_v)
    {
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

    if (IsFocussed())
        RenderTextCursor();

    glPopAttrib();
}
void TextInputBox::SetText (const char* text)
{
    strncpy (this->text, text, maxTextLength - 1);
    this->text [maxTextLength] = NULL;

    int n = strlen(this->text);
    cursorPos = fixedCursorPos = n;

    UpdateShowText ();
}
