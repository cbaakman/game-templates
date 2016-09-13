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

#ifndef TERRAGEN_H
#define TERRAGEN_H

#include <string>
#include <sstream>
#include <list>
#include "../vec.h"
#include "../util.h"

#include <math.h>
#include <functional>

inline size_t HashFromStr (const std::string &s)
{
    return std::hash <std::string> ()(s);
}

template <class TerrainType>
TerrainType ForNode (const std::string &seed, const vec2 &pos);

template <class TerrainType>
class TerraGen
{
private:
    std::string mSeed;
    float mNodeDist;

    float GetLeftMostNodeCoord (const float coord) const
    {
        if (coord < 0)
            return -mNodeDist - GetLeftMostNodeCoord (-coord);

        return mNodeDist * floor (coord / mNodeDist);
    }

public:
    TerraGen (const std::string &seed, const float nodeDist):
        mSeed(seed), mNodeDist(nodeDist) {}


    TerrainType InterpolateAt (const float x, const float z) const
    {
        return InterpolateAt ({x, z});
    }

    TerrainType InterpolateAt (const vec2 &pos) const
    {
        int ix, iz;
        float tx, tz,
              z [4],
              x [4];

        /* For cubic interpolation, 4 nodes are required per dimension
           (X and Z): */
        TerrainType n [4][4],
                    nt [4];

        // Determine node positions..

        x [1] = GetLeftMostNodeCoord (pos.x);
        x [0] = x [1] - mNodeDist;
        x [2] = x [1] + mNodeDist;
        x [3] = x [2] + mNodeDist;

        tx = (pos.x - x [1]) / mNodeDist;

        z [1] = GetLeftMostNodeCoord (pos.z);
        z [0] = z [1] - mNodeDist;
        z [2] = z [1] + mNodeDist;
        z [3] = z [2] + mNodeDist;

        tz = (pos.z - z [1]) / mNodeDist;

        // Get Values for nodes:
        for (ix = 0; ix < 4; ix++)
            for (iz = 0; iz < 4; iz++)
                n [iz][ix] = ForNode <TerrainType> (mSeed,
                                                    vec2 (x [ix], z [iz]));

        // Interpolate cubically from 1 to 2 over both x and z...


        // Interpolate over x for every node's z-value:
        for (iz = 0; iz < 4; iz++)
        {
            nt [iz] = InterpolateCubic <TerrainType> (n [iz][0], n [iz][1],
                                                      n [iz][2], n [iz][3],
                                                      tx);
        }

        // Interpolate over z, at x=tx:
        return InterpolateCubic <TerrainType> (nt [0], nt [1],
                                               nt [2], nt [3], tz);
    }
};

#endif // TERRAGEN_H
