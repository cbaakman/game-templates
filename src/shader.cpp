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

const char
    water_vsh [] = R"shader(

varying vec3 normal;

    void main()
    {
        normal=gl_Normal;

        gl_Position = ftransform();

        gl_TexCoord[0].s = 0.5 * gl_Position.x / gl_Position.w + 0.5;
        gl_TexCoord[0].t = 0.5 * gl_Position.y / gl_Position.w + 0.5;
    }

)shader",

water_fsh [] = R"shader(

    uniform sampler2D tex_reflect,
                      tex_refract,
                      tex_reflect_depth,
                      tex_refract_depth;

    varying vec3 normal;
    varying vec4 water_level_position;

    const vec3 mirror_normal = vec3(0.0, 1.0, 0.0);

    const float refrac = 1.33; // air to water

    void main()
    {
        float nfac = dot (normal, mirror_normal);

        vec2 reflectOffset;
        if (nfac > 0.9999) // clamp reflectOffset to prevent glitches
        {
            reflectOffset = vec2(0.0, 0.0);
        }
        else
        {
            vec3 flat_normal = normal - nfac * mirror_normal;

            vec3 eye_normal = normalize (gl_NormalMatrix * flat_normal);

            reflectOffset = eye_normal.xy * length (flat_normal) * 0.1;
        }

        // The deeper under water, the more distortion:

        float reflection_depth = min (texture2D (tex_reflect_depth, gl_TexCoord[0].st).r,
                                      texture2D (tex_reflect_depth, gl_TexCoord[0].st + reflectOffset.xy).r),
              refraction_depth = min (texture2D (tex_refract_depth, gl_TexCoord[0].st).r,
                                      texture2D (tex_refract_depth, gl_TexCoord[0].st - refrac * reflectOffset.xy).r),

              // diff_reflect and diff_refract will be linear representations of depth:

              diff_reflect = clamp (30 * (reflection_depth - gl_FragCoord.z) / gl_FragCoord.w, 0.0, 1.0),
              diff_refract = clamp (30 * (refraction_depth - gl_FragCoord.z) / gl_FragCoord.w, 0.0, 1.0);

        vec4 reflectcolor = texture2D (tex_reflect, gl_TexCoord[0].st + diff_reflect * reflectOffset.xy),
             refractcolor = texture2D (tex_refract, gl_TexCoord[0].st - diff_refract * refrac * reflectOffset.xy);

        float Rfraq = normalize (gl_NormalMatrix * normal).z;
        Rfraq = Rfraq * Rfraq;

        gl_FragColor = reflectcolor * (1.0 - Rfraq) + refractcolor * Rfraq;
    }

)shader",

toon_vsh [] = R"shader(

    varying vec3 N;
    varying vec3 v;
    varying vec4 color;

    void main()
    {
        v = vec3 (gl_ModelViewMatrix * gl_Vertex);

        N = normalize (gl_NormalMatrix * gl_Normal);

        color = gl_Color;

        gl_Position = ftransform();
    }

)shader",

toon_fsh [] = R"shader(

    uniform float cutOff = 0.8;

    varying vec3 N;
    varying vec3 v;
    varying vec4 color;

    void main()
    {
        vec3 L = normalize(gl_LightSource[0].position.xyz - v);

        float lum = clamp (dot(normalize (N), L) + 0.5, 0.0, 1.0);

        if (lum < cutOff)
            lum = cutOff;
        else
            lum = 1.0;

        gl_FragColor = color * lum;
    }

)shader";

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
