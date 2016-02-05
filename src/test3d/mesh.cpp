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


#include "mesh.h"
#include "../util.h"
#include "../str.h"
#include "../err.h"

const MeshVertex *MeshState::GetVertex (const std::string &id) const
{
    return &vertexStates.at (id);
}
const MeshBoneState *MeshState::GetBoneState (const std::string &id) const
{
    return &boneStates.at (id);
}
MeshState::MeshState(const MeshData *data) : pMeshData(data)
{
    InitStates();
}
void MeshState::InitStates()
{
    // Copy the mesh data's rest states to this object's vertex and bone states:

    for (std::map<std::string, MeshVertex>::const_iterator it = pMeshData->vertices.begin(); it != pMeshData->vertices.end(); it++)
    {
        const std::string id = it->first;

        vertexStates[id] = MeshVertex();
        vertexStates[id].p = pMeshData->vertices.at(id).p;
        vertexStates[id].n = pMeshData->vertices.at(id).n;
    }
    for (std::map<std::string, MeshBone>::const_iterator it = pMeshData->bones.begin(); it != pMeshData->bones.end(); it++)
    {
        const std::string id = it->first;

        boneStates[id] = MeshBoneState();
        boneStates[id].posHead = pMeshData->bones.at(id).posHead;
        boneStates[id].posTail = pMeshData->bones.at(id).posTail;
    }
}
MeshState::~MeshState()
{
}
void ToTriangles (const MeshData *pMeshData, Triangle **pT, size_t *pN)
{
    size_t n_triangles = pMeshData->quads.size() * 2 + pMeshData->triangles.size();
    Triangle *triangles = new Triangle [n_triangles];

    size_t i = 0;
    for (std::map<std::string, MeshQuad>::const_iterator it = pMeshData->quads.begin(); it != pMeshData->quads.end(); it++)
    {
        // Convert a quad to two triangles:

        const PMeshQuad pQuad = &(it -> second);

        triangles [i + 1].p[0] = triangles [i].p[0] = pMeshData->vertices.at (pQuad->GetVertexID (0)).p;
                                 triangles [i].p[1] = pMeshData->vertices.at (pQuad->GetVertexID (1)).p;
        triangles [i + 1].p[1] = triangles [i].p[2] = pMeshData->vertices.at (pQuad->GetVertexID (2)).p;
        triangles [i + 1].p[2] = pMeshData->vertices.at (pQuad->GetVertexID (3)).p;

        i += 2;
    }
    for (std::map<std::string, MeshTriangle>::const_iterator it = pMeshData->triangles.begin(); it != pMeshData->triangles.end(); it++)
    {
        const PMeshTriangle pTriangle = &(it -> second);

        triangles [i].p[0] = pMeshData->vertices.at (pTriangle->GetVertexID (0)).p;
        triangles [i].p[1] = pMeshData->vertices.at (pTriangle->GetVertexID (1)).p;
        triangles [i].p[2] = pMeshData->vertices.at (pTriangle->GetVertexID (2)).p;

        i ++;
    }

    *pN = n_triangles;
    *pT = triangles;
}
void ThroughSubsetFaces (const MeshData *pMeshData, const std::string &id, MeshFaceFunc func)
{
    if (pMeshData->subsets.find (id) == pMeshData->subsets.end ())
    {
        SetError ("cannot iterate through %s, no such subset", id.c_str());
        return;
    }
    const MeshSubset *pSubset = &pMeshData->subsets.at(id);

    const MeshVertex *vs [4];

    for (auto it = pSubset->quads.begin(); it != pSubset->quads.end(); it++)
    {
        PMeshQuad pQuad = *it;
        for(size_t j = 0; j < 4; j++)
        {
            vs [j] = &pMeshData->vertices.at (pQuad->GetVertexID(j));
        }

        func (4, vs, pQuad->texels);
    }
    for (auto it = pSubset->triangles.begin(); it != pSubset->triangles.end(); it++)
    {
        PMeshTriangle pTriangle = *it;
        for(size_t j = 0; j < 3; j++)
        {
            vs [j] = &pMeshData->vertices.at (pTriangle->GetVertexID(j));
        }

        func (3, vs, pTriangle->texels);
    }
}
void ThrougFaces (const MeshData *pMeshData, MeshFaceFunc func)
{
    const MeshVertex *vs [4];

    for (auto it = pMeshData->quads.begin(); it != pMeshData->quads.end(); it++)
    {
        PMeshQuad pQuad = &it->second;
        for(size_t j = 0; j < 4; j++)
        {
            vs [j] = &pMeshData->vertices.at (pQuad->GetVertexID(j));
        }

        func (4, vs, pQuad->texels);
    }
    for (auto it = pMeshData->triangles.begin(); it != pMeshData->triangles.end(); it++)
    {
        PMeshTriangle pTriangle = &it->second;
        for(size_t j = 0; j < 3; j++)
        {
            vs [j] = &pMeshData->vertices.at (pTriangle->GetVertexID(j));
        }

        func (3, vs, pTriangle->texels);
    }
}
void MeshState::ThroughSubsetFaces (const std::string &id, MeshFaceFunc func) const
{
    if(pMeshData->subsets.find(id) == pMeshData->subsets.end())
    {
        SetError ("cannot iterate through %s, no such subset", id.c_str());
        return;
    }
    const MeshSubset *pSubset = &pMeshData->subsets.at(id);

    const MeshVertex *vs [4];

    for (auto it = pSubset->quads.begin(); it != pSubset->quads.end(); it++)
    {
        PMeshQuad pQuad = *it;
        for(size_t j = 0; j < 4; j++)
        {
            vs [j] = &vertexStates.at (pQuad->GetVertexID(j));
        }

        func (4, vs, pQuad->texels);
    }
    for (auto it = pSubset->triangles.begin(); it != pSubset->triangles.end(); it++)
    {
        PMeshTriangle pTriangle = *it;
        for(size_t j = 0; j < 3; j++)
        {
            vs [j] = &vertexStates.at (pTriangle->GetVertexID(j));
        }

        func (3, vs, pTriangle->texels);
    }
}
void MeshState::ThroughFaces (MeshFaceFunc func) const
{
    const MeshVertex *vs [4];

    for (auto it = pMeshData->quads.begin(); it != pMeshData->quads.end(); it++)
    {
        PMeshQuad pQuad = &it->second;
        for(size_t j = 0; j < 4; j++)
        {
            vs [j] = &vertexStates.at (pQuad->GetVertexID(j));
        }

        func (4, vs, pQuad->texels);
    }
    for (auto it = pMeshData->triangles.begin(); it != pMeshData->triangles.end(); it++)
    {
        PMeshTriangle pTriangle = &it->second;
        for(size_t j = 0; j < 3; j++)
        {
            vs [j] = &vertexStates.at (pTriangle->GetVertexID(j));
        }

        func (3, vs, pTriangle->texels);
    }
}

