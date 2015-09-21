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


#include "shadow.h"
#include "app.h"
#include <math.h>
#include <string>
#include "../matrix.h"
#include <stdio.h>
#include "../io.h"
#include "../util.h"
#include "../xml.h"
#include "../random.h"
#include "../err.h"
#include "../shader.h"

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f

#define BOX_WIDTH 20.0f
#define BOX_HEIGHT 6.0f
#define BOX_LENGTH 20.0f

#define PLAYER_RADIUS 1.2f

#define PLAYER_START vec3 (0.1f,2.0f,-5.0f)

#define PARTICLE_RADIUS 10.0f
#define PARTICLE_MIN_Y -3.0f
#define PARTICLE_MAX_Y 3.0f
#define PARTICLE_INTERVAL_MIN 0.2f
#define PARTICLE_INTERVAL_MAX 0.4f
#define PARTICLE_MAXSPEED 5.0f

const char

normal_vsh [] = R"shader(

    varying vec3 b, t, n, v;

    attribute vec3 tangent,
                   bitangent;

    void main()
    {
        v = vec3 (gl_ModelViewMatrix * gl_Vertex);

        n = normalize (gl_NormalMatrix * gl_Normal);
        t = normalize (gl_NormalMatrix * tangent);
        b = normalize (gl_NormalMatrix * bitangent);

        gl_Position = ftransform();
        gl_TexCoord[0] = gl_MultiTexCoord0;
    }

)shader",

normal_fsh [] = R"shader(

    uniform sampler2D tex_color,
                      tex_normal;

    varying vec3 b, t, n, v;

    void main()
    {
        vec4 normal_color = texture2D (tex_normal, gl_TexCoord[0].st);

        vec3 L = normalize (gl_LightSource[0].position.xyz - v),

                 texNormal = vec3 (2 * (normal_color.r - 0.5),
                                   2 * (normal_color.g - 0.5),
                                   normal_color.b);

        texNormal = normalize (texNormal.z * normalize (n) +
                               texNormal.x * normalize (t) +
                               texNormal.y * normalize (b));

        float lum = 0.3 + clamp (dot(texNormal, L), 0.0, 1.0);

        gl_FragColor = lum * texture2D (tex_color, gl_TexCoord[0].st);
    }

)shader";

ShadowScene::ShadowScene(App *pApp) : Scene(pApp),
    frame (0),
    shadowTriangles (NULL),
    angleX (0.3f),
    angleY (0.0f),
    distCamera (7.0f),
    posPlayer (PLAYER_START),
    directionPlayer (0,0,1),
    vy (0),
    onGround (false), touchDown (false),
    show_bones (false), show_normals (false), show_triangles (false),
    pMeshDummy (NULL),
    collision_triangles (NULL), n_collision_triangles (0)
{
    texDummyNormal.tex = texDummy.tex = texBox.tex = texSky.tex = texPar.tex = 0;


    // colliders [0] = new FeetCollider (vec3 (0, -2.0f, 0));

    /*
        Decided not to use the feet collider here. it's too buggy for onGround tests.
     */
    colliders [0] = new SphereCollider (vec3 (0, -1.999f, 0), 0.001f);
    colliders [1] = new SphereCollider (VEC_O, PLAYER_RADIUS);

    // Place particles at random positions with random speeds and sizes
    for (int i = 0; i < N_PARTICLES; i++)
    {
        float a = RandomFloat (0, M_PI * 2),
              r = RandomFloat (0, PARTICLE_RADIUS),
              h = RandomFloat (PARTICLE_MIN_Y, PARTICLE_MAX_Y),
              s = sqrt (PARTICLE_MAXSPEED) / 3;

        particles [i].pos = vec3 (r * cos (a), h, r * sin (a));
        particles [i].vel = vec3 (RandomFloat (-s, s),
                                  RandomFloat (-s, s),
                                  RandomFloat (-s, s));
        particles [i].size = RandomFloat (0.2f, 0.6f);
    }
}
ShadowScene::~ShadowScene()
{
    glDeleteTextures(1, &texDummyNormal.tex);
    glDeleteTextures(1, &texDummy.tex);
    glDeleteTextures(1, &texBox.tex);
    glDeleteTextures(1, &texSky.tex);
    glDeleteTextures(1, &texPar.tex);

    glDeleteProgram (shader_normal);

    delete pMeshDummy;
    delete [] shadowTriangles;
    delete [] collision_triangles;

    for (int i=0; i < N_PLAYER_COLLIDERS; i++)
        delete colliders [i];
}

