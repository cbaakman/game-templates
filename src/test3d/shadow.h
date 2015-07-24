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


#ifndef SHADOW_H
#define SHADOW_H

#include "app.h"
#include "mesh.h"
#include "collision.h"
#include "../texture.h"

#define N_PLAYER_COLLIDERS 2
#define N_PARTICLES 10

struct Particle
{
    vec3 pos, vel;
    float size;
};

struct STriangle
{
    const vec3 *p[3];
    bool visible;
};

/*
 * Demonstrates:
 * - Collision detection
 * - Skeletal animation
 * - Stencil shadowing
 * - Particles
 */
class ShadowScene : public App::Scene
{
private:

    // camera angles
    GLfloat angleY, angleX, distCamera;

    // fall speed
    float vy;
    bool onGround, touchDown;

    vec3 posPlayer, directionPlayer;

    Texture texDummy,
            texBox,
            texSky,
            texPar;

    float frame;
    bool show_bones, show_normals, show_triangles;

    STriangle *shadowTriangles;

    Particle particles [N_PARTICLES];

    ColliderP colliders [N_PLAYER_COLLIDERS];

    size_t n_collision_triangles;
    Triangle *collision_triangles;

    MeshData meshDataDummy, meshDataBox, meshDataSky;
    MeshObject *pMeshDummy;

public:
    ShadowScene (App*);
    ~ShadowScene ();

    bool Init (void);
    void Update (const float dt);
    void Render (void);
    void OnKeyPress (const SDL_KeyboardEvent *event);
    void OnMouseMove (const SDL_MouseMotionEvent *event);
    void OnMouseWheel (const SDL_MouseWheelEvent *event);
};

#endif // SHADOW_H
