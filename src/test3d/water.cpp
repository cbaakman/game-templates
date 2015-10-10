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


#include "water.h"

#include "../matrix.h"
#include "../shader.h"

#include "../random.h"

#include "../err.h"

#include "../util.h"

#define PLANE_Y -2.0
#define DROP_INTERVAL 0.3f
#define WATERSIZE 10.0f
#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f
#define CUBESIZE 2.0f

#define GLSL(src) #src

WaterScene::WaterScene (App *pApp) : Scene(pApp),

    fbReflection(0), texReflection(0), texReflecDepth(0),
    fbRefraction(0), texRefraction(0), texRefracDepth(0),
    shaderProgramWater(0),

    timeDrop(0),
    angleY(PI/4), angleX(PI/4), distCamera(10.0f),

    posCube(0.0f, 1.5f, 0.0f)
{
    float offset = -0.5f * WATERSIZE;
    float spacing = WATERSIZE / (GRIDSIZE - 1);

    // build a grid to represent the moving water

    for(int x=0; x<GRIDSIZE; x++)
    {
        for(int z=0; z<GRIDSIZE; z++)
        {
            gridnormals[x][z] = vec3 (0.0f, 1.0f, 0.0f);
            gridpoints[x][z] = vec3 (x * spacing + offset, 0, z * spacing + offset);
            gridspeeds[x][z] = 0.0f;
            gridforces[x][z] = 0.0f;
        }
    }
}

const char

        // In this shader, given vertices are simply interpreted as if in clip space.
        srcClipVertex [] = "void main () {gl_Position = gl_Vertex;}",

        // This shader sets depth to 1.0 everywhere.
        srcDepth1Fragment [] = "uniform vec4 color; void main () {gl_FragColor = color; gl_FragDepth = 1.0;}",

// Vector shader for the water's surface:
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

// Fragment shader for the water's surface:
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

)shader";

bool WaterScene::Init()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    GLenum status;
    char errBuf [100];

    /*
        Build two framebuffers for reflection and refraction.
        Each framebuffer gets a color and depth-stencil texture.

        The color and depth values will be used by the pixel shader.
        The stencil buffer is needed during render to texture.
     */

    glGenFramebuffers (1, &fbReflection);
    glGenFramebuffers (1, &fbRefraction);

    glGenTextures (1, &texReflection);
    glGenTextures (1, &texRefraction);
    glGenTextures (1, &texReflecDepth);
    glGenTextures (1, &texRefracDepth);

    glBindFramebuffer (GL_FRAMEBUFFER, fbReflection);

    glBindTexture(GL_TEXTURE_2D, texReflection);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texReflection, 0);

    glBindTexture(GL_TEXTURE_2D, texReflecDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texReflecDepth, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        GLFrameBufferErrorString (errBuf, status);

        SetError ("error, reflection framebuffer is not complete: %s", errBuf);
        return false;
    }

    glBindFramebuffer (GL_FRAMEBUFFER, fbRefraction);

    glBindTexture (GL_TEXTURE_2D, texRefraction);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glFramebufferTexture (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texRefraction, 0);

    glBindTexture (GL_TEXTURE_2D, texRefracDepth);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture (GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, texRefracDepth, 0);

    status = glCheckFramebufferStatus (GL_FRAMEBUFFER);

    glBindFramebuffer (GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        GLFrameBufferErrorString (errBuf, status);

        SetError ("error, refraction framebuffer is not complete: %s", errBuf);
        return false;
    }

    // Create shader program:
    shaderProgramWater = CreateShaderProgram (water_vsh, water_fsh);
    if (!shaderProgramWater)
    {
        SetError ("error creating water shader program : %s", GetError ());
        return false;
    }

    shaderProgramDepth = CreateShaderProgram (srcClipVertex, srcDepth1Fragment);
    if (!shaderProgramDepth)
    {
        SetError ("error creating depth shader program: %s", GetError ());
        return false;
    }

    return true;
}
WaterScene::~WaterScene()
{
    glDeleteProgram(shaderProgramWater);
    glDeleteProgram(shaderProgramDepth);

    glDeleteTextures(1, &texReflection);
    glDeleteTextures(1, &texRefraction);
    glDeleteTextures(1, &texReflecDepth);
    glDeleteTextures(1, &texRefracDepth);

    glDeleteFramebuffers(1, &fbReflection);
    glDeleteFramebuffers(1, &fbRefraction);
}
void WaterScene::UpdateWaterNormals()
{
    // Recalculate normals, depending on the edges around the vertices.

    int x,z,x1,z1,x2,z2;
    vec3 p0,p1,p2;

    for (x = 0; x < GRIDSIZE; x++)
    {
        for (z = 0; z < GRIDSIZE; z++)
        {
            x1 = x;
            z1 = z;
            x2 = x + 1;
            z2 = z + 1;

            if (x2 >= GRIDSIZE)
            {
                x1--;
                x2--;
            }

            if (z2 >= GRIDSIZE)
            {
                z1--;
                z2--;
            }

            p0 = gridpoints [x1][z1];
            p1 = gridpoints [x2][z1];
            p2 = gridpoints [x1][z2];

            // Normal must point up, perpendicular to the two edges

            gridnormals [x][z] = Cross (p2 - p0, p1 - p0).Unit ();
        }
    }
}
void WaterScene::MakeWave (const vec3 p, const float l)
{
    // Move the grid points so that it looks like a wave at point p with length l

    float b = 2 * PI / l;

    float dist;
    for(int z=0; z<GRIDSIZE; z++)
    {
        for(int x=0; x<GRIDSIZE; x++)
        {
            // calculate distance

            dist = sqrt (sqr (gridpoints[x][z].x - p.x) + sqr (gridpoints [x][z].z - p.z));

            // Make the amplitude fade with distance

            gridpoints[x][z].y += p.y *cos (b * dist) * exp (-sqr (dist));
        }
    }
}
void WaterScene::CubeBlockWaves ()
{
    float x1 = posCube.x - CUBESIZE / 2, x2 = posCube.x + CUBESIZE / 2,
          y1 = posCube.y - CUBESIZE / 2, y2 = posCube.y + CUBESIZE / 2,
          z1 = posCube.z - CUBESIZE / 2, z2 = posCube.z + CUBESIZE / 2,

          smallestDist2;

    for (int x = 1; x < GRIDSIZE; x++)
    {
        for(int z = 1; z < GRIDSIZE; z++)
        {
            if (0 > y1 && 0 < y2)
            {
                if (gridpoints[x][z].x > x1 && gridpoints[x][z].x < x2 &&
                    gridpoints[x][z].z > z1 && gridpoints[x][z].z < z2)
                {
                    // Cancel all waves at the edge of the cube.

                    gridpoints[x][z].y = 0;
                }
            }
        }
    }
}