/**
 * Gets the r,g,b and a attributes from the given tag.
 * :returns: true if all float values were there, false otherwise.
 */
bool ParseRGBA (const xmlNodePtr pTag, GLfloat *c)
{
    xmlChar *pR = xmlGetProp(pTag, (const xmlChar *)"r"),
            *pG = xmlGetProp(pTag, (const xmlChar *)"g"),
            *pB = xmlGetProp(pTag, (const xmlChar *)"b"),
            *pA = xmlGetProp(pTag, (const xmlChar *)"a");

    bool bres;
    if (pR && pG && pB && pA)
    {
        ParseFloat ((const char *)pR, &c[0]);
        ParseFloat ((const char *)pG, &c[1]);
        ParseFloat ((const char *)pB, &c[2]);
        ParseFloat ((const char *)pA, &c[3]);
        bres = true;
    }
    else
    {
        bres = false;
    }

    xmlFree (pR);
    xmlFree (pG);
    xmlFree (pB);
    xmlFree (pA);

    return bres;
}
/**
 * Gets the x, y and z attributes from the given tag. Optionally with a previx in their identifiers.
 * :returns: true if all float values were there, false otherwise.
 */
bool ParseXYZ (const xmlNodePtr pTag, vec3 *pVec, const char *prefix = NULL)
{
    std::string id_x = "x", id_y = "y", id_z = "z";
    if(prefix)
    {
        id_x = std::string(prefix) + id_x;
        id_y = std::string(prefix) + id_y;
        id_z = std::string(prefix) + id_z;
    }

    xmlChar *pX = xmlGetProp(pTag, (const xmlChar *)id_x.c_str()),
            *pY = xmlGetProp(pTag, (const xmlChar *)id_y.c_str()),
            *pZ = xmlGetProp(pTag, (const xmlChar *)id_z.c_str());


    bool bres;
    if (pX && pY && pZ)
    {
        ParseFloat ((const char *)pX, &pVec->x);
        ParseFloat ((const char *)pY, &pVec->y);
        ParseFloat ((const char *)pZ, &pVec->z);
        bres = true;
    }
    else
    {
        bres = false;
    }

    xmlFree (pX);
    xmlFree (pY);
    xmlFree (pZ);

    return bres;
}
/**
 * Gets one float attribute from the given tag with given id (key)
 * :returns: true if float was successfully parsed, false otherwise.
 */
