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

#include "err.h"
#include "shader.h"

/**
 * Compile either the source of a vertex or fragment shader.
 * :param type: either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * :returns: OpenGL handle to shader, or 0 on error
 */
GLuint CreateShader(const std::string& source, int type)
{
    if(type != GL_VERTEX_SHADER && type != GL_FRAGMENT_SHADER)
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

        if (type == GL_VERTEX_SHADER)

            SetError ("Error compiling vertex shader: %s", errorString);

        else if (type == GL_FRAGMENT_SHADER)

            SetError ("Error compiling fragment shader: %s", errorString);

        delete [] errorString;

        glDeleteShader (shader);
        return 0;
    }

    return shader;
}

GLuint CreateShaderProgram(const std::string& sourceVertex, const std::string& sourceFragment)
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
