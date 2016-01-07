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


#ifndef SCENE_H
#define SCENE_H

#include "app.h"
#include "../vec.h"
#include "buffer.h"

#define GRIDSIZE 100

/*
 * Demonstrates:
 * - Pixel shaders
 * - Render to texture
 */
class WaterScene : public App::Scene
{
private:
    float timeDrop;

    // camera angles
    GLfloat angleY, angleX, distCamera;

    // position of the cube
    vec3 posCube;

    // vertex and index buffer to hold water grid data:
    VertexBuffer vbo_water;
    IndexBuffer ibo_water;

    // time-dependent variables for vertical movement of the water grid:
    float gridforces [GRIDSIZE][GRIDSIZE],
          gridspeeds [GRIDSIZE][GRIDSIZE];

    // positions and normals of the water grid vertices:
    vec3 gridpoints [GRIDSIZE][GRIDSIZE],
         gridnormals [GRIDSIZE][GRIDSIZE];

    // Frame buffers to render to and textures to store pixel data:
    GLuint
        fbReflection, texReflection, texReflecDepth,
        fbRefraction, texRefraction, texRefracDepth,

        // handle to shader programs
        shaderProgramWater,
        shaderProgramDepth;

    // Moves water grid points in time:
    void UpdateWater (const float dt);

    // Re-calculates normals for water grid points:
    void UpdateWaterNormals (void);

    void CubeBlockWaves (void);

    // Create a wave at point p with length l
    void MakeWave (const vec3 p, const float l);

public:
    WaterScene (App*);
    ~WaterScene ();

    void AddAll (Loader *);
    void Update (const float dt);
    void Render (void);
    void OnMouseMove (const SDL_MouseMotionEvent *event);
    void OnMouseWheel (const SDL_MouseWheelEvent *event);
};

#endif // SCENE_H