bool ParseFloatAttrib (const xmlNodePtr pTag, const xmlChar *key, float *f)
{
    xmlChar *pF = xmlGetProp(pTag, key);
    if(!pF)
    {
        return false;
    }

    if (!ParseFloat ((const char *)pF, f))
    {
        return false;
    }

    xmlFree (pF);

    return true;
}
/**
 * Gets one int attribute from the given tag with given id (key)
 * :returns: true if int was successfully parsed, false otherwise.
 */
bool ParseIntAttrib (const xmlNodePtr pTag, const xmlChar *key, int *i)
{
    xmlChar *pI = xmlGetProp(pTag, key);
    if(!pI)
    {
        return false;
    }

    *i = atoi ((const char *)pI);

    xmlFree (pI);

    return true;
}
/**
 * Gets one string attribute from the given tag with given id (key)
 * :returns: true if the attribute was found, false otherwise.
 */
bool ParseStringAttrib (const xmlNodePtr pTag, const xmlChar *key, std::string &s)
{
    xmlChar *pS = xmlGetProp(pTag, key);
    if (!pS)
    {
        return false;
    }

    s.assign ((const char *)pS);
    xmlFree (pS);

    return true;
}

/**
 * Converts an xml vertex tag to a vertex and adds it to given map with proper id.
 */
bool ParseVertex (const xmlNodePtr pTag, std::map<std::string, MeshVertex> &vertices)
{
    std::string id;
    if (!ParseStringAttrib(pTag, (const xmlChar *)"id", id))
    {
        SetError ("no id found on %s", pTag->name);
        return false;
    }

    vertices[id] = MeshVertex();

    xmlNodePtr pChild = pTag->children;
    while (pChild) {

        if (StrCaseCompare((const char *)pChild->name, "pos") == 0)
        {
            if (!ParseXYZ(pChild, &vertices[id].p))
            {
                SetError ("no x, y, z on pos");
                vertices.erase(id);

                return false;
            }
        }
        if (StrCaseCompare((const char *)pChild->name, "norm") == 0)
        {
            if (!ParseXYZ(pChild, &vertices[id].n))
            {
                SetError ("no x, y, z on norm");
                vertices.erase(id);

                return false;
            }
        }
        pChild = pChild->next;
    }

    return true;
}

/**
 * Converts given tag to face object and adds is to given map with proper id.
 */
template <int N>
bool ParseFace (const xmlNodePtr pTag,
                const std::map <std::string, MeshVertex> &vertices,
                std::map <std::string, MeshFace<N> > &faces)
{
    std::string id;
    if (!ParseStringAttrib(pTag, (const xmlChar *)"id", id))
    {
        SetError ("no id found on %s", pTag->name);
        return false;
    }

    faces[id] = MeshFace<N>();

    int smooth;
    if (ParseIntAttrib(pTag, (const xmlChar *)"smooth", &smooth))
    {
        faces[id].smooth = (smooth != 0);
    }

    size_t ncorner = 0;
    xmlNodePtr pChild = pTag->children;
    while (pChild) {

        if (StrCaseCompare((const char *)pChild->name, "corner") == 0)
        {
            if(ncorner >= N)
            {
                SetError ("too many corners on %s %s, %d expected", pTag->name, id.c_str(), N);
                faces.erase(id);
                return false;
            }

            std::string vertex_id;

            if (!ParseStringAttrib(pChild, (const xmlChar *)"vertex_id", vertex_id) ||
                !ParseFloatAttrib(pChild, (const xmlChar *)"tex_u", &faces[id].texels[ncorner].u) ||
                !ParseFloatAttrib(pChild, (const xmlChar *)"tex_v", &faces[id].texels[ncorner].v))
            {
                faces.erase(id);
                SetError ("face corner incomplete, should have attributes: vertex_id and numbers tex_u and tex_v\n");
                return false;
            }

            if(vertices.find(vertex_id) == vertices.end())
            {
                faces.erase(id);
                SetError ("no such vertex: %s, referred to by %d %d", vertex_id.c_str(), pTag->name, id.c_str());
                return false;
            }

            faces[id].SetVertexID(ncorner, vertex_id);
            ncorner++;
        }

        pChild = pChild->next;
    }

    if (ncorner < N)
    {
        SetError ("not enough corners on %s %s, %d expected", pTag->name, id.c_str(), N);
        faces.erase(id);
        return false;
    }

    return true;
}
/**
 * Converts given xml tag to bone object and adds it to given map with proper id.
 * Also checks the ids of the vertices are in the given vertex map.
 */
