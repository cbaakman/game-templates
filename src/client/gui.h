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

/**
 * Base class for a GUI menu item.
 */
class MenuObject : public EventListener
{
private:

    /*
        The menu variable is automatically set by the menu itself when
        this object is added to it. However, it's null when the menu object
        is not attached to any menu.
     */
    Menu* menu;

    bool focussed, // means this is the current menu object that recieves input from the menu.
         enabled; // when disabled, it doesn't recieve mouse input.

protected:
    // Override this if the cursor should look different when hovering over this object.
    virtual void RenderCursor (const int mX, const int mY);

    Menu *GetMenu () const { return menu; }

public:
    // These functions are usefull when using a menu object without a menu.
    void SetEnabled (const bool b) { enabled = b; }
    void SetFocus (const bool);

    bool IsFocussed () const {return focussed;}
    bool IsInputEnabled () const;

    // Must implement this, decides whether mouse cursor hovers over or not.
    virtual bool MouseOver (const GLfloat mouse_x, const GLfloat mouse_y) const = 0;

    // Override these to take special actions when the object gains or loses menu focus.
    virtual void OnFocusGain () {}
    virtual void OnFocusLose () {}

    virtual void Render () {}
    virtual void Update (const float dt) {}

    // used when tab is pressed, returns NULL by default
    virtual MenuObject* NextInLine() const;

    MenuObject();

friend class Menu;
};


/**
 * This class represents a graphical menu, that handles events and rendering for the
 * underlying menu objects.
 *
 * It's not necessary for menu objects. A menu is only usefull when there are multiple
 * menu objects, that shouldn't all recieve input at the same time. When used, this object
 * decides which menu object has input focus.
 *
 * When the menu objects should all recieve input at the same time, don't use this class.
 *
 * A menu object:
 * - recieves focus from mouse click or tab key press
 * - knows itself when it's focussed or not
 * - has the ability to decide what the mouse cursor looks like.
 */
class Menu : public EventListener
{
private:
    Client *pClient;

    // All objects in this menu:
    std::list <MenuObject*> objects;

    // currently focussed object:
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
    void FocusMenuObject (MenuObject *); // may be NULL to focus nothing

    virtual void OnEvent (const SDL_Event *);
    virtual void OnMouseClick (const SDL_MouseButtonEvent *);
    virtual void OnKeyPress (const SDL_KeyboardEvent *);
    virtual void OnMouseWheel (const SDL_MouseWheelEvent *);
    virtual void OnMouseMove (const SDL_MouseMotionEvent *);
    virtual void OnTextInput (const SDL_TextInputEvent *);

    /*
        Any object added with AddMenuObject is automatically deleted when
        the menu gets deleted.
     */
    void AddMenuObject (MenuObject *);

    virtual void Render();
    virtual void Update(const float dt);
};

/**
 * This is where users can insert text. The text can be hidden (passwords) by setting a mask char.
 * Text in the input box is selectable by mouse and SHIFT + arrow keys and can be copied to the clipboard with Ctrl + C.
 * It depends on a font to know its dimensions.
 *
 * This has not been tested with multiline input text.
 * The up and down keys haven't been implemented for this class.
 */
class TextInputBox : public MenuObject
{
private:

    char *text, // actual inputted text
         *showText, // only used when text is masked

         textMask; // mask char, NULL means no mask

    int maxTextLength,

        /*
           cursorPos is the current cursor position,
           fixedCursorPos is where selection started
           when either shift or the left mouse key was down.

           Both are measured in utf-8 character positions.
         */
        cursorPos,
        fixedCursorPos;

    /*
       The font is necessary,
       it determines the coordinates of the characters and is needed for rendering.

       x, y and textAlign are just as important for this!
     */
    const Font *pFont;
    GLfloat x, y; // render position of the text
    int textAlign; // one of the defined TEXTALIGN constants

    // Variables used to measure time passed since certain events:
    float button_time,
          cursor_time;

    // This changes the masked text, must be called when input text changes.
    void UpdateShowText();

    // Removes the characters at given positions from the text string.
    void ClearText (const std::size_t start, const std::size_t end);

    /*
       The following routines are called whenever a key press
       changes something to the state of the input box.
     */
    void AddCharProc (const char *);
    void BackspaceProc ();
    void DelProc ();
    void LeftProc (); // when left key is pressed
    void RightProc (); // when right key is pressed

    // Render subroutines:
    void RenderText() const;
    void RenderTextCursor() const;

    // Used for copy and paste actions:
    void CopySelectedText() const;
    void PasteText();

protected:

    /*
        OnDelChar, OnAddChar, OnMoveCursor and OnBlock
        do nothing by default, but they are called when the corresponding change occurs.
        They can be overridden.
     */
    virtual void OnDelChar (const unicode_char);
    virtual void OnAddChar (const unicode_char);
    virtual void OnMoveCursor (int direction); // -1: left, 1: right
    virtual void OnBlock (); // user tries to move the cursor, but can't go any further

    virtual void OnFocusGain ();
    virtual void OnFocusLose ();

    // The input box needs to respond to the following input events:
    void OnMouseClick (const SDL_MouseButtonEvent *);
    void OnMouseMove (const SDL_MouseMotionEvent *);
    void OnKeyPress (const SDL_KeyboardEvent *);
    void OnTextInput (const SDL_TextInputEvent *);

public:
    TextInputBox (const GLfloat x, const GLfloat y,
        const Font*,
        const int maxTextLength,
        const int textAlign,
        const char mask=NULL);

    virtual ~TextInputBox ();

    // menu object methods, that the tex box implements:
    virtual bool MouseOver (GLfloat mX, GLfloat mY) const;

    /*
        Update and Render can be overridden, but MUST be called
        in their derived classes for them to work properly.
     */
    virtual void Render ();
    virtual void Update (const float dt);


    // Getters and setters:
    void SetText (const char* text);
    const char* GetText () const;

    void SetX (GLfloat _x) { x = _x; }
    void SetY (GLfloat _y) { y = _y; }

    GLfloat GetX() const { return x; }
    GLfloat GetY() const { return y; }

    const Font *GetFont () const { return pFont; }

    int GetCursorPos () const;

    // These two functions get the absolute coordinates of the cursor in the text:
    void GetCursorCoords (GLfloat& cx, GLfloat& cy) const;
    void GetFixedCursorCoords (GLfloat& cx, GLfloat& cy) const;

friend class Menu;
};
#endif //GUI_H
