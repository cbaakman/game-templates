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

#ifndef GRASS_H
#define GRASS_H

#include "app.h"
#include "buffer.h"
#include "mesh.h"
#include "../texture.h"

/**
 * Demonstrates different techniques for rendering grass.
 *
 * Currently implemented methods:
 * - layer by layer (looks better from the top)
 * - one polygon per blade (more expensive)
 */
class GrassScene : public App::Scene
{
private:

    float angleX, angleY, distCamera;

    VertexBuffer vbo_hill,
                 vbo_grass;
    MeshData meshDataHill;

    vec3 *grass_neutral_positions;

    Texture texDots,
            texGrass;

    GLuint layerShader,
           polyShader;

    // wind parameters, makes the grass move
    vec3 wind;
    float t;

    /*
        GrassMode enum type cannot be incremented.
        So treat it as an int.
     */
    int mode;
    enum GrassMode
    {
        GRASSMODE_LAYER,
        GRASSMODE_POLYGON,

        N_GRASSMODES
    };
public:

    GrassScene (App *);
    ~GrassScene ();

    void Update (const float dt);
    void Render (void);

    void AddAll (Loader *);

    void OnMouseWheel (const SDL_MouseWheelEvent *);
    void OnMouseMove (const SDL_MouseMotionEvent *);
    void OnKeyPress (const SDL_KeyboardEvent *);
};

#endif // GRASS_H