bool ParseBone(const xmlNodePtr pTag,
               const std::map<std::string, MeshVertex> &vertices,
               std::map<std::string, MeshBone> &bones)
{
    std::string id,
                vert_id;

    if(!ParseStringAttrib (pTag, (const xmlChar *)"id", id))
    {
        SetError ("No id found on %s", pTag->name);
        return false;
    }

    bones [id] = MeshBone();

    if (!ParseStringAttrib (pTag, (const xmlChar *)"parent_id", bones [id].parent_id))
    {
        bones [id].parent_id = "";
    }

    // Get head pos and tail pos.
    if (!ParseXYZ (pTag, &bones[id].posHead))
    {
        SetError ("No x, y, z on %s %s", pTag->name, id.c_str ());
        return false;
    }

    if (!ParseXYZ (pTag, &bones[id].posTail, "tail_"))
        bones [id].posTail = bones[id].posHead;

    if (!ParseFloatAttrib (pTag, (const xmlChar *)"weight", &bones[id].weight))
        bones [id].weight = 1.0f;

    // Find vertex tags with ids inside the bone tag:
    xmlNodePtr pChild = pTag->children,
               pVert;
    while (pChild)
    {
        if (StrCaseCompare((const char *) pChild->name, "vertices") == 0)
        {
            pVert = pChild->children;
            while (pVert)
            {
                if (StrCaseCompare((const char *) pVert->name, "vertex") == 0)
                {
                    // Check that the id is present and that it's in the vertices map also:

                    if (!ParseStringAttrib (pVert, (const xmlChar *)"id", vert_id))
                    {
                        SetError ("no id found on vertex in %s %s\n", pTag->name, id.c_str());
                        return false;
                    }

                    if (vertices.find (vert_id) == vertices.end())
                    {
                        SetError ("no such vertex: %s, referred to by %s %s\n", vert_id.c_str(), pTag->name, id.c_str());
                        return false;
                    }

                    bones[id].vertex_ids.push_back(vert_id);
                }

                pVert = pVert->next;
            }
        }
        pChild = pChild->next;
    }

    // at this point all went well
    return true;
}

/**
 * Converts given xml tag to subset and adds it to the given subset map with proper id.
 * Also check that the referenced triangles and quads already exist.
 */
bool ParseSubset(const xmlNodePtr pTag,
                 const std::map<std::string, MeshQuad> &quads,
                 const std::map<std::string, MeshTriangle> &triangles,
                 std::map<std::string, MeshSubset> &subsets)
{
    xmlNodePtr pChild, pFace;
    std::string id, face_id;


    if (!ParseStringAttrib(pTag, (const xmlChar *)"id", id))
    {
        SetError ("no id found on %s", pTag->name);
        return false;
    }

    // Add a new subset to the map with defaults:
    MeshSubset subset;
    subsets[id] = subset;
    for(int i=0; i<4; i++)
    {
        subsets[id].diffuse[i] = 1.0;
        subsets[id].specular[i] = 1.0;
        subsets[id].emission[i] = 1.0;
    }

    pChild = pTag->children;
    while (pChild) {

        // Look for diffuse, specular, emission colors and a list of faces inside the subset tag:
        if (StrCaseCompare((const char *) pChild->name, "diffuse") == 0) {

            if(!ParseRGBA (pChild, subsets[id].diffuse))
            {
                SetError ("error getting %s RGBA for %s %s", pChild->name, id.c_str(), pTag->name);
                return false;
            }
        }
        else if (StrCaseCompare((const char *) pChild->name, "specular") == 0) {

            if(!ParseRGBA(pChild, subsets[id].specular))
            {
                SetError ("error getting %s RGBA for %s %s", pChild->name, id.c_str(), pTag->name);
                return false;
            }
        }
        else if (StrCaseCompare((const char *) pChild->name, "emission") == 0) {

            if(!ParseRGBA(pChild, subsets[id].emission))
            {
                SetError ("error getting %s RGBA for %s %s", pChild->name, id.c_str(), pTag->name);
                return false;
            }
        }
        else if (StrCaseCompare((const char *) pChild->name, "faces") == 0) {
            pFace = pChild->children;
            while (pFace)
            {
                // Add face as either quad or triangle.

                if (StrCaseCompare((const char *) pFace->name, "quad") == 0)
                {
                    if (!ParseStringAttrib(pFace, (const xmlChar *)"id", face_id))
                    {
                        SetError ("no id found for %s in %s %s", pFace->name, pTag->name, id.c_str());
                        return false;
                    }

                    if (quads.find(face_id) == quads.end())
                    {
                        SetError ("no such quad: %s", pFace->name, face_id.c_str());
                        return false;
                    }
                    subsets[id].quads.push_back(&quads.at(face_id));
                }
                else if(StrCaseCompare((const char *) pFace->name, "triangle") == 0)
                {
                    if (!ParseStringAttrib(pFace, (const xmlChar *)"id", face_id))
                    {
                        SetError ("no id found for %s in %s %s", pFace->name, pTag->name, id.c_str());
                        return false;
                    }

                    if (triangles.find(face_id) == triangles.end())
                    {
                        SetError ("no such triangle: %s", pFace->name, face_id.c_str());
                        return false;
                    }
                    subsets[id].triangles.push_back(&triangles.at(face_id));
                }

                pFace = pFace->next;
            }
        }

        pChild = pChild->next;
    }
    return true;
}

