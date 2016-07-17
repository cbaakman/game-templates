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

#include "grass.h"
#include "../GLutil.h"
#include "../err.h"
#include <functional>

#include "../shader.h"

#include <cstdlib>

#include <GL/gl.h>

const GLfloat GRASS_TOP_COLOR [3] = {0.0f, 0.8f, 0.0f},
              GRASS_BOTTOM_COLOR [3] = {0.0f, 0.5f, 0.0f};

const char

    /*
        For some reason, 150 shaders don't work with client states,
        so we must use vertex attributes to refer to position, normal and texture coords.
     */

    layer_vsh [] = R"shader(
        #version 150

        uniform mat4 projMatrix,
                     modelViewMatrix;

        in vec3 vertex,
                normal;
        in vec2 texCoord;

        out VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexOut;

        void main ()
        {
            gl_Position = projMatrix * modelViewMatrix * vec4 (vertex, 1.0);

            VertexOut.eyeVertex = (modelViewMatrix * vec4 (vertex, 1.0)).xyz;

            mat4 normalMatrix = transpose (inverse (modelViewMatrix));
            VertexOut.eyeNormal = (normalMatrix * vec4 (normalize (normal), 0.0)).xyz;

            VertexOut.texCoord = texCoord;
        }
    )shader",

    /*
        This geometry shader is responsible for generating the layers.
        It will be called once per layer.
     */
    layer_gsh [] = R"shader(
        #version 150

        uniform int layer, // current layer
                    n_layers;
        uniform float extend_max;
        uniform vec3 wind;

        uniform mat4 modelViewMatrix;

        layout (triangles) in;
        layout (triangle_strip, max_vertices=3) out;

        uniform mat4 projMatrix;

        // [3] for triangles:
        in VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexIn [];

        out VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
            float extension;
        } VertexOut;

        void main ()
        {
            int i;
            float step = extend_max / n_layers;
            for (i = 0; i < gl_in.length(); i++)
            {
                VertexOut.extension = float (layer) / n_layers;

                vec3 gravity = (modelViewMatrix * vec4 (0.0, -0.2, 0.0, 0.0)).xyz,

                     newVertex = VertexIn [i].eyeVertex + (VertexIn [i].eyeNormal) * layer * step
                                                        + wind * VertexOut.extension
                                                        + gravity * VertexOut.extension * VertexOut.extension;

                gl_Position = projMatrix * vec4 (newVertex, 1.0);

                VertexOut.eyeVertex = newVertex;
                VertexOut.eyeNormal = VertexIn [i].eyeNormal;
                VertexOut.texCoord = VertexIn [i].texCoord;

                EmitVertex ();
            }
        }

    )shader",

    layer_fsh [] = R"shader(
        #version 150

        uniform sampler2D tex_dots;
        uniform int layer;

        uniform vec3 top_color,
                     bottom_color;

        uniform mat4 modelViewMatrix;

        uniform vec3 eyeLightDir;

        in VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
            float extension;
        } VertexIn;

        void main ()
        {
            // On the texture, black is opaque, white is transparent.
            vec4 dotSample = texture2D (tex_dots, VertexIn.texCoord.st);
            float alpha = 1.0 - dotSample.r - VertexIn.extension * VertexIn.extension;

            // The bottom layer is always opaque.
            if (layer <= 0)
                alpha = 1.0;

            /*
                This alpha threshold makes the grass look more solid.
                Remove it to get softer, moss-like grass.
             */
            if (alpha > 0.25)
                alpha = 1.0;
            else
                alpha = 0.0;

            vec3 L = normalize (-eyeLightDir),
                 N = normalize (VertexIn.eyeNormal);

            float lum = clamp (dot (N, L), 0.0, 1.0);

            vec3 color = (1.0 - VertexIn.extension) * bottom_color + VertexIn.extension * top_color;

            gl_FragColor = vec4 (color * lum * lum, alpha);
        }

    )shader",

    poly_ground_vsh [] = R"shader(
        #version 150

        uniform mat4 projMatrix,
                     modelViewMatrix;

        in vec3 vertex,
                normal;
        in vec2 texCoord;

        out VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexOut;

        void main()
        {
            VertexOut.eyeVertex = (modelViewMatrix * vec4 (vertex, 1.0)).xyz;

            mat4 normalMatrix = transpose (inverse (modelViewMatrix));
            VertexOut.eyeNormal = normalize (normalMatrix * vec4 (normal, 0)).xyz;

            gl_Position = projMatrix * modelViewMatrix * vec4 (vertex, 1.0);

            VertexOut.texCoord = texCoord;
        }

    )shader",

    poly_fsh [] = R"shader(
        #version 150

        uniform sampler2D tex_grass;

        uniform vec3 top_color,
                     bottom_color;

        in VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexIn;

        uniform vec3 eyeLightDir;

        void main ()
        {
            vec3 L = normalize (-eyeLightDir),
                 N = normalize (VertexIn.eyeNormal);

            // Rendering two faces of the triangle, so take abs value of dot product:
            float lum = clamp (dot (N, L), 0.0, 1.0);

            // On the texture, black is opaque, white is transparent.
            vec4 sample = texture2D (tex_grass, VertexIn.texCoord.st);

            float a = VertexIn.texCoord.t * VertexIn.texCoord.t;
            vec3 color = (1.0 - a) * bottom_color + a * top_color;

            gl_FragColor = vec4 (color * lum * lum, 1.0 - sample.r);
        }

    )shader",

    ground_fsh [] = R"shader(
        #version 150

        uniform vec3 bottom_color;

        in VertexData {
            vec3 eyeVertex,
                 eyeNormal;
            vec2 texCoord;
        } VertexIn;

        uniform vec3 eyeLightDir;

        void main ()
        {
            vec3 L = normalize (-eyeLightDir),
                 N = normalize (VertexIn.eyeNormal);

            float lum = clamp (dot (N, L), 0.0, 1.0);

            gl_FragColor = vec4 (bottom_color * lum * lum, 1.0);
        }

    )shader";