bool ShadowScene::Init(void)
{
    /*
        Load and parse all resources.
        On failure, set the error message and return false.
     */

    std::string resPath = std::string(SDL_GetBasePath()) + "test3d.zip";

    SDL_RWops *f;
    bool success;
    xmlDocPtr pDoc;

    // Load dummy texture:

    f = SDL_RWFromZipArchive (resPath.c_str(), "dummy.png");
    if (!f) // file or archive missing
        return false;

    success = LoadPNG(f, &texDummy);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing dummy.png: %s", GetError ());
        return false;
    }

    // Load dummy normal texture:

    f = SDL_RWFromZipArchive (resPath.c_str(), "dummy_n.png");
    if (!f) // file or archive missing
        return false;

    success = LoadPNG(f, &texDummyNormal);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing dummy_n.png: %s", GetError ());
        return false;
    }

    // Load dummy mesh as xml document:

    f = SDL_RWFromZipArchive (resPath.c_str(), "dummy.xml");
    if (!f)
        return false;
    pDoc = ParseXML(f);
    f->close(f);

    if (!pDoc)
    {
        SetError ("error parsing dummy.xml: %s", GetError ());
        return false;
    }

    // Convert xml document to mesh object:

    success = ParseMesh(pDoc, &meshDataDummy);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing dummy.xml: %s", GetError ());
        return false;
    }

    // Load environment texture:

    f = SDL_RWFromZipArchive (resPath.c_str(), "box.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texBox);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing box.png: %s", GetError ());
        return false;
    }

    // Load environment mesh as xml:

    f = SDL_RWFromZipArchive (resPath.c_str(), "box.xml");
    if (!f)
        return false;
    pDoc = ParseXML(f);
    f->close(f);

    if (!pDoc)
    {
        SetError ("error parsing box.xml: %s", GetError ());
        return false;
    }

    // Convert xml to mesh:

    success = ParseMesh(pDoc, &meshDataBox);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing box.xml: %s", GetError ());
        return false;
    }

    // Load skybox texture:

    f = SDL_RWFromZipArchive (resPath.c_str(), "sky.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texSky);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing sky.png: %s", GetError ());
        return false;
    }

    // Load skybox mesh as xml:

    f = SDL_RWFromZipArchive (resPath.c_str(), "sky.xml");
    if (!f)
        return false;
    pDoc = ParseXML(f);
    f->close(f);

    if (!pDoc)
    {
        SetError ("error parsing sky.xml: %s", GetError ());
        return false;
    }

    // convert xml to mesh:

    success = ParseMesh(pDoc, &meshDataSky);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing sky.xml: %s", GetError ());
        return false;
    }

    // Load texture for the particles:

    f = SDL_RWFromZipArchive (resPath.c_str(), "particle.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texPar);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing particle.png: %s", GetError ());
        return false;
    }

    shader_normal = CreateShaderProgram (normal_vsh, normal_fsh);
    if (!shader_normal)
    {
        SetError ("error creating normal shader program: %s", GetError ());
        return false;
    }

    // Derive extra objects from the data we just loaded:

    shadowTriangles = new STriangle[meshDataDummy.triangles.size() + 2 * meshDataDummy.quads.size()];

    pMeshDummy = new MeshObject (&meshDataDummy);
    ToTriangles (&meshDataBox, &collision_triangles, &n_collision_triangles);

    // Put the player on the ground we just generated
    posPlayer = CollisionMove (posPlayer, vec3 (posPlayer.x, -1000.0f, posPlayer.z),
                               colliders, N_PLAYER_COLLIDERS, collision_triangles, n_collision_triangles);

    return true;
}

/*
float toAngle(const float x, const float z)
{
    if (x == 0)
    {
        if (z > 0)
            return M_PI / 2;
        else
            return -M_PI / 2;
    }
    else if (x > 0)
    {
        return atan (z / x);
    }
    else // x <= 0
    {
        return M_PI - atan (z / x);
    }
}
*/