/**
 * Converts given xml tag to animation object and adds it to the given
 * animations map with proper id.
 * Also verifies that the bones in the animation exist.
 */
bool ParseAnimation(const xmlNodePtr pTag,
                    const std::map<std::string, MeshBone> &bones,
                    std::map<std::string, MeshAnimation> &animations)
{
    xmlNodePtr pLayer, pKey;

    // the id is actually called 'name' in animation tags.
    std::string id, bone_id;
    if (!ParseStringAttrib(pTag, (const xmlChar *)"name", id))
    {
        SetError ("%s without a name", pTag->name);
        return false;
    }

    // Create a new animation object:
    animations[id] = MeshAnimation();

    // An animation must have a length:
    if (!ParseIntAttrib(pTag, (const xmlChar *)"length", &animations[id].length))
    {
        SetError ("no length given for %s %s", pTag->name, id.c_str());
        return false;
    }

    pLayer = pTag->children;
    while (pLayer)
    {
        if(StrCaseCompare((const char *)pLayer->name, "layer") == 0)
        {
            // Check that the layer is associated with an existing bone:

            if (!ParseStringAttrib(pLayer, (const xmlChar *)"bone_id", bone_id))
            {
                SetError ("no id on %s in %s %s", pLayer->name, pTag->name, id.c_str());
                return false;
            }

            if(bones.find(bone_id) == bones.end())
            {
                SetError ("no such bone: %s", bone_id.c_str());
                return false;
            }

            // Add the layer to the animation and set its references to its bone.
            animations[id].layers.push_back (MeshBoneLayer());
            animations[id].layers.back ().pBone = &bones.at(bone_id);
            animations[id].layers.back ().bone_id = bone_id;

            pKey = pLayer->children;
            while (pKey)
            {
                if (StrCaseCompare ((const char *)pKey->name, "key") == 0)
                {
                    // Get key's frame number:
                    float frame;
                    if (!ParseFloatAttrib(pKey, (const xmlChar *)"frame", &frame))
                    {
                        SetError ("no frame number given for %s", pKey->name);
                        return false;
                    }

                    // Add key to layer at the correct frame number:
                    animations [id].layers.back().keys [frame] = MeshBoneKey();

                    // Parse the key's rotation and location state for the bone:
                    if (ParseFloatAttrib(pKey, (const xmlChar *)"rot_w", &animations [id].layers.back().keys[frame].rot.w))
                    {
                        if (!ParseFloatAttrib (pKey, (const xmlChar *)"rot_x", &animations [id].layers.back().keys [frame].rot.x) ||
                            !ParseFloatAttrib (pKey, (const xmlChar *)"rot_y", &animations [id].layers.back().keys [frame].rot.y) ||
                            !ParseFloatAttrib (pKey, (const xmlChar *)"rot_z", &animations [id].layers.back().keys [frame].rot.z))
                        {
                            SetError ("%s bone %s rotation incomplete for key at %f", id.c_str(), bone_id.c_str(), frame);
                            return false;
                        }
                    }
                    else
                        animations[id].layers.back ().keys [frame].rot = QUAT_ID;

                    if (!ParseXYZ (pKey, &animations [id].layers.back ().keys [frame].loc))
                        animations [id].layers.back ().keys [frame].loc = vec3 (0, 0, 0);
                }
                pKey = pKey->next;
            }

            // Check that the layer actually got any keys from this:
            if (animations[id].layers.back().keys.size () <= 0)
            {
                SetError ("layer %s with no keys in %s", bone_id.c_str(), id.c_str());
                return false;
            }
        }
        pLayer = pLayer->next;
    }
    return true;
}
bool ParseMesh(const xmlDocPtr pDoc, MeshData *pData)
{
    xmlNodePtr pChild, pRoot = xmlDocGetRootElement (pDoc), pList, pTag;
    if(!pRoot) {

        SetError ("no root element in mesh xml doc");
        return false;
    }

    if (StrCaseCompare((const char *) pRoot->name, "mesh") != 0) {

        SetError ("document of the wrong type, found \'%s\' instead of \'mesh\'", pRoot->name);
        return false;
    }

    // First, collect all the vertices:
    pChild = pRoot->children;
    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "vertices") == 0) {

            pTag = pChild->children;
            while (pTag) {

                if (StrCaseCompare((const char *) pTag->name, "vertex") == 0)
                    if (!ParseVertex(pTag, pData->vertices))
                    {
                        // 'ParseVertex' calls 'SetError'
                        return false;
                    }
                pTag = pTag->next;
            }
        }
        pChild = pChild->next;
    }

    // Next, collect all faces, that refer to vertices:
    pChild = pRoot->children;
    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "faces") == 0) {

            pTag = pChild->children;
            while (pTag) {

                if (StrCaseCompare((const char *) pTag->name, "quad") == 0)
                {
                    if (!ParseFace(pTag, pData->vertices, pData->quads))
                    {
                        // 'ParseFace' calls 'SetError'
                        return false;
                    }
                }
                else if(StrCaseCompare((const char *) pTag->name, "triangle") == 0)
                {
                    if (!ParseFace(pTag, pData->vertices, pData->triangles))
                    {
                        // 'ParseFace' calls 'SetError'
                        return false;
                    }
                }

                pTag = pTag->next;
            }
        }
        pChild = pChild->next;
    }

    // Next collect subsets, which refer to faces:
    pChild = pRoot->children;
    while (pChild) {

        if (StrCaseCompare((const char *) pChild->name, "subsets") == 0) {

            pTag = pChild->children;
            while (pTag) {

                if (StrCaseCompare((const char *) pTag->name, "subset") == 0)
                {
                    if (!ParseSubset(pTag, pData->quads, pData->triangles, pData->subsets))
                    {
                        return false;
                    }
                }
                pTag = pTag->next;
            }
        }
        pChild = pChild->next;
    }

    // Next, look for bones, which refer to vertices also:
    pChild = pRoot->children;
    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "armature") == 0)
        {
            pList = pChild->children;
            while (pList) {

                if (StrCaseCompare((const char *) pList->name, "bones") == 0)
                {
                    pTag = pList->children;
                    while (pTag) {

                        if(StrCaseCompare((const char *) pTag->name, "bone") == 0)
                        {
                            if (!ParseBone(pTag, pData->vertices, pData->bones))
                                return false;
                        }

                        pTag = pTag->next;
                    }
                }

                pList = pList->next;
            }
        }
        pChild = pChild->next;
    }

    // set the parents on the bones and check the vertex ids
    for (std::map<std::string, MeshBone>::iterator it = pData->bones.begin(); it != pData->bones.end(); it++)
    {
        std::string id = it->first;
        std::string parent_id = pData->bones[id].parent_id;
        if(parent_id.size() <= 0)
        {
            pData->bones[id].parent = NULL;
            continue;
        }

        for (std::list <std::string>::const_iterator it = pData->bones [id].vertex_ids.begin ();
             it != pData->bones [id].vertex_ids.end (); it++)
        {
            std::string vertex_id = *it;
            if (pData->vertices.find (vertex_id) == pData->vertices.end ())
            {
                SetError ("bone %s refers to nonexistent vertex %s", id.c_str(), vertex_id.c_str());
                return false;
            }
        }

        if (pData->bones.find(parent_id) == pData->bones.end())
        {
            SetError ("bone %s refers to nonexistent parent %s", id.c_str(), parent_id.c_str());
            return false;
        }
        pData->bones[id].parent = &pData->bones[parent_id];
    }

    // Finally, look for animations, which refer to bones:
    pChild = pRoot->children;
    while (pChild) {
        if (StrCaseCompare((const char *) pChild->name, "armature") == 0)
        {
            pList = pChild->children;
            while (pList) {

                if (StrCaseCompare((const char *) pList->name, "animations") == 0)
                {
                    pTag = pList->children;
                    while (pTag) {

                        if(StrCaseCompare((const char *) pTag->name, "animation") == 0)
                        {
                            if (!ParseAnimation(pTag, pData->bones, pData->animations))
                            {
                                // 'ParseAnimation' calls 'SetError'
                                return false;
                            }
                        }

                        pTag = pTag->next;
                    }
                }

                pList = pList->next;
            }
        }
        pChild = pChild->next;
    }

    return true;
}
/**
 * Calculates the transformations for every bone in the mesh for a given animation and frame.
 *
 * :returns: the matrices per bone id
 */