#define PLAYER_EYE_Y 0.7f
#define PLAYER_FEET_Y -1.0f

#define CHUNK_LOAD_DIST 1

GrassScene::GrassScene (App *pApp) : Scene (pApp),
    playerPos(0.0f,1.7f,0.0f), playerVelocity(0.0f,0.0f,0.0f),
    playerLookAngleX(0.0f), playerLookAngleY(0.0f),
    playerOnGround(false),
    layerShader(0), polyShader(0), groundShader(0),
    wind(0.0f, 0.0f, 0.0f), t(0.0f),
    mode(GRASSMODE_POLYGON)
{
    texDots.tex = texGrass.tex = 0;

    pTerraGen = new TerraGen <TerrainType> ("5", 5.0f);

    playerColliders.push_back (new SphereCollider (vec3 (0, 0.5f, 0), 0.5f));
    playerColliders.push_back (new SphereCollider (
                                            vec3 (0, PLAYER_FEET_Y + 0.1f, 0),
                                            0.1f));
}
GrassScene::~GrassScene ()
{
    for (auto pair : chunks)
        delete pair.second;

    delete pTerraGen;
    for (ColliderP pCollider : playerColliders)
        delete pCollider;

    glDeleteProgram (layerShader);
    glDeleteProgram (polyShader);
    glDeleteTextures (1, &texDots.tex);
    glDeleteTextures (1, &texGrass.tex);
}
void UpdateChunks (std::map <std::tuple <ChunkNumber,ChunkNumber>, GrassChunk *> &chunks,
                   const TerraGen <TerrainType> *pTerraGen, vec3 aroundPos)
{
    ChunkNumber cX, cZ, xi, zi;

    std::tie (cX, cZ) = ChunkNumbersFor (aroundPos.x, aroundPos.z);
    std::list <std::tuple <ChunkNumber, ChunkNumber>> chunks_remove;
    for (auto pair : chunks)
    {
        std::tie (xi, zi) = pair.first;
        if (abs (xi - cX) > CHUNK_LOAD_DIST || abs (zi - cZ) > CHUNK_LOAD_DIST)
            chunks_remove.push_back (pair.first);
    }
    for (auto key : chunks_remove)
    {
        delete chunks [key];
        chunks.erase (key);
    }
    for (xi = cX - CHUNK_LOAD_DIST; xi <= cX + CHUNK_LOAD_DIST; xi++)
    {
        for (zi = cZ - CHUNK_LOAD_DIST; zi <= cZ + CHUNK_LOAD_DIST; zi++)
        {
            auto id = std::make_tuple (xi, zi);
            if (chunks.find (id) == chunks.end ())
            {
                chunks [id] = new GrassChunk (xi, zi);
                if (!chunks [id]->Generate (pTerraGen))
                    fprintf (stderr, "error generating chunk: %s", GetError());
            }
        }
    }
}
void GrassScene::AddAll (Loader *pLoader)
{
    pLoader->Add (
        [this] ()
        {
            layerShader = CreateShaderProgram (GL_VERTEX_SHADER, layer_vsh,
                                               GL_GEOMETRY_SHADER, layer_gsh,
                                               GL_FRAGMENT_SHADER, layer_fsh);
            if (!layerShader)
                return false;

            /*
                Tell the shaders which attribute indices represent
                position, normal and texture coordinates:
             */
            glBindAttribLocation (layerShader, VERTEX_INDEX, "vertex");
            if (!CheckGLOK ("setting vertex attrib location"))
                return false;
            glBindAttribLocation (layerShader, NORMAL_INDEX, "normal");
            if (!CheckGLOK ("setting normal attrib location"))
                return false;
            glBindAttribLocation (layerShader, TEXCOORD_INDEX, "texCoord");
            if (!CheckGLOK ("setting texCoord attrib location"))
                return false;

            // Pass on parameters to the shader:
            GLint loc;

            glUseProgram (layerShader);
            loc = glGetUniformLocation (layerShader, "top_color");
            glUniform3fv (loc, 1, GRASS_TOP_COLOR);

            loc = glGetUniformLocation (layerShader, "bottom_color");
            glUniform3fv (loc, 1, GRASS_BOTTOM_COLOR);

            loc = glGetUniformLocation (layerShader, "extend_max");
            glUniform1f (loc, GRASS_HEIGHT);

            loc = glGetUniformLocation (layerShader, "n_layers");
            glUniform1i (loc, N_GRASS_LAYERS);
            glUseProgram (0);

            return true;
        }
    );

    pLoader->Add (
        [this] ()
        {
            polyShader = CreateShaderProgram (GL_VERTEX_SHADER, poly_ground_vsh,
                                              GL_FRAGMENT_SHADER, poly_fsh);
            if (!polyShader)
                return false;

            /*
                Tell the shaders which attribute indices represent
                position, normal and texture coordinates:
             */
            glBindAttribLocation (polyShader, VERTEX_INDEX, "vertex");
            if (!CheckGLOK ("setting vertex attrib location"))
                return false;
            glBindAttribLocation (polyShader, NORMAL_INDEX, "normal");
            if (!CheckGLOK ("setting normal attrib location"))
                return false;
            glBindAttribLocation (polyShader, TEXCOORD_INDEX, "texCoord");
            if (!CheckGLOK ("setting texCoord attrib location"))
                return false;

            // Pass on parameters to the shader:
            GLint loc;

            glUseProgram (polyShader);
            loc = glGetUniformLocation (polyShader, "top_color");
            glUniform3fv (loc, 1, GRASS_TOP_COLOR);

            loc = glGetUniformLocation (polyShader, "bottom_color");
            glUniform3fv (loc, 1, GRASS_BOTTOM_COLOR);
            glUseProgram (0);

            return true;
        }
    );

    pLoader->Add (
        [this] ()
        {
            groundShader = CreateShaderProgram (GL_VERTEX_SHADER, poly_ground_vsh,
                                                GL_FRAGMENT_SHADER, ground_fsh);
            if (!groundShader)
                return false;

            /*
                Tell the shaders which attribute indices represent
                position, normal and texture coordinates:
             */
            glBindAttribLocation (groundShader, VERTEX_INDEX, "vertex");
            if (!CheckGLOK ("setting vertex attrib location"))
                return false;
            glBindAttribLocation (groundShader, NORMAL_INDEX, "normal");
            if (!CheckGLOK ("setting normal attrib location"))
                return false;
            glBindAttribLocation (groundShader, TEXCOORD_INDEX, "texCoord");
            if (!CheckGLOK ("setting texCoord attrib location"))
                return false;

            // Pass on parameters to the shader:
            GLint loc;

            glUseProgram (groundShader);
            loc = glGetUniformLocation (groundShader, "bottom_color");
            glUniform3fv (loc, 1, GRASS_BOTTOM_COLOR);
            glUseProgram (0);

            return true;
        }
    );

    pLoader->Add (LoadPNGFunc (zipPath.c_str(), "dots.png", &texDots));

    pLoader->Add (
        [this, zipPath] ()
        {
            if (!LoadPNG (zipPath.c_str(), "grass.png", &texGrass))
                return false;

            // Don't repeat at the edges of the grass blade texture:
            glBindTexture (GL_TEXTURE_2D, texGrass.tex);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture (GL_TEXTURE_2D, 0);

            return true;
        }
    );

    // The first chunks:
    pLoader->Add (
        [this] ()
        {
            ChunkNumber xi, zi;
            for (xi = -CHUNK_LOAD_DIST; xi <= CHUNK_LOAD_DIST; xi++)
            {
                for (zi = -CHUNK_LOAD_DIST; zi <= CHUNK_LOAD_DIST; zi++)
                {
                    auto id = std::make_tuple (xi, zi);
                    chunks [id] = new GrassChunk (xi, zi);
                    if (!chunks [id]->Generate (pTerraGen))
                    {
                        SetError("error generating chunk: %s", GetError());
                        return false;
                    }
                }
            }

            return true;
        }
    );
}
#define MOUSEMOVE_SENSITIVITY 0.007f
void GrassScene::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    playerLookAngleY += MOUSEMOVE_SENSITIVITY * event -> xrel;
    playerLookAngleX += MOUSEMOVE_SENSITIVITY * event -> yrel;

    // clamp this angle:
    if (playerLookAngleX < -1.45f)
        playerLookAngleX = -1.45f;
    if (playerLookAngleX > 1.45f)
        playerLookAngleX = 1.45f;

    if (!SDL_GetRelativeMouseMode ())
    {
        if (SDL_SetRelativeMouseMode(SDL_TRUE) < 0)
        {
            fprintf (stderr, "error setting relative mouse mode: %s",
                     SDL_GetError ());
        }
        playerLookAngleX = 0.0f;
        playerLookAngleY = 0.0f;
    }
}
void GrassScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (event->type == SDL_KEYDOWN)
    {
        if (event->keysym.sym == SDLK_m)
        {
            mode = (mode + 1) % N_GRASSMODES;
        }
    }
}
vec3 RotateToPlane (const vec3 &horizontalMovement, const vec3 &n)
{
    float length = horizontalMovement.Length ();

    vec3 dir = PlaneProjection (horizontalMovement, {n, 0.0}).Unit ();

    return length * dir;
}