void getCameraPosition( const vec3 &center,
        const GLfloat angleX, const GLfloat angleY, const GLfloat dist,
    vec3 &posCamera)
{
    // Place camera at chosen distance and angle
    posCamera = center + matRotY (angleY) * matRotX (angleX) * vec3 (0, 0, -dist);
}
void ShadowScene::Update(const float dt)
{
    vec3 posCamera, newPosPlayer = posPlayer;
    bool forward = false;

    // Detect movement input:
    const Uint8 *state = SDL_GetKeyboardState (NULL);

    if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_D]
        || state[SDL_SCANCODE_W] || state[SDL_SCANCODE_S])
    {
        getCameraPosition(posPlayer, angleX, angleY, distCamera, posCamera);

        // Move dummy, according to camera view and buttons pressed
        GLfloat mx, mz;

        if (state [SDL_SCANCODE_W])
            mz = 1.0f;
        else if (state [SDL_SCANCODE_S])
            mz = -1.0f;
        else
            mz = 0.0f;

        if (state [SDL_SCANCODE_A])
            mx = -1.0f;
        else if (state [SDL_SCANCODE_D])
            mx = 1.0f;
        else
            mx = 0.0f;

        vec3 camZ = posPlayer - posCamera;
        camZ.y = 0;
        camZ = camZ.Unit ();
        vec3 camX = Cross (VEC_UP, camZ);

        directionPlayer = mx * camX + mz * camZ;

        newPosPlayer += 5 * directionPlayer.Unit () * dt;
        forward = true;
    }

    // if falling, see if player hit the ground
    onGround = (vy <= 0.0f) && TestOnGround (posPlayer, colliders, N_PLAYER_COLLIDERS, collision_triangles, n_collision_triangles);

    if (onGround)
        vy = 0.0f;
    else
    {
        touchDown = true;
        vy -= 30.0f * dt;
    }

    newPosPlayer.y += vy * dt;

    /*
        If on the ground, the collision mechanics are a little different.
        We try to keep the feet on the ground. Unless the fall is too long
     */
    if (onGround)

        posPlayer = CollisionWalk (posPlayer, newPosPlayer, colliders, N_PLAYER_COLLIDERS, collision_triangles, n_collision_triangles);
    else
        posPlayer = CollisionMove (posPlayer, newPosPlayer, colliders, N_PLAYER_COLLIDERS, collision_triangles, n_collision_triangles);

    // animate the mesh:

    if (onGround && forward) // walking / running animation
    {
        frame += 15 * dt;
        pMeshDummy->SetAnimationState ("run", frame);
        touchDown = false;
    }
    else if (vy > 0) // going up animation
    {
        frame = 1.0f;
        pMeshDummy->SetAnimationState ("jump", frame);
    }
    else if (!onGround) // falling animation
    {
        frame += 30 * dt;
        if (frame > 10.0f)
            frame = 10.0f;

        pMeshDummy->SetAnimationState ("jump", frame);
    }
    else // onGround and not moving forward
    {
        frame += 15 * dt;
        if (frame > 20.0f)
            touchDown = false;

        if (touchDown) // touchdown animation

            pMeshDummy->SetAnimationState ("jump", frame);

        else // stationary animation

            pMeshDummy->SetAnimationState (NULL, 0);
    }


    // Move the particles, according to time passed.
    for (int i = 0; i < N_PARTICLES; i++)
    {
        particles [i].vel += -0.03f * particles [i].pos * dt;
        particles [i].pos += particles [i].vel * dt;
    }
}
const GLfloat ambient [] = {0.3f, 0.3f, 0.3f, 1.0f},
              diffuse [] = {1.0f, 1.0f, 1.0f, 1.0f};

const vec3 posLight (0.0f, 10.0f, 0.0f);

/*
 * Extract STriangle objects from a mesh to compute drop shadows from.
 */
int GetTriangles(const MeshObject *pMesh, STriangle *triangles)
{
    const MeshData *pData = pMesh->GetMeshData();
    const std::map<std::string, MeshVertex> &vertices = pMesh->GetVertices();

    int i = 0;
    for (std::map<std::string, MeshQuad>::const_iterator it = pData->quads.begin(); it != pData->quads.end(); it++)
    {
        const MeshQuad *pQuad = &it->second;

        /*
            Converting quads to triangles might not be exactly right,
            unless the quads are completely flat of coarse.
         */

        triangles[i].p[0] = &vertices.at (pQuad->GetVertexID(0)).p;
        triangles[i].p[1] = &vertices.at (pQuad->GetVertexID(1)).p;
        triangles[i].p[2] = &vertices.at (pQuad->GetVertexID(2)).p;

        i++;

        triangles[i].p[0] = &vertices.at (pQuad->GetVertexID(2)).p;
        triangles[i].p[1] = &vertices.at (pQuad->GetVertexID(3)).p;
        triangles[i].p[2] = &vertices.at (pQuad->GetVertexID(0)).p;

        i++;
    }
    for (std::map<std::string, MeshTriangle>::const_iterator it = pData->triangles.begin(); it != pData->triangles.end(); it++)
    {
        const MeshTriangle *pTri = &it->second;

        triangles[i].p[0] = &vertices.at (pTri->GetVertexID(0)).p;
        triangles[i].p[1] = &vertices.at (pTri->GetVertexID(1)).p;
        triangles[i].p[2] = &vertices.at (pTri->GetVertexID(2)).p;

        i++;
    }

    return i;
}

