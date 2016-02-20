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


#ifndef GL_UTIL_H
#define GL_UTIL_H

#include <GL/glew.h>
#include <GL/gl.h>

#include "matrix.h"
#include "geo2d.h"

/**
 * Converts a GL error status to a string.
 */
void GLErrorString (char *out, GLenum status);

/**
 * Converts a GL framebuffer error status to a string.
 */
void GLFrameBufferErrorString (char *out, GLenum status);

/**
 * Renders a S x S x S cube at center P, parallel to the current x, y and z axes.
 */
void RenderCube (const vec3 &p, const float s);

/**
 * Gives a h,s,v color to OpenGL
 */
void glColorHSV (float h, float s, float v);

/**
 * Check GL status and sets error string if not OK.
 * See err.h
 */
bool CheckGLOK (const char *doing);

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
#endif // GL_UTIL_H