std::map <std::string, matrix4> GetBoneTransformations (MeshState *pMesh, const MeshAnimation *pAnimation, const float frame, const bool loop)
{
    std::map<std::string, matrix4> transforms;

    // Iterate over layers (bones)
    for (std::list<MeshBoneLayer>::const_iterator it = pAnimation->layers.begin(); it != pAnimation->layers.end(); it++)
    {

        // Get the closest key on the left and right of the current frame.
        // Also get the leftmost and rightmost keys, in case we have a looping animation.
        const MeshBoneLayer *pLayer = &(*it);

        float keyLeft = -1.0f,
              keyRight = -1.0f,
              keyFirst = -1.0f,
              keyLast = -1.0f,
              key;

        for (std::map <float, MeshBoneKey>::const_iterator jt = pLayer->keys.begin (); jt != pLayer->keys.end (); ++jt)
        {
            key = jt->first;

            if (key >= frame && (keyRight == -1.0f || key < keyRight))
                keyRight = key;

            if (key <= frame && (keyLeft == -1.0f || key > keyLeft))
                keyLeft = key;

            if (key < keyFirst || keyFirst == -1.0f)
                keyFirst = key;

            if (key > keyLast || keyLast == -1.0f)
                keyLast = key;
        }

        transforms[pLayer->bone_id] = matID();

        if (keyLast < 0 || keyFirst < 0)
        {
            fprintf (stderr, "layer %s: no keys\n", pLayer->bone_id.c_str());
            continue;
        }

        if (keyLeft < 0)
        {
            if (loop)
                keyLeft = keyLast;
            else
            {
                fprintf (stderr, "layer %s: no left key at %.3f\n", pLayer->bone_id.c_str(), frame);
                continue;
            }
        }
        else if (keyRight < 0)
        {
            if (loop)
                keyRight = keyFirst;
            else
            {
                fprintf (stderr, "layer %s: no right key at %.3f\n", pLayer->bone_id.c_str(), frame);
                continue;
            }
        }

        // Get the actual keys:
        const MeshBoneKey *pLeft = &pLayer->keys.at (keyLeft),
                          *pRight = &pLayer->keys.at (keyRight);


        // Determine the location and rotation of the bone in this frame:
        vec3 loc;
        Quaternion rot;
        if (keyLeft == keyRight)
        {
            // easy !
            loc = pLeft->loc;
            rot = pLeft->rot;
        }
        else
        {
            /*
                Need to interpolate between the two keys.
                Determine t and u factors, these tell how much of the left and rightkey must be taken
             */
            float range_length = keyRight - keyLeft,
                  range_pos = frame - keyLeft,

                  u = range_pos / range_length,
                  t = 1.0f - u;

            loc = t * pLeft->loc + u * pRight->loc;
            rot = Slerp (pLeft->rot, pRight->rot, u);
        }

        /*
            Make a matrix of it. Apply translation after rotation.
         */
        transforms [pLayer->bone_id] = matTranslation (loc) * matQuat (rot);
    }

    return transforms;
}
void MeshState::ApplyBoneTransformations(const std::map<std::string, matrix4> transforms)
{
    // This counts how many bones modify a given vertex.
    std::map <std::string, int> bonesPerVertex;

    /*
        First set vertices to 0,0,0.
        On these, we will add up all bone transformations.
     */
    for(std::map<std::string, MeshVertex>::iterator it = vertexStates.begin(); it != vertexStates.end(); it++)
    {
        std::string id = it->first;
        MeshVertex *pVertex = &it->second;
        if (pMeshData->vertices.find(id) == pMeshData->vertices.end())
        {
            // This is not expected to happen, since the parser checks for such errors.

            fprintf (stderr, "no such vertex: %s\n", id.c_str());
            continue;
        }
        pVertex->n = vec3 (0,0,0);
        pVertex->p = vec3 (0,0,0);

        bonesPerVertex [id] = 0;
    }

    for(std::map <std::string, MeshBone>::const_iterator it = pMeshData->bones.begin (); it != pMeshData->bones.end (); it++)
    {
        const std::string bone_id = it->first;

        if (pMeshData->bones.find (bone_id) == pMeshData->bones.end ())
        {
            // This is not expected to happen, since the parser checks for such errors.

            fprintf (stderr, "no such bone: %s\n", bone_id.c_str ());
            continue;
        }
        const MeshBone *pBone = &pMeshData->bones.at (bone_id);

        // Transform the bone state:
        MeshBoneState* pBoneState = &boneStates [bone_id];
        std::string chainpoint_id = bone_id;
        vec3 newHead = pBone->posHead,
             newTail = pBone->posTail;
        while (chainpoint_id.size () > 0)
        {
            const MeshBone *pChainPoint = &pMeshData->bones.at (chainpoint_id);
            if (transforms.find (chainpoint_id) != transforms.end ())
            {
                newHead = transforms.at (chainpoint_id) * (newHead - pChainPoint->posHead) + pChainPoint->posHead;
                newTail = transforms.at (chainpoint_id) * (newTail - pChainPoint->posHead) + pChainPoint->posHead;
            }

            chainpoint_id = pChainPoint->parent_id;
        }
        pBoneState->posHead = newHead;
        pBoneState->posTail = newTail;

        // Transform the vertex states:
        for(std::list<std::string>::const_iterator jt = pBone->vertex_ids.begin(); jt != pBone->vertex_ids.end(); jt++)
        {
            std::string vertex_id = *jt;
            const MeshVertex *pDataVertex = &pMeshData->vertices.at(vertex_id);
            MeshVertex *pVertex = &vertexStates[vertex_id];

            vec3 newp = pDataVertex->p,
                 newn = pDataVertex->n;

            std::string chainpoint_id = bone_id;
            while (chainpoint_id.size() > 0)
            {
                if (pMeshData->bones.find(chainpoint_id) == pMeshData->bones.end())
                {
                    // This is not expected to happen, since the parser checks for such errors.

                    fprintf (stderr, "no such bone: %s\n", chainpoint_id.c_str());
                    break;
                }

                const MeshBone *pChainPoint = &pMeshData->bones.at(chainpoint_id);

                if (transforms.find(chainpoint_id) != transforms.end())
                {
                    newp = transforms.at(chainpoint_id) * (newp - pChainPoint->posHead) + pChainPoint->posHead;

                    // Special matrix for normals:
                    matrix4 n_transform = matTranspose (matInverse (transforms.at (chainpoint_id)));
                    newn = n_transform * newn;
                }

                chainpoint_id = pChainPoint->parent_id;
            }

            // Add up the results of all transformations per vertex:
            // (could maybe apply bone weights here)
            bonesPerVertex [vertex_id] ++;
            pVertex->p += newp;
            pVertex->n += newn;
        }
    }

    // Convert vector sums to averages, divide by transform count:
    for(std::map<std::string, MeshVertex>::iterator it = vertexStates.begin(); it != vertexStates.end(); it++)
    {
        std::string id = it->first;
        MeshVertex* pVertex = &vertexStates [id];

        if (bonesPerVertex [id] > 0)
        {
            pVertex->p /= bonesPerVertex [id];
            pVertex->n /= bonesPerVertex [id];
        }
        else // not transformed, set to rest state
        {
            const MeshVertex *pDataVertex = &pMeshData->vertices.at (id);
            pVertex->p = pDataVertex->p;
            pVertex->n = pDataVertex->n;
        }
    }
}
void MeshState::SetAnimationState(const char *animation_id, float frame, bool loop)
{
    const MeshAnimation *pAnimation;
    if (animation_id)
    {
        if (pMeshData->animations.find(animation_id) == pMeshData->animations.end())
        {
            fprintf(stderr, "error, no such animation: %s\n", animation_id);
            InitStates();
            return;
        }
        else
            pAnimation = &pMeshData->animations.at(animation_id);
    }
    else // NULL is given as argument
    {
        // set vertices and bone to initial states

        InitStates();
        return;
    }

    if (frame < 0)
        frame = 0;

    if (loop && frame > pAnimation->length)
    {
        // if animation has length 10 and we request loop frame 11, we will get frame 1

        int iframe = int (frame),
            mframe = iframe % pAnimation->length;
        frame = mframe + (frame - iframe);
    }

    std::map<std::string, matrix4> tranforms = GetBoneTransformations (this, pAnimation, frame, loop);

    ApplyBoneTransformations(tranforms);
}

#include "../io.h"
#include "../xml.h"
bool LoadMesh (const char *zipPath, const char *xmlPath, MeshData *pData)
{
    SDL_RWops *f = SDL_RWFromZipArchive (zipPath, xmlPath);
    if (!f)
    {
        SetError (SDL_GetError ());
        return false;
    }

    xmlDocPtr pDoc = ParseXML (f);
    f->close(f);

    if (!pDoc)
    {
        SetError ("error parsing xml %s: %s", xmlPath, GetError ());
        return false;
    }

    // Convert xml to mesh:

    bool success = ParseMesh (pDoc, pData);
    xmlFreeDoc (pDoc);

    if (!success)
    {
        SetError ("error parsing mesh %s: %s", xmlPath, GetError ());
        return false;
    }

    return true;
}
bool LoadMesh (const std::string &zipPath, const std::string &xmlPath, MeshData *pData)
{
    return LoadMesh (zipPath.c_str(), xmlPath.c_str (), pData);
}
