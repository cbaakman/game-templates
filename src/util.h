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


#ifndef UTIL_H
#define UTIL_H

#include <GL/glew.h>
#include <GL/gl.h>
#include <SDL2/SDL.h>

#include <list>
#include <string>
#include <cmath>
#include "matrix.h"
#include "geo2d.h"
#include <libxml/tree.h>
#include <cstring>

#include "io.h"

/**
 * square function, x must be a number
 */
template <typename T>
T sqr (T x)
{
    return x * x;
}

/**
 * Swaps the values of n1 and n2
 */
template <class Value>
void swap (Value& n1, Value& n2)
{
    Value r=n1;
    n1=n2;
    n2=r ;
}

/**
 * Converts the SDL key code to an ascii character
 *
 * Only works for american key board setting.
 */
char GetKeyChar (const SDL_KeyboardEvent *event);

/**
 * Reads the entire content of an input source to a string
 */
bool ReadAll (SDL_RWops *, std::string &out);

/**
 * Converts a GL error status to a string.
 */
void GLErrorString (char *out, GLenum status);

/**
 * Gives a h,s,v color to OpenGL
 */
void glColorHSV (float h, float s, float v);

/**
 * Check GL status and sets error string if not OK.
 * See err.h
 */
bool CheckGLOK (const char *doing);

/**
 * Sets memory to zeros.
 */
void Zero(void *p, size_t size);

/*
    Shortcuts to give vec and matrix objects to OpenGL:
 */
inline void glVertex2f(const vec2& v)
{
    glVertex2f((GLfloat)v.x,(GLfloat)v.y);
}
inline void glVertex3f(const vec3& v)
{
    glVertex3fv((GLfloat*)v.v);
}
inline void glNormal3f(const vec3& v)
{
    glNormal3fv((GLfloat*)v.v);
}
inline void glLoadMatrixf(const matrix4& m)
{
    glLoadMatrixf((GLfloat*)m.m);
}
inline void glMultMatrixf(const matrix4& m)
{
    glMultMatrixf((GLfloat*)m.m);
}
#endif // UTIL_H