/*
 * Determines which triangles are pointing towards eye and sets their visibility flag.
 */
void SetVisibilities(const vec3 &posEye, const int n_triangles, STriangle *triangles)
{
    for(int i = 0; i < n_triangles; i++)
    {
        STriangle* t = &triangles[i];

        /*
           Calculate normal from triangle,
           assume clockwise is front.
         */

        vec3 n = Cross ((*t->p[1] - *t->p[0]), (*t->p[2] - *t->p[0])).Unit();

        float d = -Dot (*t->p[0], n), // plane's distance from origin

              side = Dot(n, posEye) + d;

        // If the normal points towards the eye, then the triangle is visible.

        t->visible = (side > 0);
    }
}

struct Edge { const vec3 *p[2]; };

/**
 * Determines which triangle Edges should make up the shadow.
 * Precondition is that the 'visible' fields have been set on the triangles beforehand !!
 */
void GetShadowEdges(const int n_triangles, const STriangle *triangles, std::list<Edge> &result)
{

    // We're looking for edges that are not between two visible triangles,
    // but ARE part of a visible triangle

    /*
       one bool per edge of the triangle with points (0, 1, 2),
       0 between triangle point 0 and 1,
       1 between 1 and 2
       and 2 between 2 and 0
    */
    bool use_edge [3];

    int i, j, x, y, n, x2, y2;
    for (i = 0; i < n_triangles; i++)
    {
        // no need to check the edges of invisible triangles:
        if (!triangles[i].visible)
            continue;

        /*
            Include all edges by default.
            Iteration must rule out edges that are shared by two visible triangles.
         */
        for (x = 0; x < 3; x++)
            use_edge [x] = true;

        /*
            Iterate to find visible triangles (j)
            that share edges with visible triangles. (i)
         */
        for (j = 0; j < n_triangles; j++)
        {
            /*
               Make sure not to compare triangles with themselves
               and be sure to skip other invisible triangles.
            */

            if (i == j || !triangles [j].visible)
                continue;

            // compare the three edges of both triangles with each other:

            for (x = 0; x < 3; x++) // iterate over the edges of triangle i
            {
                x2 = (x + 1) % 3; // second point of edge x

                for (y = 0; y < 3; y++) // iterate over the edges of triangle j
                {
                    y2 = (y + 1) % 3; // second point of edge y

                    if (triangles[i].p[x] == triangles[j].p[y] && triangles[i].p[x2] == triangles[j].p[y2] ||
                        triangles[i].p[x2] == triangles[j].p[y] && triangles[i].p[x] == triangles[j].p[y2])
                    {
                        // edge x on triangle i is equal to edge y on triangle j

                        use_edge [x] = false;
                        break;
                    }
                }
            }

            /*
               If all three edges (0, 1, 2) of triangle (i) were found shared with other triangles,
               then search no more other triangles (j) for this particular triangle (i).
            */
            if (!use_edge [0] && !use_edge [1] && !use_edge [2])
                break;
        }

        // Add the edges (x) of triangle (i) that were found needed:
        for (x = 0; x < 3; x++)
        {
            if (use_edge [x])
            {
                x2 = (x + 1) % 3; // second point of edge x

                Edge edge;
                edge.p [0] = triangles [i].p [x];
                edge.p [1] = triangles [i].p [x2];
                result.push_back (edge);
            }
        }
    }
}

/**
 * Renders squares from every edge to infinity, using the light position as origin.
 */
