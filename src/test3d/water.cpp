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

WaterScene::WaterScene(App *pApp) : Scene(pApp),

    fbReflection(0), cbReflection(0), dbReflection(0), texReflection(0),
    fbRefraction(0), cbRefraction(0), dbRefraction(0), texRefraction(0),
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
            gridnormals[x][z] = vec3(0.0f, 1.0f, 0.0f);
            gridpoints[x][z] = vec3(x * spacing + offset, 0, z * spacing + offset);
            gridspeeds[x][z] = 0.0f;
            gridforces[x][z] = 0.0f;
        }
    }
}
bool WaterScene::Init()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    GLenum status;

    std::string resourceZip = std::string(SDL_GetBasePath()) + "test3d.zip",
                shaderDir = "shaders/",
                pathVSH = shaderDir + "water.vsh",
                pathFSH = shaderDir + "water.fsh",
                sourceV = "", sourceF = "";

    glGenFramebuffers(1, &fbReflection);
    glGenFramebuffers(1, &fbRefraction);

    glGenRenderbuffers(1, &cbReflection);
    glGenRenderbuffers(1, &cbRefraction);
    glGenRenderbuffers(1, &dbReflection);
    glGenRenderbuffers(1, &dbRefraction);

    glGenTextures(1, &texReflection);
    glGenTextures(1, &texRefraction);

    glBindFramebuffer(GL_FRAMEBUFFER, fbReflection);

    glBindRenderbuffer(GL_RENDERBUFFER, cbReflection);

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, w, h);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cbReflection);

    glBindRenderbuffer(GL_RENDERBUFFER, dbReflection);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, dbReflection);

    glBindTexture(GL_TEXTURE_2D, texReflection);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texReflection, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        SetError ("error, reflection framebuffer is not complete");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbRefraction);

    glBindRenderbuffer(GL_RENDERBUFFER, cbRefraction);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cbRefraction);

    glBindRenderbuffer(GL_RENDERBUFFER, dbRefraction);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, dbRefraction);

    glBindTexture(GL_TEXTURE_2D, texRefraction);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texRefraction, 0);

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        SetError ("error, refraction framebuffer is not complete");
        return false;
    }

    SDL_RWops *f;
    bool success;

    f = SDL_RWFromZipArchive (resourceZip.c_str(), pathVSH.c_str());
    if (!f)
        return false; // file or archive missing

    success = ReadAll (f, sourceV);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing %s: %s", pathVSH.c_str(), GetError ());
        return false;
    }

    f = SDL_RWFromZipArchive (resourceZip.c_str(), pathFSH.c_str());
    if (!f)
        return false;
    success = ReadAll (f, sourceF);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing %s: %s", pathFSH.c_str(), GetError ());
        return false;
    }

    shaderProgramWater = createShaderProgram (sourceV, sourceF);
    if (!shaderProgramWater)
    {
        SetError ("error creating shader program from %s and %s: %s", pathVSH.c_str(), pathFSH.c_str(), GetError ());
        return false;
    }

    return true;
}
WaterScene::~WaterScene()
{
    glDeleteProgram(shaderProgramWater);

    glDeleteTextures(1, &texReflection);
    glDeleteTextures(1, &texRefraction);

    glDeleteFramebuffers(1, &fbReflection);
    glDeleteFramebuffers(1, &fbRefraction);

    glDeleteRenderbuffers(1, &cbReflection);
    glDeleteRenderbuffers(1, &cbRefraction);
    glDeleteRenderbuffers(1, &dbReflection);
    glDeleteRenderbuffers(1, &dbRefraction);
}
void WaterScene::UpdateWaterNormals()
{
    // Recalculate normals, depending on the edges around the vertices.

    vec3 p0,p1,p2;
    for(int x=0; x<GRIDSIZE-1; x++)
    {
        for(int z=0; z<GRIDSIZE-1; z++)
        {
            p0=gridpoints[x][z]; p1=gridpoints[x+1][z]; p2=gridpoints[x][z+1];
            gridnormals[x][z]=Cross(p2-p0,p1-p0).Unit();
        }
    }
    for(int x=0; x<GRIDSIZE-1; x++)
    {
        p0=gridpoints[x][GRIDSIZE-1]; p1=gridpoints[x-1][GRIDSIZE-1]; p2=gridpoints[x][GRIDSIZE-2];
        gridnormals[x][GRIDSIZE-1]=Cross(p0-p2,p0-p1).Unit();
    }
    for(int z=0; z<GRIDSIZE-1; z++)
    {
        p0=gridpoints[GRIDSIZE-1][z]; p1=gridpoints[GRIDSIZE-2][z]; p2=gridpoints[GRIDSIZE-1][z-1];
        gridnormals[GRIDSIZE-1][z]=Cross(p0-p2,p0-p1).Unit();
    }

    p0=gridpoints[GRIDSIZE-1][GRIDSIZE-1];
    p1=gridpoints[GRIDSIZE-2][GRIDSIZE-1];
    p2=gridpoints[GRIDSIZE-1][GRIDSIZE-2];

    gridnormals[GRIDSIZE-1][GRIDSIZE-1]=Cross(p2-p0,p1-p0).Unit();
}
void WaterScene::MakeWave(const vec3 p, const float l)
{
    // Move the grid points so that it looks like a wave at point p with length l

    float b = 2 * PI / l;

    float dist;
    for(int z=0; z<GRIDSIZE; z++)
    {
        for(int x=0; x<GRIDSIZE; x++)
        {
            dist = sqrt (sqr (gridpoints[x][z].x - p.x) + sqr (gridpoints [x][z].z - p.z));
            gridpoints[x][z].y += p.y *cos (b * dist) * exp (-sqr (dist));
        }
    }
}
#define SQRT2DIV2 0.707106781186547524400844362104849f
void WaterScene::UpdateWater(const float dt)
{
    // Move the grid points to update the waves

    float d;

    for(int x=0; x<GRIDSIZE; x++)
    {
        for(int z=0; z<GRIDSIZE; z++)
        {
            gridforces[x][z] = 0.0f;
        }
    }

    for(int x=1; x<(GRIDSIZE-1); x++)
    {
        for(int z=1; z<(GRIDSIZE-1); z++)
        {
            d=gridpoints[x][z].y -  gridpoints[x-1][z].y;
              gridforces[x][z]-=d;    gridforces[x-1][z]+=d;

            d=gridpoints[x][z].y -  gridpoints[x+1][z].y;
              gridforces[x][z]-=d;    gridforces[x+1][z]+=d;

            d=gridpoints[x][z].y -  gridpoints[x][z-1].y;
              gridforces[x][z]-=d;    gridforces[x][z-1]+=d;

            d=gridpoints[x][z].y -  gridpoints[x][z+1].y;
              gridforces[x][z]-=d;    gridforces[x][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x-1][z+1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x-1][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x+1][z+1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x+1][z+1]+=d;

            d=gridpoints[x][z].y - gridpoints[x-1][z-1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x-1][z-1]+=d;

            d=gridpoints[x][z].y - gridpoints[x+1][z-1].y; d*=SQRT2DIV2;
               gridforces[x][z]-=d;    gridforces[x+1][z-1]+=d;
        }
    }

    float ft = 30.0f * dt;
    if (ft > 1.0f)
        ft = 1.0f;

    // friction / viscosity
    float u = (float)exp(250.0f * dt * log(0.99f));

    for(int x=0; x<GRIDSIZE; x++)
    {
        for(int z=0; z<GRIDSIZE; z++)
        {
            gridspeeds[x][z] += 0.2f * gridforces[x][z] * ft;
            gridpoints[x][z].y += gridspeeds[x][z] * ft;
            gridpoints[x][z].y *= u;
        }
    }

    UpdateWaterNormals();
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
    // Make random waves over time

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
    UpdateWater (dt < 0.02f ? dt : 0.02f);


    const Uint8 *state = SDL_GetKeyboardState (NULL);
    if (state [SDL_SCANCODE_UP] || state [SDL_SCANCODE_DOWN]
        || state [SDL_SCANCODE_LEFT] || state [SDL_SCANCODE_RIGHT]
        || state [SDL_SCANCODE_PAGEDOWN] || state [SDL_SCANCODE_PAGEUP])
    {
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
        vec3 camX = Cross(VEC_UP, camZ);
        posCube += mx * camX - mz * camZ;
        posCube.y += my;
    }
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
void RenderPlane()
{
    GLfloat size=20.0f;

    glBegin(GL_QUADS);

    glNormal3f( 0, 1, 0);
    glVertex3f(-1*size, 0, 1*size);
    glVertex3f( 1*size, 0, 1*size);
    glVertex3f( 1*size, 0,-1*size);
    glVertex3f(-1*size, 0,-1*size);

    glEnd();
}
void RenderCube()
{
    GLfloat size=1.0f;

    glBegin(GL_QUADS);

    glNormal3f( 0, 0, 1);
    glVertex3f(-1*size, 1*size,-1*size);
    glVertex3f( 1*size, 1*size,-1*size);
    glVertex3f( 1*size,-1*size,-1*size);
    glVertex3f(-1*size,-1*size,-1*size);

    glNormal3f( 0, 0,-1);
    glVertex3f( 1*size, 1*size, 1*size);
    glVertex3f(-1*size, 1*size, 1*size);
    glVertex3f(-1*size,-1*size, 1*size);
    glVertex3f( 1*size,-1*size, 1*size);

    glNormal3f( 1, 0, 0);
    glVertex3f( 1*size, 1*size,-1*size);
    glVertex3f( 1*size, 1*size, 1*size);
    glVertex3f( 1*size,-1*size, 1*size);
    glVertex3f( 1*size,-1*size,-1*size);

    glNormal3f(-1, 0, 0);
    glVertex3f(-1*size, 1*size, 1*size);
    glVertex3f(-1*size, 1*size,-1*size);
    glVertex3f(-1*size,-1*size,-1*size);
    glVertex3f(-1*size,-1*size, 1*size);

    glNormal3f( 0, 1, 0);
    glVertex3f(-1*size, 1*size, 1*size);
    glVertex3f( 1*size, 1*size, 1*size);
    glVertex3f( 1*size, 1*size,-1*size);
    glVertex3f(-1*size, 1*size,-1*size);

    glNormal3f( 0,-1, 0);
    glVertex3f(-1*size,-1*size,-1*size);
    glVertex3f( 1*size,-1*size,-1*size);
    glVertex3f( 1*size,-1*size, 1*size);
    glVertex3f(-1*size,-1*size, 1*size);

    glEnd();
}

const GLfloat colorCube[] = {0.0f, 1.0f, 0.0f, 1.0f},
              colorCubeReflect[] = {0.0f, 0.8f, 0.1f, 1.0f},
              bgcolorReflect[] ={0.1f, 0.2f, 1.0f, 1.0f},
              bgcolorRefract[] = {0.1f, 0.1f, 0.2f, 1.0f},

              posLight[] = {
                            0.0f,
                            5.0f,
                            0.0f,
                            1.0f}, // 0 is directional

                ambient[] = {0.5f, 0.5f, 0.5f, 1.0f},
                diffuse[] = {0.5f, 0.5f, 0.5f, 1.0f};

void WaterScene::Render()
{
    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    // Init view and perspective
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(matPerspec(VIEW_ANGLE,(GLfloat)w/(GLfloat)h,NEAR_VIEW,FAR_VIEW).m);

    // Settings that stay during the entire rendering process
    glEnable (GL_DEPTH_TEST);
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


    // Render the refraction in its buffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbRefraction);
    glViewport(0, 0, w, h);

    // Clear the buffer with water color and minimum depth
    glClearDepth(0.0f);
    glClearColor(bgcolorRefract[0], bgcolorRefract[1], bgcolorRefract[2], bgcolorRefract[3]);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_CULL_FACE);

    // Position the light in normal space
    glLightfv(GL_LIGHT0, GL_POSITION, posLight);

    // Make sure anything above the water isn't drawn
    glDepthFunc(GL_GREATER);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    RenderPlane();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_COLOR_MATERIAL);

    glTranslatef(posCube.x, posCube.y, posCube.z);
    glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCube);
    RenderCube();
    glTranslatef(-posCube.x, -posCube.y, -posCube.z);




    // Render the reflection in its buffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbReflection);
    glViewport(0, 0, w, h);

    // Clear the buffer with water color and minimum depth
    glClearDepth(0.0f);
    glClearColor(bgcolorReflect[0], bgcolorReflect[1], bgcolorReflect[2], bgcolorReflect[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // Make sure anything below the water isn't drawn
    glDepthFunc(GL_GEQUAL);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    RenderPlane();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glCullFace(GL_BACK);
    glPushMatrix();
        // move light and cube to their mirror position
        glScalef(1, -1, 1);
        glLightfv(GL_LIGHT0, GL_POSITION, posLight);
        glTranslatef(posCube.x, posCube.y, posCube.z);
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCubeReflect);
        RenderCube();
    glPopMatrix();


    // Direct rendering to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);

    glDepthFunc(GL_LEQUAL);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texReflection);
    glEnable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texRefraction);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    // clear the screen with black color and max depth
    glClearDepth (1.0f);
    glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render reflection and refraction to the water
    glUseProgram(shaderProgramWater);
    RenderWater();
    glUseProgram(0);

    // This must be done to get the coloring right, dunno why
    glBindTexture (GL_TEXTURE_2D,0);
    glActiveTexture (GL_TEXTURE0);
    glDisable (GL_TEXTURE_2D);

    // Position the light in normal space
    glLightfv (GL_LIGHT0, GL_POSITION, posLight);

    // Draw The cube at the set position
    glMaterialfv (GL_FRONT, GL_AMBIENT_AND_DIFFUSE, colorCube);
    glPushMatrix ();
        glTranslatef (posCube.x, posCube.y, posCube.z);
        RenderCube ();
    glPopMatrix ();
}