#define SQRT2DIV2 0.707106781186547524400844362104849f
void WaterScene::UpdateWater (const float dt)
{
    // Move the grid points to update the waves

    float d;

    // Init forces at zero
    for(int x=0; x<GRIDSIZE; x++)
    {
        for(int z=0; z<GRIDSIZE; z++)
        {
            gridforces[x][z] = 0.0f;
        }
    }

    // Add on all forces per grid point:
    for(int x=1; x<(GRIDSIZE-1); x++)
    {
        for(int z=1; z<(GRIDSIZE-1); z++)
        {
            // The forces are composed of the y level differences between points:

            d=gridpoints[x][z].y - gridpoints[x-1][z].y;
              gridforces[x][z]-=d;    gridforces[x-1][z]+=d;

            d=gridpoints[x][z].y - gridpoints[x+1][z].y;
              gridforces[x][z]-=d;    gridforces[x+1][z]+=d;

            d=gridpoints[x][z].y - gridpoints[x][z-1].y;
              gridforces[x][z]-=d;    gridforces[x][z-1]+=d;

            d=gridpoints[x][z].y - gridpoints[x][z+1].y;
              gridforces[x][z]-=d;    gridforces[x][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x-1][z+1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x-1][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x+1][z+1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x+1][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x-1][z-1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x-1][z-1]+=d;

            d=gridpoints[x][z].y - gridpoints[x+1][z-1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x+1][z-1]+=d;

            // friction:
            // gridforces[x][z] -= gridspeeds[x][z];
        }
    }

    float ft = 30.0f * dt;
    if (ft > 1.0f)
        ft = 1.0f;

    float u = (float)exp (-2.5 * dt);

    for(int x=0; x<GRIDSIZE; x++)
    {
        for(int z=0; z<GRIDSIZE; z++)
        {
            float force = 0.2f * gridforces[x][z] * ft;

            gridspeeds[x][z] += force;

            gridpoints[x][z].y += gridspeeds[x][z] * ft;
            gridpoints[x][z].y *= u;
        }
    }
}
void getCameraPosition(
        const GLfloat angleX, const GLfloat angleY, const GLfloat distCamera,
    vec3 &posCamera)
{
    // Place camera at chosen distance and angle
    posCamera = vec3 (0, 0, -distCamera);
    posCamera *= matRotY (angleY) * matRotX (angleX);
}
void WaterScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
void WaterScene::OnMouseMove(const SDL_MouseMotionEvent *event)
{
    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change the camera angles

        angleY += 0.01f * event -> xrel;
        angleX += 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < 0)
            angleX = 0;
    }
}
#define CUBE_SPEED 0.1f
void WaterScene::Update(float dt)
{
    /*
        Make waves with random position and size,
        over constant time intervals.
     */

    timeDrop += dt;
    if (timeDrop > DROP_INTERVAL)
    {
        timeDrop = 0;

        float x = RandomFloat (-0.5f * WATERSIZE, 0.5f * WATERSIZE),
              y = RandomFloat (0.55f, 1.0f),
              z = RandomFloat (-0.5f * WATERSIZE, 0.5f * WATERSIZE),
              l = RandomFloat (0.85f, 1.2f);

        MakeWave (vec3(x, y, z), l);
    }

    // Make the waves move:
    UpdateWater (dt < 0.02f ? dt : 0.02f);

    const Uint8 *state = SDL_GetKeyboardState (NULL);
    if (state [SDL_SCANCODE_UP] || state [SDL_SCANCODE_DOWN]
        || state [SDL_SCANCODE_LEFT] || state [SDL_SCANCODE_RIGHT]
        || state [SDL_SCANCODE_PAGEDOWN] || state [SDL_SCANCODE_PAGEUP])
    {
        // Get camera position for chosen angles and distance:
        vec3 posCamera;
        getCameraPosition (angleX, angleY, distCamera, posCamera);

        // Move cube, according to camera view and buttons pressed
        GLfloat mx, mz, my;

        if (state[SDL_SCANCODE_UP])
            mz = -CUBE_SPEED;
        else if (state[SDL_SCANCODE_DOWN])
            mz = CUBE_SPEED;
        else
            mz = 0.0f;

        if (state[SDL_SCANCODE_LEFT])
            mx = -CUBE_SPEED;
        else if (state[SDL_SCANCODE_RIGHT])
            mx = CUBE_SPEED;
        else
            mx = 0.0f;

        if (state[SDL_SCANCODE_PAGEUP])
            my = CUBE_SPEED;
        else if (state[SDL_SCANCODE_PAGEDOWN])
            my = -CUBE_SPEED;
        else
            my = 0.0f;

        vec3 camZ = (-posCamera); camZ.y = 0; camZ = camZ.Unit();
        vec3 camX = Cross (VEC_UP, camZ);
        posCube += mx * camX - mz * camZ;
        posCube.y += my;
    }

    // CubeBlockWaves ();

    // Adjust normals to new grid points:
    UpdateWaterNormals();
}
void WaterScene::RenderWater()
{
    // Draw quads between the grid points

    glBegin(GL_QUADS);

    vec3 p1,p2,p3,p4,n1,n2,n3,n4;
    for(int x=0; x<GRIDSIZE-1; x++)
    {
        for(int z=0; z<GRIDSIZE-1; z++)
        {
            p1=gridpoints[x][z];
            p2=gridpoints[x+1][z];
            p3=gridpoints[x+1][z+1];
            p4=gridpoints[x][z+1];
            n1=gridnormals[x][z];
            n2=gridnormals[x+1][z];
            n3=gridnormals[x+1][z+1];
            n4=gridnormals[x][z+1];

            glNormal3f(n4.x, n4.y, n4.z);
            glVertex3f(p4.x, p4.y, p4.z);

            glNormal3f(n3.x, n3.y, n3.z);
            glVertex3f(p3.x, p3.y, p3.z);

            glNormal3f(n2.x, n2.y, n2.z);
            glVertex3f(p2.x, p2.y, p2.z);

            glNormal3f(n1.x, n1.y, n1.z);
            glVertex3f(p1.x, p1.y, p1.z);
        }
    }

    glEnd();
}