#define SHADOW_INFINITY 1000.0f
void ShadowPass(const std::list<Edge> &edges, const vec3 &posLight)
{
    for (std::list<Edge>::const_iterator it = edges.begin(); it != edges.end(); it++)
    {
        const Edge *pEdge = &(*it);

        // These are the four vertices of the shadow quad:
        vec3 d1 = (*pEdge->p[0] - posLight).Unit(),
             d2 = (*pEdge->p[1] - posLight).Unit(),

             v0 = *pEdge->p[0] + d1 * SHADOW_INFINITY,
             v1 = *pEdge->p[1] + d2 * SHADOW_INFINITY;

        glBegin(GL_QUADS);
        glVertex3f(pEdge->p[1]->x, pEdge->p[1]->y, pEdge->p[1]->z);
        glVertex3f(pEdge->p[0]->x, pEdge->p[0]->y, pEdge->p[0]->z);
        glVertex3f(v0.x, v0.y, v0.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glEnd();
    }
}
void RenderSprite (const vec3 &pos, const Texture *pTex,
                   const float tx1, const float ty1, const float tx2, const float ty2, // texture coordinates (in pixels)
                   const float size = 1.0f)
{
    matrix4 modelView;
    glGetFloatv (GL_MODELVIEW_MATRIX, modelView.m);

    /*
       Extract the axes system from the current modelview matrix and normalize them,
       then render the quad on the normalized axes system, so that it's always pointed towards the camera,
       but it's zoom is still variable.
    */

    const float sz = size / 2;
    vec3 modelViewRight = vec3 (modelView.m11, modelView.m12, modelView.m13).Unit(),
         modelViewUp = vec3 (modelView.m21, modelView.m22, modelView.m23).Unit(),

         p1 = pos + sz * modelViewUp - sz * modelViewRight,
         p2 = pos + sz * modelViewUp + sz * modelViewRight,
         p3 = pos - sz * modelViewUp + sz * modelViewRight,
         p4 = pos - sz * modelViewUp - sz * modelViewRight;

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, pTex->tex);

    glBegin (GL_QUADS);
    glTexCoord2f (tx1 / pTex->w, ty1 / pTex->h);
    glVertex3f (p1.x, p1.y, p1.z);
    glTexCoord2f (tx2 / pTex->w, ty1 / pTex->h);
    glVertex3f (p2.x, p2.y, p2.z);
    glTexCoord2f (tx2 / pTex->w, ty2 / pTex->h);
    glVertex3f (p3.x, p3.y, p3.z);
    glTexCoord2f (tx1 / pTex->w, ty2 / pTex->h);
    glVertex3f (p4.x, p4.y, p4.z);
    glEnd ();
}

