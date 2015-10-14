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

/*
    Describe's a bone in a mesh's armature. (if any)
 */
struct MeshBone {

    std::string parent_id; // can be empty, if no parent

    MeshBone *parent; // can be NULL if no parent

    // Two vectors to represent the rest position of a bone:
    vec3 posHead, posTail;

    // Haven't used this much:
    float weight;

    // Vertices that this bone pulls at.
    std::list<std::string> vertex_ids;
};

/*
    MeshBoneState was added mainly for debugging.
    One can register the current position and rotation
    of a bone in it.
 */
struct MeshBoneState {

    vec3 posHead,
         posTail;
};

/*
    MeshBoneKey is a keyframe for one bone in an animation.
 */
struct MeshBoneKey {

    Quaternion rot;
    vec3 loc;
};

/*
    MeshBoneLayer describes the motion of a bone over time
    in an animation.
 */
struct MeshBoneLayer {

    // Only one bone is modified by a layer object:
    std::string bone_id;
    const MeshBone *pBone;

    // the map keys are frame numbers, values are keyframes
    std::map <float, MeshBoneKey> keys;
};

/*
    Only one animation can be active at the time per mesh.
    The animation hass all the information needed to calculate
    the positions of all bones in a mesh at a given time.
 */
struct MeshAnimation {

    int length; // max length ov animation in time.

    std::list <MeshBoneLayer> layers;
};

// MeshTexel represents a position on a texture
struct MeshTexel {

    GLfloat u, v;
};

typedef char *charp;

/*
    A MeshFace can be a triangle or quad, depending
    on the number of vertices
 */
template <int N>
class MeshFace {

private:

    // For some reason, std::string here wouldn't compile
    charp vertex_ids [N];

public:
    MeshTexel texels [N];
    bool smooth;

    MeshFace (const MeshFace &other) : MeshFace()
    {
        for(int i = 0; i < N; i++)
        {
             SetVertexID (i, other.GetVertexID(i));
             texels [i] = other.texels [i];
        }
        smooth = other.smooth;
    }

    MeshFace ()
    {
        for(int i = 0; i < N; i++)
        {
            vertex_ids[i] = NULL;
            texels [i].u = 0.0f;
            texels [i].v = 0.0f;
        }
        smooth = true;
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
        SetVertexID (i, id.c_str());
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

/*
    A subset is a set of faces with color settings and an id attached to it.
    A Mesh can consist of more than one subset.
    Texture is not included.
 */
struct MeshSubset {

    GLfloat diffuse[4], specular[4], emission[4];

    std::string id;

    std::list<PMeshQuad> quads;
    std::list<PMeshTriangle> triangles;
};

// MeshData represents whats in the mesh file, can be rendered directly using RenderUnAnimatedSubset.
struct MeshData {

    // These maps have the xml tags' id values as keys
    std::map<std::string, MeshBone> bones;
    std::map<std::string, MeshVertex> vertices;
    std::map<std::string, MeshTriangle> triangles;
    std::map<std::string, MeshQuad> quads;
    std::map<std::string, MeshSubset> subsets;
    std::map<std::string, MeshAnimation> animations;
};

/**
 * Arguments of MeshFaceFunc are:
 * 1. a user provided uniform object
 * 2. the number of vertices in the face
 * 3. the array of vertex pointers
 * 4. the array of texels
 */
typedef void (*MeshFaceFunc) (void *, const int, const MeshVertex **, const MeshTexel *);

void ThroughUnAnimatedSubsetFaces (const MeshData *, const std::string &subset_id, MeshFaceFunc func, void *pObj=NULL);
void RenderUnAnimatedSubset (const MeshData *, const std::string &subset_id);

// ToTriangles is useful for collision detection
void ToTriangles (const MeshData *, Triangle **triangles, size_t *n_triangles);

/**
 * Converts a given xml document to a mesh.
 * :returns: true if all data was found and meshdata is valid, false otherwise.
 */
bool ParseMesh (const xmlDocPtr, MeshData *pData);

// MeshObject uses MeshData, but has an animation state that can be changed.
class MeshObject
{

private:
    const MeshData *pMeshData;

    // current animation transformations of vertices and bones:
    std::map <std::string, MeshVertex> vertexStates;
    std::map <std::string, MeshBoneState> boneStates;

    void InitStates (); // Resets all vertices and bones to default state

    // One matrix per bone id. Matrices are applied to bone states and vertex states.
    void ApplyBoneTransformations (const std::map <std::string, matrix4> transforms);

public:

    // Executes func for every face in the subset
    void ThroughSubsetFaces (const std::string &subset_id, MeshFaceFunc func, void *pObj=NULL);

    // Render subset in OpenGL, could precede this with desired GL settings
    void RenderSubset (const std::string &subset_id);

    // These render functions can be usefull for debugging
    void RenderBones ();
    void RenderNormals ();

    /**
     * Puts mesh in desired animation frame.
     * :param animation_id: name of animation in xml file, or NULL for initial state.
     */
    void SetAnimationState (const char *animation_id, float frame, bool loop = true);

    // Functions to manipulate bones directly:
    void SetBoneAngle(const char *bone_id, const vec3 *axis, const float angle);
    void SetBoneLoc(const char *bone_id, const vec3 *loc);

    const std::map<std::string, MeshVertex> &GetVertices() const { return vertexStates; }
    const MeshData *GetMeshData() const { return pMeshData; }

    MeshObject(const MeshData *data);
    ~MeshObject();
};



#endif // MESH_H
