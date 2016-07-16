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

#ifndef CHUNK_H
#define CHUNK_H

#include "../vec.h"

#include <string>
#include <list>
#include <tuple>
#include "terragen.h"
#include "buffer.h"
#include "mesh.h"

/*
    Set these 2 parameters higher to get better looking grass.
    Set them lower for faster rendering.
 */
#define GRASS_PER_M2 200
#define N_GRASS_LAYERS 20

#define MAX_GRASS_ANGLE 0.785
#define GRASS_WIDTH 0.2f
#define GRASS_HEIGHT 0.6f

struct GroundVertex
{
    MeshVertex v;
    MeshTexel t;
};
struct GrassVertex
{
    MeshVertex v;
    MeshTexel t;
};
// index values: where to find data fields in the vertices
#define VERTEX_INDEX 0
#define NORMAL_INDEX 1
#define TEXCOORD_INDEX 2

struct TerrainType
{
    float height;
};
inline TerrainType operator+ (const TerrainType &t1, const TerrainType &t2)
{
    TerrainType t;
    t.height = t1.height + t2.height;
    return t;
}
inline TerrainType operator- (const TerrainType &t1, const TerrainType &t2)
{
    TerrainType t;
    t.height = t1.height - t2.height;
    return t;
}
inline TerrainType operator* (const float f, const TerrainType &t1)
{
    TerrainType t;
    t.height = t1.height * f;
    return t;
}

typedef signed long long ChunkNumber;
typedef std::tuple <ChunkNumber, ChunkNumber> ChunkID;

#define PER_CHUNK_SIZE 16

class GrassChunk
{
private:
    ChunkNumber numX, numZ;

    GroundVertex grid [PER_CHUNK_SIZE + 1][PER_CHUNK_SIZE + 1];
    std::list <Triangle> collTriangles;

    VertexBuffer vbo_ground,
                 vbo_grass;
    vec3 *grass_neutral_positions;
public:
    GrassChunk (ChunkNumber x, ChunkNumber z);
    ~GrassChunk ();

    void GetChunkNumbers (ChunkNumber &x, ChunkNumber &z) const;
    void GetPositionRange (float &x1, float &z1,
                           float &x2, float &z2) const;

    const std::list <Triangle> &GetCollisionTriangles (void) const;

    bool Generate (const TerraGen <TerrainType> *);

    void RenderGroundVertices (void);
    void RenderGrassBladeVertices (const vec3 &wind);
};

std::tuple <ChunkNumber, ChunkNumber>
    ChunkNumbersFor (const float x, const float z);

#endif // CHUNK_H
