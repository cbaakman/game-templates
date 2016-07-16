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

#include "chunk.h"

#include <GL/gl.h>

#include "../random.h"
#include "../err.h"

template <>
inline TerrainType ForNode (const std::string &seed, const vec2 &pos)
{
    size_t hash = HashFromStr (seed);
    float value;

    value = NoiseFrom (hash);
    value = NoiseFrom (value + pos.x);
    value = NoiseFrom (value * pos.z);

    TerrainType t;
    t.height = -1 + 2 * sin (value);
    return t;
}
GrassChunk::GrassChunk (ChunkNumber x, ChunkNumber z):
    numX(x), numZ(z),
    grass_neutral_positions(NULL)
{
    glGenBuffer (&vbo_ground);
    glGenBuffer (&vbo_grass);
}
GrassChunk::~GrassChunk ()
{
    delete [] grass_neutral_positions;

    glDeleteBuffer (&vbo_ground);
    glDeleteBuffer (&vbo_grass);
}
void GrassChunk::GetChunkNumbers (ChunkNumber &x, ChunkNumber &z) const
{
    x = numX;
    z = numZ;
}
void GrassChunk::GetPositionRange (float &x1, float &z1,
                              float &x2, float &z2) const
{
    x1 = ((float)numX - 0.5f) * PER_CHUNK_SIZE;
    x2 = ((float)numX + 0.5f) * PER_CHUNK_SIZE;
    z1 = ((float)numZ - 0.5f) * PER_CHUNK_SIZE;
    z2 = ((float)numZ + 0.5f) * PER_CHUNK_SIZE;
}
std::tuple <ChunkNumber, ChunkNumber>
    ChunkNumbersFor (const float x, const float z)
{
    ChunkNumber numX = (ChunkNumber)round (x / PER_CHUNK_SIZE),
                numZ = (ChunkNumber)round (z / PER_CHUNK_SIZE);

    return std::make_tuple (numX, numZ);
}
const std::list <Triangle> &GrassChunk::GetCollisionTriangles (void) const
{
    return collTriangles;
}
bool FillGrassBladeVertexBuffer (VertexBuffer *pBuffer, const std::list <Triangle> &groundTriangles,
                                 vec3 **p_grass_neutral_positions)
{
    size_t i = 0,
           m, u, j, k;

    const int triangles [][3] = {{0, 1, 2}, {0, 2, 3}};

    // Hard to predict this exactly, depends on the surface area of the floor:
    const size_t n_vertices_estimate = GRASS_PER_M2 * 10 * groundTriangles.size ();

    // The buffer must be dynamic, because the wind will deform the blades all the time:
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (GrassVertex) * n_vertices_estimate, NULL, GL_DYNAMIC_DRAW);
    *p_grass_neutral_positions = new vec3 [n_vertices_estimate];

    GrassVertex *pbuf = (GrassVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return false;
    }

    // Generate blade quads for the entire surface of the mesh:
    for (const Triangle &groundTriangle : groundTriangles)
    {
        const vec3 p0 = groundTriangle.p[0],
                   p1 = groundTriangle.p[1],
                   p2 = groundTriangle.p[2],
                   n = Cross (p1 - p0, p2 - p0).Unit ();

        const float l01 = (p1 - p0).Length (),
                    l02 = (p2 - p0).Length (),
                    l12 = (p2 - p1).Length (),
                    s = (l01 + l02 + l12) / 2,
                    area = sqrt (s * (s - l01) * (s - l02) * (s - l12));

        const size_t n_blades = area * GRASS_PER_M2;
        for (j = 0; j < n_blades; j++)
        {
            if ((i + 6) >= n_vertices_estimate)
            {
                SetError ("vertex estimate exceeded");
                return false;
            }

            // Give each blade a random orientation and position on the triangle.
            float r1 = RandomFloat (0.0f, 1.0f),
                  r2 = RandomFloat (0.0f, 1.0f),
                  a = RandomFloat (0, 2 * M_PI),
                  b = RandomFloat (-MAX_GRASS_ANGLE, MAX_GRASS_ANGLE),
                  c = RandomFloat (-MAX_GRASS_ANGLE, MAX_GRASS_ANGLE);

            if (r2 > (1.0f - r1))
            {
                /*
                    Point is outside the triangle, but inside the parallelogram.
                    Rotate 180 degrees to fix
                 */

                r2 = 1.0f - r2;
                r1 = 1.0f - r1;
            }

            matrix4 transf = matRotX (c) * matRotZ (b) * matRotY (a);

            vec3 base = p0 + r1 * (p1 - p0) + r2 * (p2 - p0),
                 vertical = transf * n,
                 horizontal = transf * Cross (n, VEC_FORWARD),
                 normal = transf * VEC_BACK;

            // move grass down into the ground to correct for angle:
            base -= vertical * (GRASS_WIDTH / 2) * sin (b);

            // Make a quad from two triangles (6 vertices):

            pbuf [i].v.p = base - (GRASS_WIDTH / 2) * horizontal;
            pbuf [i].v.n = normal;
            pbuf [i].t = {0.0f, 0.0f};

            pbuf [i + 1].v.p = pbuf [i].v.p + GRASS_HEIGHT * vertical;
            pbuf [i + 1].v.n = normal;
            pbuf [i + 1].t = {0.0f, 1.0f};

            pbuf [i + 2].v.p = base + (GRASS_WIDTH / 2) * horizontal;
            pbuf [i + 2].v.n = normal;
            pbuf [i + 2].t = {1.0f, 0.0f};

            pbuf [i + 3] = pbuf [i + 1];

            pbuf [i + 4].v.p = pbuf [i + 2].v.p + GRASS_HEIGHT * vertical;
            pbuf [i + 4].v.n = normal;
            pbuf [i + 4].t = {1.0f, 1.0f};

            pbuf [i + 5] = pbuf [i + 2];

            // Register these so that wind can be calculated
            for (k=0; k < 6; k++)
                (*p_grass_neutral_positions) [i + k] = pbuf [i + k].v.p;

            i += 6;
        }
    }

    pBuffer->n_vertices = i;

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
void UpdateGrassPolygons (VertexBuffer *pBuffer, const vec3 *grass_neutral_positions, const vec3 &wind)
{
    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);

    GrassVertex *pbuf = (GrassVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        fprintf (stderr, "WARNING, failed to obtain buffer pointer during update!\n");
    }


    // Every second, fourth and fifth vertex is a grass blade's top
    for (size_t i = 0; i < pBuffer->n_vertices; i += 6)
    {
        pbuf [i + 1].v.p = grass_neutral_positions [i + 1] + wind;
        pbuf [i + 3].v.p = grass_neutral_positions [i + 3] + wind;
        pbuf [i + 4].v.p = grass_neutral_positions [i + 4] + wind;
    }

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
bool FillGroundVertexBuffer (VertexBuffer *pBuffer, const GroundVertex grid [][PER_CHUNK_SIZE + 1])
{
    size_t i = 0;

    pBuffer->n_vertices = 6 * PER_CHUNK_SIZE * PER_CHUNK_SIZE;

    glBindBuffer (GL_ARRAY_BUFFER, pBuffer->handle);
    glBufferData (GL_ARRAY_BUFFER, sizeof (GroundVertex) * pBuffer->n_vertices, NULL, GL_STATIC_DRAW);

    GroundVertex *pbuf = (GroundVertex *)glMapBuffer (GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!pbuf)
    {
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        SetError ("failed to obtain buffer pointer");
        return false;
    }

    // Two triangles for every grid square:
    for (int x = 0; x < PER_CHUNK_SIZE; x++)
    {
        for (int z = 0; z < PER_CHUNK_SIZE; z++)
        {
            pbuf [i] = grid [x][z];
            pbuf [i + 1] = grid [x + 1][z];
            pbuf [i + 2] = grid [x + 1][z + 1];
            pbuf [i + 3] = grid [x][z];
            pbuf [i + 4] = grid [x + 1][z + 1];
            pbuf [i + 5] = grid [x][z + 1];
            i += 6;
        }
    }

    glUnmapBuffer (GL_ARRAY_BUFFER);
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return true;
}
bool GrassChunk::Generate (const TerraGen <TerrainType> *pTerraGen)
{
    float x1, z1, x2, z2;
    GetPositionRange (x1, z1, x2, z2);

    int ix, iz;
    for (ix = 0; ix <= PER_CHUNK_SIZE; ix++)
    {
        for (iz = 0; iz <= PER_CHUNK_SIZE; iz++)
        {
            vec2 a2 (x1 + (x2 - x1) * float (ix) / PER_CHUNK_SIZE,
                     z1 + (z2 - z1) * float (iz) / PER_CHUNK_SIZE),
                 b2 (x1 + (x2 - x1) * float (ix + 1) / PER_CHUNK_SIZE,
                     z1 + (z2 - z1) * float (iz) / PER_CHUNK_SIZE),
                 c2 (x1 + (x2 - x1) * float (ix) / PER_CHUNK_SIZE,
                     z1 + (z2 - z1) * float (iz + 1) / PER_CHUNK_SIZE);
            vec3 a3 (a2.x, pTerraGen->InterpolateAt (a2).height, a2.z),
                 b3 (b2.x, pTerraGen->InterpolateAt (b2).height, b2.z),
                 c3 (c2.x, pTerraGen->InterpolateAt (c2).height, c2.z);

            grid [ix][iz].v.p = a3;
            grid [ix][iz].v.n = Cross (c3 - a3, b3 - a3).Unit();
            grid [ix][iz].t.u = float (ix) / PER_CHUNK_SIZE;
            grid [ix][iz].t.v = float (iz) / PER_CHUNK_SIZE;

            if (ix > 0 && iz > 0)
            {
                collTriangles.push_back (Triangle (grid [ix - 1][iz].v.p,
                                                   grid [ix][iz - 1].v.p,
                                                   grid [ix - 1][iz - 1].v.p));
                collTriangles.push_back (Triangle (grid [ix - 1][iz].v.p,
                                                   grid [ix][iz].v.p,
                                                   grid [ix][iz - 1].v.p));
            }
        }
    }
    if (!FillGroundVertexBuffer (&vbo_ground, grid))
    {
        SetError ("error filling vertex buffer for ground: %s", GetError());
        return false;
    }
    if (!FillGrassBladeVertexBuffer (&vbo_grass, collTriangles, &grass_neutral_positions))
    {
        SetError ("error filling vertex buffer for grass: %s", GetError());
        return false;
    }

    return true;
}
void GrassChunk::RenderGroundVertices ()
{
    glBindBuffer (GL_ARRAY_BUFFER, vbo_ground.handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableVertexAttribArray (VERTEX_INDEX);
    glVertexAttribPointer (VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), 0);
    glVertexPointer (3, GL_FLOAT, sizeof (GroundVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glEnableVertexAttribArray (NORMAL_INDEX);
    glVertexAttribPointer (NORMAL_INDEX, 3, GL_FLOAT, GL_TRUE, sizeof (GrassVertex), (const GLvoid *)(sizeof (vec3)));
    glNormalPointer (GL_FLOAT, sizeof (GroundVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableVertexAttribArray (TEXCOORD_INDEX);
    glVertexAttribPointer (TEXCOORD_INDEX, 2, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), (const GLvoid *)(2 * sizeof (vec3)));
    glTexCoordPointer (2, GL_FLOAT, sizeof (GroundVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, vbo_ground.n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableVertexAttribArray (VERTEX_INDEX);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableVertexAttribArray (NORMAL_INDEX);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableVertexAttribArray (TEXCOORD_INDEX);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
void GrassChunk::RenderGrassBladeVertices (const vec3 &wind)
{
    UpdateGrassPolygons (&vbo_grass, grass_neutral_positions, wind);

    glBindBuffer (GL_ARRAY_BUFFER, vbo_grass.handle);

    // Match the vertex format: position [3], normal [3], texcoord [2]
    glEnableClientState (GL_VERTEX_ARRAY);
    glEnableVertexAttribArray (VERTEX_INDEX);
    glVertexAttribPointer (VERTEX_INDEX, 3, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), 0);
    glVertexPointer (3, GL_FLOAT, sizeof (GrassVertex), 0);

    glEnableClientState (GL_NORMAL_ARRAY);
    glEnableVertexAttribArray (NORMAL_INDEX);
    glVertexAttribPointer (NORMAL_INDEX, 3, GL_FLOAT, GL_TRUE, sizeof (GrassVertex), (const GLvoid *)(sizeof (vec3)));
    glNormalPointer (GL_FLOAT, sizeof (GrassVertex), (const GLvoid *)(sizeof (vec3)));

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableVertexAttribArray (TEXCOORD_INDEX);
    glVertexAttribPointer (TEXCOORD_INDEX, 2, GL_FLOAT, GL_FALSE, sizeof (GrassVertex), (const GLvoid *)(2 * sizeof (vec3)));
    glTexCoordPointer (2, GL_FLOAT, sizeof (GrassVertex), (const GLvoid *)(2 * sizeof (vec3)));

    glDrawArrays (GL_TRIANGLES, 0, vbo_grass.n_vertices);

    glDisableClientState (GL_VERTEX_ARRAY);
    glDisableVertexAttribArray (VERTEX_INDEX);
    glDisableClientState (GL_NORMAL_ARRAY);
    glDisableVertexAttribArray (NORMAL_INDEX);
    glDisableClientState (GL_TEXTURE_COORD_ARRAY);
    glDisableVertexAttribArray (TEXCOORD_INDEX);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
}