/**
 * Renders a square 40x40 plane at the origin, with normal pointing up.
 */
void RenderPlane()
{
    GLfloat size = 20.0f;

    glBegin(GL_QUADS);

    glNormal3f( 0, 1, 0);
    glVertex3f(-1*size, 0, 1*size);
    glVertex3f( 1*size, 0, 1*size);
    glVertex3f( 1*size, 0,-1*size);
    glVertex3f(-1*size, 0,-1*size);

    glEnd();
}

const GLfloat colorCube [] = {0.0f, 1.0f, 0.0f, 1.0f},
              colorCubeReflect [] = {0.0f, 0.8f, 0.1f, 1.0f},
              bgcolorReflect [] ={0.1f, 0.2f, 1.0f, 1.0f},
              bgcolorRefract [] = {0.1f, 0.1f, 0.2f, 1.0f},
              bgcolorRed [] = {1.0f, 0.0f, 0.0f, 1.0f},

              posLight [] = {
                            0.0f,
                            5.0f,
                            0.0f,
                            1.0f}, // 0 is directional

                ambient [] = {0.5f, 0.5f, 0.5f, 1.0f},
                diffuse [] = {0.5f, 0.5f, 0.5f, 1.0f};

// const GLdouble planeWater [] = {0, -1.0, 0, 0}; // at origin, pointing down

