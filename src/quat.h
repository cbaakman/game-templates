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


#ifndef QUAT_H
#define QUAT_H

#include <math.h>

#define QUAT_ID Quaternion(0,0,0,1)

struct Quaternion {

    union {
        struct {
            float x, y, z, w;
        };
        float q[4];
    };

    Quaternion () {}
    Quaternion (const float _x, const float _y, const float _z, const float _w)
    {
        x=_x;
        y=_y;
        z=_z;
        w=_w;
    }
    Quaternion (const Quaternion &other)
    {
        for (int i=0; i < 4; i++)
            q[i] = other.q[i];
    }

    float Length2 () const
    {
        float length2 = 0.0f;
        for (int i=0; i < 4; i++)
            length2 += q[i] * q[i];

        return length2;
    }
    float Length () const 
    {
        return sqrt (Length2());
    }

    Quaternion operator/ (const float scalar)
    {
        Quaternion m;
        for (int i=0; i < 4; i++)
            m.q[i] = q[i] / scalar;
        return m;
    }

    Quaternion Unit()
    {
        float l = Length();
        if (l > 0)
            return (*this) / l;
        else
            return (*this);
    }
};

inline float Dot (const Quaternion &q1, const Quaternion &q2)
{
    float v = 0.0f;
    for (int i=0; i < 4; i++)
        v += q1.q[i] * q2.q[i];
    return v;
}

inline Quaternion operator+ (const Quaternion &q1, const Quaternion &q2)
{
    Quaternion sum;
    for (int i=0; i < 4; i++)
        sum.q[i] = q1.q[i] + q2.q[i];
    return sum;
}
inline Quaternion operator* (const Quaternion &q, const float scalar)
{
    Quaternion m;
    for (int i=0; i < 4; i++)
        m.q[i] = q.q[i] * scalar;
    return m;
}

inline Quaternion operator/ (const Quaternion &q, const float scalar)
{
    Quaternion m;
    for (int i=0; i < 4; i++)
        m.q[i] = q.q[i] / scalar;
    return m;
}

inline Quaternion slerp (Quaternion start, Quaternion end, float s)
{
    if (start.Length2() != 1.0f)
        start = start.Unit();

    if (end.Length2() != 1.0f)
        end = end.Unit();

    float dot = Dot(start, end),
          w1, w2;

    if (dot > 0.9995) // too close
    {
        w1 = (1.0f - s);
        w2 = s;
    }
    else
    {
        // stay within acos domain
        if (dot < -1.0f)
            dot = -1.0f;

        float theta = acos(dot);
        float sTheta = sin(theta);

        w1 = sin ((1.0f - s) * theta) / sTheta;
        w2 = sin (s * theta) / sTheta;
    }

    return start * w1 + end * w2;
}

#endif
