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

/**
 * Compile either the source of a vertex or fragment shader.
 * :param type: either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * :returns: OpenGL handle to shader, or 0 on error
 */
GLuint CreateShader(const std::string& source, GLenum type)
{
    char typeName [100];
    GetShaderTypeName (type, typeName);

    if (!typeName [0])
    {
        SetError ("Incorrect shader type: %.4X", type);
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
GLuint CreateShaderProgram (const char *sourceVertex, const char *sourceFragment)
{
    return CreateShaderProgram (std::string (sourceVertex), std::string (sourceFragment));
}
GLuint CreateShaderProgram (const std::string& sourceVertex, const std::string& sourceFragment)
{
    GLint result = GL_FALSE;
    int logLength;
    GLuint program=0, vertexShader, fragmentShader;

    // Compile the sources:
    vertexShader = CreateShader (sourceVertex, GL_VERTEX_SHADER);
    fragmentShader = CreateShader (sourceFragment, GL_FRAGMENT_SHADER);

    if (vertexShader && fragmentShader)
    {
        // Combine the compiled shaders into a program:
        program = glCreateProgram();

        glAttachShader (program, vertexShader);
        glAttachShader (program, fragmentShader);

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

            glDeleteShader (vertexShader);
            glDeleteShader (fragmentShader);
            glDeleteProgram (program);
            return 0;
        }
    }

    // Not needed anymore at this point:
    glDeleteShader (vertexShader);
    glDeleteShader (fragmentShader);

    return program;
}
