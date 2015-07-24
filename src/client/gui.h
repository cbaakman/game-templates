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


#ifndef GUI_H
#define GUI_H

#include <list>

#include "client.h"

#include "../font.h"

class Menu;

/*
 * Base class for a GUI menu item.
 */
class MenuObject : public EventListener
{
private:
    // automatically set by 'Menu':
    Menu* menu;
    bool focussed, enabled;
protected:
    virtual void RenderCursor (const int mX, const int mY);

    Menu *GetMenu () const { return menu; }

public:
    void SetEnabled (const bool b) { enabled = b; }
    void SetFocus (const bool);

    bool IsFocussed () const {return focussed;}
    bool IsInputEnabled () const;

    virtual bool MouseOver (const GLfloat mouse_x, const GLfloat mouse_y) const = 0;

    virtual void OnFocusGain () {}
    virtual void OnFocusLose () {}

    virtual void Render () {}
    virtual void Update (const float dt) {}

    // used when tab is pressed
    virtual MenuObject* NextInLine() const;

    MenuObject();

friend class Menu;
};


/*
 * A Graphical menu, that handles events and rendering for the underlying menu objects.
 */
class Menu : public EventListener
{
private:
    Client *pClient;

    std::list <MenuObject*> objects;
    MenuObject* focussed;
    bool inputEnabled;

public:
    /*
     * called when the default cursor is needed
     */
    virtual void RenderDefaultCursor (const int mX, const int mY) = 0;

    Menu (Client *);
    virtual ~Menu();

    void DisableInput();
    void EnableInput();

    bool IsInputEnabled () const { return inputEnabled; }

    void DisableObjectFocus (MenuObject* obj);
    void EnableObjectFocus (MenuObject* obj);

    MenuObject* FocussedObject () { return focussed; }
    void FocusMenuObject (MenuObject *); // may be NULL

    virtual void OnEvent (const SDL_Event *);

    virtual void OnMouseClick (const SDL_MouseButtonEvent *event);
    virtual void OnKeyPress (const SDL_KeyboardEvent *event);
    virtual void OnMouseWheel (const SDL_MouseWheelEvent *event);

    void AddMenuObject (MenuObject *); // is autodeleted when the menu is deleted

    virtual void Render();
    virtual void Update(const float dt);
};

/*
 * This is where users can insert text. The text can be hidden (passwords) by setting a mask char.
 * Text in the input box is selectable by mouse and SHIFT + arrow keys and can be copied to the clipboard with Ctrl + C.
 * It depends on a font to know its dimensions.
 */
class TextInputBox : public MenuObject
{
private:
    char*text,
        *showText,
        textMask;
    int maxTextLength,
        cursorPos,
        fixedCursorPos;
    const Font *pFont;
    float button_time, cursor_time;
    int lastCharKey;
    char lastCharKeyChar;

    GLfloat x,y;
    int textAlign;

    void UpdateShowText();
    void ClearText(const int start, const int end);

    void AddCharProc(char c);
    void BackspaceProc();
    void DelProc();
    void LeftProc();
    void RightProc();

    void RenderText() const;

    void RenderTextCursor() const;

    void CopySelectedText() const;
    void PasteText();

protected:
    void SetX (GLfloat _x) { x = _x; }
    void SetY (GLfloat _y) { y = _y; }

    GLfloat GetX() const { return x; }
    GLfloat GetY() const { return y; }
    const Font *GetFont() const { return pFont; }

    int GetCursorPos() const;

    void GetCursorCoords(GLfloat& cx, GLfloat& cy) const;
    void GetFixedCursorCoords(GLfloat& cx, GLfloat& cy) const;

    // extra effects when something happens:
    virtual void OnDelChar(char);
    virtual void OnAddChar(char);
    virtual void OnMoveCursor(int direction); // -1: left, 1: right
    virtual void OnBlock();
    virtual void OnFocusGain();
    virtual void OnFocusLose();

    void OnMouseClick (const SDL_MouseButtonEvent *event);
    void OnMouseMove (const SDL_MouseMotionEvent *event);
    void OnKeyPress (const SDL_KeyboardEvent *event);

public:
    TextInputBox (const GLfloat x, const GLfloat y,
        const Font*,
        const int maxTextLength,
        const int textAlign,
        const char mask=NULL);

    virtual ~TextInputBox();

    virtual bool MouseOver (GLfloat mX, GLfloat mY) const;
    virtual void Render ();
    virtual void Update (const float dt);

    void SetText (const char* text);
    const char* GetText () const;

friend class Menu;
};
#endif //GUI_H