#define WATER_STENCIL_MASK 0xFFFFFFFFL
#define WATER_LINE_MARGE 0.1f // moves water line slightly up to prevent artifacts

void WaterScene::Render()
{
    GLint texLoc, colLoc;
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    // Init view and perspective
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(matPerspec(VIEW_ANGLE,(GLfloat)w/(GLfloat)h,NEAR_VIEW,FAR_VIEW).m);

    // Settings that stay during the entire rendering process
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_COLOR_MATERIAL);
    glDisable (GL_TEXTURE_2D);
    glDisable (GL_BLEND);
    glDisable (GL_STENCIL_TEST);
    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glFrontFace (GL_CCW);

    // set to camera view
    vec3 posCamera;
    getCameraPosition(angleX, angleY, distCamera, posCamera);
    glMatrixMode(GL_MODELVIEW);
    matrix4 lm=matLookAt(posCamera, vec3(0,0,0), vec3(0,1,0));
    glLoadMatrixf(lm.m);

    // Light settings, position will be set later
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);


    /*
        Refraction: make sure anything above the water isn't drawn.
        Set background depth to 1.0 for the pixel shader.
     */

    // Switch to refraction frame buffer
    glBindFramebuffer (GL_FRAMEBUFFER, fbRefraction);
    glViewport (0, 0, w, h);

    glEnable (GL_CULL_FACE);
    glEnable (GL_STENCIL_TEST);
    glEnable (GL_DEPTH_TEST);

    // Position the light in normal space
    glLightfv (GL_LIGHT0, GL_POSITION, posLight);

    // Clear the refraction buffer with water color, maximum depth and stencil 1
    glClearDepth (1.0f);
    glClearStencil (1);
    glClearColor (bgcolorRefract[0], bgcolorRefract[1], bgcolorRefract[2], bgcolorRefract[3]);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Render cube refraction, set stencil to 0 there
    glStencilFunc (GL_ALWAYS, 0, WATER_STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);
    glCullFace (GL_FRONT);
    glDepthFunc (GL_LEQUAL);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCubeReflect);
    RenderCube (posCube, CUBESIZE);

    // Set stencil to 1 for everything above the water level.
    glStencilFunc (GL_ALWAYS, 1, WATER_STENCIL_MASK);
    glCullFace (GL_FRONT);
    glDepthFunc (GL_GEQUAL);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);
    glDepthMask (GL_FALSE);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glTranslatef (0, WATER_LINE_MARGE, 0);
    RenderWater ();
    glTranslatef (0, -WATER_LINE_MARGE, 0);

    // Set the depth value 1.0 and water color for all pixels with stencil 1.
    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc (GL_ALWAYS);
    glStencilFunc (GL_EQUAL, 1, WATER_STENCIL_MASK);
    glUseProgram (shaderProgramDepth);
    colLoc = glGetUniformLocation (shaderProgramDepth, "color");
    glUniform4fv (colLoc, 1, bgcolorRefract);
    glBegin (GL_QUADS);
        glVertex4f (-1.0f, 1.0f, 0.5f, 1.0f);
        glVertex4f ( 1.0f, 1.0f, 0.5f, 1.0f);
        glVertex4f ( 1.0f,-1.0f, 0.5f, 1.0f);
        glVertex4f (-1.0f,-1.0f, 0.5f, 1.0f);
    glEnd ();
    glUseProgram (0);


    /*
        Reflection: make sure anything below the water isn't drawn and
        everything above the water is drawn in mirror image.
        Set background depth to 1.0 for the pixel shader.
     */

    // Switch to reflection frame buffer
    glBindFramebuffer (GL_FRAMEBUFFER, fbReflection);
    glViewport (0, 0, w, h);

    glEnable (GL_CULL_FACE);
    glEnable (GL_STENCIL_TEST);
    glEnable (GL_DEPTH_TEST);

    // Clear the reflection buffer with water color, maximum depth and stencil 1
    glClearDepth (1.0f);
    glClearStencil (1);
    glClearColor (bgcolorReflect[0], bgcolorReflect[1], bgcolorReflect[2], bgcolorReflect[3]);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Render cube reflection, set stencil to 0 there
    glStencilFunc (GL_ALWAYS, 0, WATER_STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);
    glCullFace (GL_BACK);
    glDepthFunc (GL_LEQUAL);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPushMatrix ();
        // move light and cube to their mirror position
        glScalef (1, -1, 1);
        glLightfv (GL_LIGHT0, GL_POSITION, posLight);
        glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCubeReflect);
        RenderCube (posCube, CUBESIZE);
    glPopMatrix ();

    // Set stencil to 1 for everything above the water level.
    glStencilFunc (GL_ALWAYS, 1, WATER_STENCIL_MASK);
    glCullFace (GL_FRONT);
    glDepthFunc (GL_GEQUAL);
    glStencilOp (GL_KEEP, GL_KEEP, GL_REPLACE);
    glDepthMask (GL_FALSE);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glTranslatef (0, WATER_LINE_MARGE, 0);
    RenderWater ();
    glTranslatef (0, -WATER_LINE_MARGE, 0);

    // Set the depth value 1.0 and water color for all pixels with stencil 1.
    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthFunc (GL_ALWAYS);
    glStencilFunc (GL_EQUAL, 1, WATER_STENCIL_MASK);
    glUseProgram (shaderProgramDepth);
    colLoc = glGetUniformLocation (shaderProgramDepth, "color");
    glUniform4fv (colLoc, 1, bgcolorReflect);
    glBegin (GL_QUADS);
        glVertex4f (-1.0f, 1.0f, 0.5f, 1.0f);
        glVertex4f ( 1.0f, 1.0f, 0.5f, 1.0f);
        glVertex4f ( 1.0f,-1.0f, 0.5f, 1.0f);
        glVertex4f (-1.0f,-1.0f, 0.5f, 1.0f);
    glEnd ();
    glUseProgram (0);


    // Direct rendering to the screen
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glViewport (0, 0, w, h);

    glDepthFunc (GL_LEQUAL);

    glEnable (GL_TEXTURE_2D);

    // Bind reflection to texture 0 and refraction to texture 1
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texReflection);

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, texRefraction);

    glActiveTexture (GL_TEXTURE2);
    glBindTexture (GL_TEXTURE_2D, texReflecDepth);

    glActiveTexture (GL_TEXTURE3);
    glBindTexture (GL_TEXTURE_2D, texRefracDepth);

    glEnable (GL_CULL_FACE);
    glCullFace (GL_FRONT);

    // clear the screen with black color and max depth
    glClearDepth (1.0f);
    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render reflection and refraction to the water
    glUseProgram(shaderProgramWater);
    glDisable (GL_STENCIL_TEST);

    // Tell the shader to use textures GL_TEXTURE0 and GL_TEXTURE1
    texLoc = glGetUniformLocation (shaderProgramWater, "tex_reflect");
    glUniform1i (texLoc, 0);

    texLoc = glGetUniformLocation (shaderProgramWater, "tex_refract");
    glUniform1i (texLoc, 1);

    texLoc = glGetUniformLocation (shaderProgramWater, "tex_reflect_depth");
    glUniform1i (texLoc, 2);

    texLoc = glGetUniformLocation (shaderProgramWater, "tex_refract_depth");
    glUniform1i (texLoc, 3);

    RenderWater();
    glUseProgram(0);

    // Set active texture back to 0 and disable
    glBindTexture (GL_TEXTURE_2D,0);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_2D);

    // Position the light in normal space
    glLightfv (GL_LIGHT0, GL_POSITION, posLight);

    // Draw The cube at the set position
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCube);
    RenderCube (posCube, CUBESIZE);
}