void GetChunksTriangles (std::list <Triangle> &triangles,
                         const std::map <ChunkID, GrassChunk*> &chunks, const vec3 &aroundPos)
{
    for (auto pair : chunks)
    {
        float x1, z1, x2, z2;
        pair.second->GetPositionRange (x1, z1, x2, z2);
        if (aroundPos.x > (x1 - 1.0f) && aroundPos.x < (x2 + 1.0f) &&
                aroundPos.z > (z1 - 1.0f) && aroundPos.z < (z2 + 1.0f))

            for (Triangle t : pair.second->GetCollisionTriangles ())
                triangles.push_back (t);
    }
}

#define GRAVITY 10.0f
#define FEET_ACC 40.0f
#define STATIC_FRICTION_COEFF 1.0f
#define KINETIC_FRICTION_COEFF 0.8f
void GrassScene::Update (const float dt)
{
    // Free chunks and load new ones.
    bool hit;
    Triangle tr;
    vec3 groundUnderPlayer, playerFeetPos;

    UpdateChunks (chunks, pTerraGen, playerPos);

    std::list <Triangle> collTriangles;
    GetChunksTriangles (collTriangles, chunks, playerPos);

    t += dt;
    wind.x = 0.1f * sin (t);

    const Uint8 *state = SDL_GetKeyboardState (NULL);

    vec3 a_gravity = vec3 (0.0f, -GRAVITY, 0.0f),
         a_normal = VEC_O,
         a_friction = VEC_O,
         a_drag = VEC_O,
         a_feet = VEC_O;

    // OnGround test:
    playerFeetPos = vec3 (playerPos.x,
                          playerPos.y + PLAYER_FEET_Y,
                          playerPos.z);
    std::tie (hit, tr, groundUnderPlayer) =
            CollisionTraceBeam (playerFeetPos,
                                playerFeetPos + VEC_DOWN, collTriangles);

    vec3 canMovePlayerDown = CollisionClosestBump (playerPos,
                                                   playerPos + VEC_DOWN,
                                                   playerColliders,
                                                   collTriangles);

    playerOnGround = (playerPos - canMovePlayerDown).Length () < 0.005f;


    a_drag = -0.05f * playerVelocity.Length2 () * playerVelocity.Unit ();

    if (playerOnGround)
    {
        if (state [SDL_SCANCODE_W]) {
            a_feet += FEET_ACC * (matRotY (playerLookAngleY) * VEC_FORWARD);
        }
        else if (state [SDL_SCANCODE_S]) {
            a_feet += FEET_ACC * (matRotY (playerLookAngleY) * VEC_BACK);
        }
        if (state [SDL_SCANCODE_A]) {
            a_feet += FEET_ACC * (matRotY (playerLookAngleY) * VEC_LEFT);
        }
        else if (state [SDL_SCANCODE_D]) {
            a_feet += FEET_ACC * (matRotY (playerLookAngleY) * VEC_RIGHT);
        }
        a_feet = RotateToPlane (a_feet, tr.GetPlane ().n);

        if (state [SDL_SCANCODE_SPACE]) // jump
        {
            playerOnGround = false;
            playerVelocity += 5.0f * tr.GetPlane ().n; // independent of dt !
        }
        else
        {
            a_normal = GRAVITY * tr.GetPlane ().n;

            // sum of forces that friction must counteract:
            vec3 a_applied = a_gravity + a_normal + a_feet;

            if (a_applied.Length () > STATIC_FRICTION_COEFF * GRAVITY)

                a_friction = -KINETIC_FRICTION_COEFF * GRAVITY * a_applied.Unit ();
            else
                a_friction = -a_applied;
            a_drag = -10.0f * playerVelocity.Length () * playerVelocity.Unit ();
        }
    }

    playerVelocity += (a_gravity + a_normal + a_feet + a_drag + a_friction) * dt;

    vec3 targetPlayerPos = playerPos + playerVelocity * dt;

    playerPos = CollisionMove (playerPos, targetPlayerPos,
                               playerColliders,
                               collTriangles);

    // Try to keep it on the ground after one step.
    if (playerOnGround)
        playerPos = CollisionClosestBump (playerPos + 0.1f * VEC_UP,
                                          playerPos + 0.1f * VEC_DOWN,
                                          playerColliders,
                                          collTriangles);
}

