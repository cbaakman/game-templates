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


#ifndef MESH_H
#define MESH_H

#include <libxml/tree.h>
#include "../matrix.h"

#include <string>
#include <list>
#include <map>

#include "app.h"

struct MeshVertex {

    vec3 p, n;
};
typedef const MeshVertex *PMeshVertex;

struct MeshBone {

    std::string parent_id;
    MeshBone *parent;

    vec3 posHead, posTail;
    float weight;

    std::list<std::string> vertex_ids;
};

// MeshBoneState is mainly for debugging.
struct MeshBoneState {

    vec3 posHead,
         posTail;
};

struct MeshBoneKey {

    Quaternion rot;
    vec3 loc;
};

struct MeshBoneLayer {

    std::string bone_id;
    const MeshBone *pBone;
    std::map <float, MeshBoneKey> keys;
};

struct MeshAnimation {

    int length;
    std::list <MeshBoneLayer> layers;
};

// MeshTexel represents a position of a texture
struct MeshTexel {

    GLfloat u, v;
};

typedef char *charp;

template <int N>
class MeshFace {

private:
    charp vertex_ids [N];
public:
    MeshTexel texels [N];

    MeshFace (const MeshFace &other) : MeshFace()
    {
        for(int i = 0; i < N; i++)
             SetVertexID(i, other.GetVertexID(i));
    }

    MeshFace ()
    {
        for(int i = 0; i < N; i++)
        {
            vertex_ids[i] = NULL;
        }
    }
    ~MeshFace ()
    {
        for(int i = 0; i < N; i++)
            delete [] vertex_ids[i];
    }

    void SetVertexID (const int i, const char *id)
    {
        delete [] vertex_ids[i];
        if (!id)
        {
            vertex_ids[i] = NULL;
            return;
        }

        vertex_ids[i] = new char [strlen(id) + 1];
        strcpy (vertex_ids [i], id);
    }
    void SetVertexID (const int i, const std::string &id)
    {
        SetVertexID(i, id.c_str());
    }
    const char *GetVertexID (const int i) const
    {
        return vertex_ids [i];
    }
};

typedef MeshFace<3> MeshTriangle;
typedef MeshFace<4> MeshQuad;

typedef const MeshQuad *PMeshQuad;
typedef const MeshTriangle *PMeshTriangle;

struct MeshSubset {

    GLfloat diffuse[4], specular[4], emission[4];

    std::string id;

    std::list<PMeshQuad> quads;
    std::list<PMeshTriangle> triangles;
};

// MeshData represents whats in the mesh file, can be rendered directly using RenderUnAnimatedSubset.
struct MeshData {

    std::map<std::string, MeshBone> bones;
    std::map<std::string, MeshVertex> vertices;
    std::map<std::string, MeshTriangle> triangles;
    std::map<std::string, MeshQuad> quads;
    std::map<std::string, MeshSubset> subsets;
    std::map<std::string, MeshAnimation> animations;
};

void RenderUnAnimatedSubset (const MeshData *, const std::string &subset_id);

// ToTriangles is useful for collision detection
void ToTriangles (const MeshData *, Triangle **triangles, size_t *n_triangles);

bool ParseMesh (const xmlDocPtr, MeshData *pData);

// MeshObject uses MeshData, but has an animation state that can be changed.
class MeshObject
{
private:
    const MeshData *pMeshData;

    std::map<std::string, MeshVertex> vertexStates;

    void InitStates();
    void ApplyBoneTransformations(const std::map<std::string, matrix4> transforms);
public:
    std::map<std::string, MeshBoneState> boneStates;
    void RenderSubset (const std::string &subset_id);
    void RenderBones ();
    void RenderNormals ();

    void SetAnimationState(const char *animation_id, float frame, bool loop = true);
    void SetBoneAngle(const char *bone_id, const vec3 *axis, const float angle);
    void SetBoneLoc(const char *bone_id, const vec3 *loc);

    const std::map<std::string, MeshVertex> &GetVertices() const { return vertexStates; }
    const MeshData *GetMeshData() const { return pMeshData; }

    MeshObject(const MeshData *data);
    ~MeshObject();
};



#endif // MESH_H