#define TANGENT_INDEX 16
#define BITANGENT_INDEX 17
void DummyRenderFunc (void *pObj, const int n_vertices, const MeshVertex **p_vertices, const MeshTexel *texels)
{
    if (n_vertices == 4)
        glBegin (GL_QUADS);
    else if (n_vertices == 3)
        glBegin (GL_TRIANGLES);
    else
        return;

    vec3 t, b, v0, v1;

    size_t i, j, k;

    float du0, dv0, du1, dv1;

    for(i = 0; i < n_vertices; i++)
    {
        glTexCoord2f (texels [i].u, texels [i].v);
        glNormal3f (p_vertices [i]->n.x, p_vertices [i]->n.y, p_vertices [i]->n.z);
        glVertex3f (p_vertices [i]->p.x, p_vertices [i]->p.y, p_vertices [i]->p.z);

        /*
          Calculate tangent and bitangent from neighbouring
          vertices (positions and texture coords)
         */

        j = (n_vertices + i - 1) % n_vertices;
        k = (i + 1) % n_vertices;

        v0 = p_vertices [j]->p - p_vertices [i]->p;
        du0 = texels [j].u - texels [i].u;
        dv0 = texels [j].v - texels [i].v;

        v1 = p_vertices [k]->p - p_vertices [i]->p;
        du1 = texels [k].u - texels [i].u;
        dv1 = texels [k].v - texels [i].v;

        float r = 1.0f / (du0 * dv1 - dv0 * du1);
        t = (v0 * dv1 - v1 * dv0) * r;
        b = (v1 * du0 - v0 * du1) * r;

        glVertexAttrib3f (TANGENT_INDEX, t.x, t.y, t.z);
        glVertexAttrib3f (BITANGENT_INDEX, b.x, b.y, b.z);
    }

    glEnd ();
}
#define SHADOW_STENCIL_MASK 0xFFFFFFFFL
void ShadowScene::Render ()
{
    GLint texLoc;

    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);

    vec3 posCamera;
    getCameraPosition (posPlayer, angleX, angleY, distCamera, posCamera);

    // We want the camera to stay at some minimal distance from the walls:
    vec3 shift = 0.5f * (posCamera - posPlayer).Unit(),
         intersection = CollisionTraceBeam(posPlayer, posCamera + shift, collision_triangles, n_collision_triangles) - shift;
    if (intersection != posCamera)
    {
        posCamera = intersection;
    }

    // Set the 3d projection matrix:
    glMatrixMode(GL_PROJECTION);
    matrix4 matCamera = matPerspec(VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf(matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode(GL_MODELVIEW);
    const matrix4 matCameraView = matLookAt(posCamera, posPlayer, VEC_UP),
                  matSkyBox = matLookAt(VEC_O, posPlayer - posCamera, VEC_UP);

    glDepthMask (GL_TRUE);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc (GL_ALWAYS, 0, SHADOW_STENCIL_MASK);
    glClearDepth (1.0f);
    glClearColor (0, 0, 0, 1.0);
    glClearStencil (0);
    glClear (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glColorMaterial (GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);

    glDisable (GL_STENCIL_TEST);

    glDisable (GL_BLEND);
    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

    glEnable (GL_CULL_FACE);
    glFrontFace (GL_CW);
    glCullFace (GL_BACK);

    glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);
    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);

    // Render skybox around camera
    glLoadMatrixf(matSkyBox.m);

    glBindTexture (GL_TEXTURE_2D, texSky.tex);
    // Render two half spheres, one is mirror image
    glDisable (GL_CULL_FACE);
    RenderUnAnimatedSubset (&meshDataSky, "0");
    glScalef(1.0f, -1.0f, 1.0f);
    RenderUnAnimatedSubset (&meshDataSky, "0");

    glLoadMatrixf(matCameraView.m);

    // Light settings, position will be set later
    glEnable (GL_LIGHTING);
    glLightModelfv (GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightfv (GL_LIGHT0, GL_DIFFUSE, diffuse);
    glEnable (GL_LIGHT0);
    GLfloat vLightPos [4] = {posLight.x, posLight.y, posLight.z, 1.0f};// 0 is directional
    glLightfv (GL_LIGHT0, GL_POSITION, vLightPos);

    glEnable (GL_CULL_FACE);

    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDepthFunc (GL_LEQUAL);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render the world:
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texBox.tex);
    RenderUnAnimatedSubset (&meshDataBox, "0");
    glDisable (GL_BLEND);
    glDisable (GL_TEXTURE_2D);

    if (show_triangles)
    {
        glDisable(GL_DEPTH_TEST);
        glColor4f(0.8f, 0.0f, 0.0f, 1.0f);
        for (int i = 0; i < n_collision_triangles; i++)
        {
            glBegin(GL_LINES);
            for (int j=0; j < 3; j++)
            {
                int k = (j + 1) % 3;
                glVertex3f(collision_triangles[i].p[j].x, collision_triangles[i].p[j].y, collision_triangles[i].p[j].z);
                glVertex3f(collision_triangles[i].p[k].x, collision_triangles[i].p[k].y, collision_triangles[i].p[k].z);
            }
            glEnd();
        }

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
    }

    // Render player (and bones/normals if requested)
    glEnable (GL_TEXTURE_2D);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texDummy.tex);
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, texDummyNormal.tex);

    matrix4 transformPlayer = matTranslation (posPlayer) * matInverse (matLookAt (VEC_O, directionPlayer, VEC_UP));

    glMultMatrixf (transformPlayer.m);

    glUseProgram (shader_normal);

    texLoc = glGetUniformLocation (shader_normal, "tex_color");
    glUniform1i (texLoc, 0);

    texLoc = glGetUniformLocation (shader_normal, "tex_normal");
    glUniform1i (texLoc, 1);

    glBindAttribLocation (shader_normal, TANGENT_INDEX, "tangent");
    glBindAttribLocation (shader_normal, BITANGENT_INDEX, "bitangent");

    pMeshDummy->ThroughSubsetFaces("0", DummyRenderFunc);

    glUseProgram (0);
    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, 0);

    glDisable (GL_DEPTH_TEST);
    glDisable (GL_LIGHTING);
    glDisable (GL_TEXTURE_2D);
    if (show_bones)
    {
        glColor4f(0.0f, 1.0f, 1.0f, 1.0f);
        pMeshDummy->RenderBones ();
    }
    glEnable (GL_DEPTH_TEST);
    if (show_normals)
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        pMeshDummy->RenderNormals ();
    }

    // We need to have the light position in player space, because that's where the triangles are.

    const vec3 invPosLight = matInverse (transformPlayer) * posLight;
    const int n_triangles = GetTriangles (pMeshDummy, shadowTriangles);
    SetVisibilities (invPosLight, n_triangles, shadowTriangles);
    std::list<Edge> edges;
    GetShadowEdges (n_triangles, shadowTriangles, edges);

    glDisable (GL_LIGHTING); // Turn Off Lighting
    glDepthMask (GL_FALSE); // Turn Off Writing To The Depth-Buffer
    glDepthFunc (GL_LEQUAL);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);// Don't Draw Into The Colour Buffer
    glEnable (GL_STENCIL_TEST); // Turn On Stencil Buffer Testing
    glStencilFunc (GL_ALWAYS, 1, SHADOW_STENCIL_MASK);

    // first pass, increase stencil value on the shadow's outside
    glFrontFace (GL_CW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_INCR);
    ShadowPass (edges, invPosLight);

    // second pass, decrease stencil value on the shadow's inside
    glFrontFace (GL_CCW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_DECR);
    ShadowPass (edges, invPosLight);

    // Set the stencil pixels to zero where the dummy is drawn and where nothing is drawn (background)
    glFrontFace (GL_CW);
    glStencilOp (GL_KEEP, GL_KEEP, GL_ZERO);
    pMeshDummy->RenderSubset ("0");

    glLoadIdentity ();

    // Set every stencil pixel behind the walls/floor to zero
    glBegin (GL_QUADS);
        glVertex3f (-1.0e+15f, 1.0e+15f, 0.9f * SHADOW_INFINITY);
        glVertex3f ( 1.0e+15f, 1.0e+15f, 0.9f * SHADOW_INFINITY);
        glVertex3f ( 1.0e+15f,-1.0e+15f, 0.9f * SHADOW_INFINITY);
        glVertex3f (-1.0e+15f,-1.0e+15f, 0.9f * SHADOW_INFINITY);
    glEnd ();

    // Turn color rendering back on and draw to nonzero stencil pixels
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc (GL_NOTEQUAL, 0, SHADOW_STENCIL_MASK);
    glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP);

    // Draw a shadowing rectangle, covering the entire screen
    glEnable (GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glColor4f (0.0f, 0.0f, 0.0f, 0.4f);
    glBegin (GL_QUADS);
        glVertex3f (-1.0f, 1.0f, NEAR_VIEW + 0.1f);
        glVertex3f ( 1.0f, 1.0f, NEAR_VIEW + 0.1f);
        glVertex3f ( 1.0f,-1.0f, NEAR_VIEW + 0.1f);
        glVertex3f (-1.0f,-1.0f, NEAR_VIEW + 0.1f);
    glEnd ();

    /*
       Render the particles after the shadows, because they're transparent
       and that doesn't go well with the depth buffer.
    */

    glColor4f (1.0f, 1.0f, 1.0f, 1.0f);
    glLoadMatrixf (matCameraView.m);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_STENCIL_TEST);
    glEnable (GL_TEXTURE_2D);

    for (int i = 0; i < N_PARTICLES; i++)
    {
        RenderSprite (particles [i].pos, &texPar, 0, 0, 64, 64, particles [i].size);
    }
}
void ShadowScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out, but not closer than 0.5

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
void ShadowScene::OnKeyPress (const SDL_KeyboardEvent *event)
{
    if (event->type == SDL_KEYDOWN)
    {
        if (event->keysym.sym == SDLK_SPACE && onGround)
        {
            // jump
            vy = 15.0f;
        }

        /*
            The following keys toggle settings:
         */
        else if (event->keysym.sym == SDLK_b)
        {
            show_bones = !show_bones;
        }
        else if (event->keysym.sym == SDLK_n)
        {
            show_normals = !show_normals;
        }
        else if (event->keysym.sym == SDLK_z)
        {
            show_triangles = !show_triangles;
        }
    }
}
void ShadowScene::OnMouseMove(const SDL_MouseMotionEvent *event)
{
    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change camera angles, if mouse key is pressed

        angleY += 0.01f * event -> xrel;
        angleX += 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < -1.5f)
            angleX = -1.5f;
    }
}