const GLfloat ambient [] = {0.3f, 0.3f, 0.3f, 1.0f},
              diffuse [] = {0.7f, 0.7f, 0.7f, 1.0f};
const vec3 lightDir = vec3 (0.0f, -1.0f, 0.0f);

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f
void GrassScene::Render (void)
{
    GLint loc;
    int i, w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    glClearColor (0.0f, 0.7f, 1.0f, 1.0f);
    glClearDepth (1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set the 3d projection matrix:
    matrix4 matProj = matPerspec (VIEW_ANGLE, (GLfloat) w / (GLfloat) h,
                                  NEAR_VIEW, FAR_VIEW);
    glMatrixMode (GL_PROJECTION);
    glLoadMatrixf (matProj.m);

    // Set the model matrix to first person view:
    const vec3 lookDir = matRotY (playerLookAngleY) *
                         matRotX (playerLookAngleX) * VEC_FORWARD,
               camPos = vec3 (playerPos.x,
                              playerPos.y + PLAYER_EYE_Y,
                              playerPos.z);
    const matrix4 matView = matLookAt (camPos,
                                       camPos + lookDir, VEC_UP),
                  matViewNorm = matTranspose (matInverse (matView));
    glMatrixMode (GL_MODELVIEW);
    glLoadMatrixf (matView.m);

    glDisable (GL_COLOR_MATERIAL);

    glEnable (GL_LIGHTING);
    glEnable (GL_LIGHT0);
    glLightfv (GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuse);
    GLfloat vLightPos [4] = {0.0f, 1.0f, 0.0f, 0.0f};
    glLightfv (GL_LIGHT0, GL_POSITION, vLightPos);

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDepthMask (GL_TRUE);

    glDisable (GL_STENCIL_TEST);
    glDisable (GL_CULL_FACE);

    switch (mode)
    {
    case GRASSMODE_LAYER:

        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable (GL_TEXTURE_2D);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, texDots.tex);

        glUseProgram (layerShader);

        // Pass on variables to the shader:
        loc = glGetUniformLocation (layerShader, "projMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matProj.m);

        loc = glGetUniformLocation (layerShader, "modelViewMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matView.m);

        loc = glGetUniformLocation (layerShader, "eyeLightDir");
        glUniform3fv(loc, 1, (matViewNorm * lightDir).v);

        loc = glGetUniformLocation (layerShader, "wind");
        glUniform3fv (loc, 1, wind.v);

        // Render The Grass layer by layer:
        loc = glGetUniformLocation (layerShader, "layer");
        for (i = 0; i < N_GRASS_LAYERS; i++)
        {
            glUniform1i (loc, i);
            for (auto pair : chunks)
                pair.second->RenderGroundVertices ();
        }

        glUseProgram (0);
    break;
    case GRASSMODE_POLYGON:

        // Render hill first:
        glUseProgram (groundShader);

        // Pass on variables to the shader:
        loc = glGetUniformLocation (groundShader, "projMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matProj.m);

        loc = glGetUniformLocation (groundShader, "modelViewMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matView.m);

        loc = glGetUniformLocation (groundShader, "eyeLightDir");
        glUniform3fv(loc, 1, (matViewNorm * lightDir).v);

        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable (GL_TEXTURE_2D);

        for (auto pair : chunks)
        {
            pair.second->RenderGroundVertices ();
        }


        // Now render the blades:
        glEnable (GL_TEXTURE_2D);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, texGrass.tex);

        glUseProgram (polyShader);

        // Pass on variables to the shader:
        loc = glGetUniformLocation (polyShader, "projMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matProj.m);

        loc = glGetUniformLocation (polyShader, "modelViewMatrix");
        glUniformMatrix4fv (loc, 1, GL_FALSE, matView.m);

        loc = glGetUniformLocation (polyShader, "eyeLightDir");
        glUniform3fv(loc, 1, (matViewNorm * lightDir).v);

        /*
            Transparent fragments must not be rendered,
            because we need the depth buffer here.
         */
        glAlphaFunc (GL_GREATER, 0.99f) ;
        glEnable (GL_ALPHA_TEST);
        for (auto pair : chunks)
        {
            pair.second->RenderGrassBladeVertices (wind);
        }
        glDisable (GL_ALPHA_TEST);

        glUseProgram (0);
    break;
    }
}
