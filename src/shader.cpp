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


#include <string>
#include <list>
#include <stdio.h>
#include <cstring>

#include "err.h"
#include "shader.h"

void GetShaderTypeName (GLenum type, char *pOut)
{
    switch (type)
    {
    case GL_VERTEX_SHADER:
        strcpy (pOut, "vertex");
        break;
    case GL_FRAGMENT_SHADER:
        strcpy (pOut, "fragment");
        break;
    case GL_GEOMETRY_SHADER:
        strcpy (pOut, "geometry");
        break;
    case GL_COMPUTE_SHADER:
        strcpy (pOut, "compute");
        break;
    case GL_TESS_CONTROL_SHADER:
        strcpy (pOut, "tessellation control");
        break;
    case GL_TESS_EVALUATION_SHADER:
        strcpy (pOut, "tessellation evaluation");
        break;
    default:
        strcpy (pOut, "");
    };
}

GLuint CreateShader(const std::string& source, GLenum type)
{
    char typeName [100];
    GetShaderTypeName (type, typeName);

    if (!typeName [0])
    {
        SetError ("Unknown shader type: %.4X", type);
        return 0;
    }

    GLint result;
    int logLength;
    GLuint shader = glCreateShader (type);

    // Compile
    const char *src_ptr = source.c_str();
    glShaderSource (shader, 1, &src_ptr, NULL);
    glCompileShader (shader);

    glGetShaderiv (shader, GL_COMPILE_STATUS, &result);
    if(result!=GL_TRUE)
    {
        // Error occurred, get compile log:
        glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &logLength);

        char *errorString = new char [logLength + 1];
        glGetShaderInfoLog (shader, logLength, NULL, errorString);

        SetError ("Error compiling %s shader: %s", typeName, errorString);

        delete [] errorString;

        glDeleteShader (shader);
        return 0;
    }

    return shader;
}
GLuint CreateShaderProgram (const std::list <GLuint>& shaders);
GLuint CreateShaderProgram (const GLuint s1)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const GLuint s1, const GLuint s2)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    shaders.push_back (s2);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const GLuint s1, const GLuint s2, const GLuint s3)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    shaders.push_back (s2);
    shaders.push_back (s3);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const GLuint s1, const GLuint s2, const GLuint s3,
                            const GLuint s4)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    shaders.push_back (s2);
    shaders.push_back (s3);
    shaders.push_back (s4);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const GLuint s1, const GLuint s2, const GLuint s3,
                            const GLuint s4, const GLuint s5)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    shaders.push_back (s2);
    shaders.push_back (s3);
    shaders.push_back (s4);
    shaders.push_back (s5);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const GLuint s1, const GLuint s2, const GLuint s3,
                            const GLuint s4, const GLuint s5, const GLuint s6)
{
    std::list <GLuint> shaders;
    shaders.push_back (s1);
    shaders.push_back (s2);
    shaders.push_back (s3);
    shaders.push_back (s4);
    shaders.push_back (s5);
    shaders.push_back (s6);
    return CreateShaderProgram (shaders);
}
GLuint CreateShaderProgram (const std::list <GLuint>& shaders)
{
    GLint result = GL_FALSE;
    int logLength;
    GLuint program=0;

    // Combine the compiled shaders into a program:
    program = glCreateProgram();

    for (auto shader : shaders)
        glAttachShader (program, shader);

    glLinkProgram (program);

    glGetProgramiv (program, GL_LINK_STATUS, &result);
    if (result != GL_TRUE)
    {
        // Error occurred, get log:

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        char *errorString = new char [logLength];
        glGetProgramInfoLog (program, logLength, NULL, errorString);
        SetError ("Error linking shader program: %s", errorString);
        delete [] errorString;

        glDeleteProgram (program);
        return 0;
    }

    return program;
}
