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
#include "buffer.h"
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
/* Triangle format specially designed for calculating shadows */
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

    vec3 posPlayer;
    Quaternion rotationPlayer;

    GLuint shader_normal;

    VertexBuffer vbo_box,
                 vbo_sky,
                 vbo_dummy;

    // indices for tangent and bitangent vertex attributes:
    GLint index_tangent,
          index_bitangent;

    Texture texDummy, texDummyNormal, // textures of the character mesh
            texBox, // texture of the walls and floor
            texSky, // texture for the sky
            texPar; // semi-transparent particle texture

    float frame; // current animation frame

    // flags that represent the current settings for the scene:
    bool show_bones,
         show_normals,
         show_triangles;

    STriangle *shadowTriangles; // number of triangles depends on mesh used

    // Objects to keep track of the state of each particle in the scene
    Particle particles [N_PARTICLES];

    // Objects to represent the collision boundaries around the player
    ColliderP colliders [N_PLAYER_COLLIDERS];

    /*
        These triangles represent the environment.
        They're used for collision detection.
     */
    size_t n_collision_triangles;
    Triangle *collision_triangles;

    MeshData meshDataDummy, // the movable character, with bones and animation data
             meshDataBox, // environment: ground and walls
             meshDataSky; // skybox, half a sphere

    MeshState *pMeshDummy; // animatable object, representing the player

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
