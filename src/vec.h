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


#ifndef VEC_H
#define VEC_H

#include <math.h>
#include <stdio.h>

#define VEC_O vec3(0,0,0)
#define VEC_UP vec3(0,1.0f,0)
#define VEC_DOWN vec3(0,-1.0f,0)
#define VEC_LEFT vec3(-1.0f,0,0)
#define VEC_RIGHT vec3(1.0f,0,0)
#define VEC_FORWARD vec3(0,0,-1.0f)
#define VEC_BACK vec3(0,0,1.0f)

#define PI 3.1415926535897932384626433832795

/*
    vec3 is a vector of length 3, used for 3D space calculations.
 */

struct vec3
{
    // Coordinates: x, y, and z, or v[0], v[1] and v[2]
    union {
        struct {
            float x, y, z;
        };
        float v[3];
    };

    vec3 (){}
    vec3 (float _x,float _y,float _z){ x=_x; y=_y; z=_z; }
    vec3 (const vec3 &v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }

    /*
        Length2 is the squared length, it's
        computationally less expensive than Length
     */
    float    Length()    const    {return sqrt(x*x+y*y+z*z);}
    float    Length2()    const    {return (x*x+y*y+z*z);}

    vec3    Unit()        const
    {
        // The unit vector has length 1.0

        float l = Length();
        if(l>0) return ((*this)/l);
        else return *this;
    }

    /*
        vec3 objects can be added on, subtracted and
        multiplied/divided by a scalar.
     */

    vec3        operator-  ()                    const { return vec3(-x,-y,-z);    }
    vec3        operator-  (const vec3 &v)        const { return vec3(x-v.x,y-v.y,z-v.z); }
    vec3        operator+  (const vec3 &v)        const { return vec3(x+v.x,y+v.y,z+v.z); }
    void    operator-= (const vec3 &v)        {x-=v.x;y-=v.y;z-=v.z;}
    void    operator+= (const vec3 &v)        {x+=v.x;y+=v.y;z+=v.z;}
    bool    operator== (const vec3 &v)        const { return (x == v.x && y == v.y && z == v.z); }
    bool    operator!= (const vec3 &v)        const { return (x != v.x || y != v.y || z != v.z); }

    vec3        operator/  (const float scalar)        const { return vec3(x/scalar,y/scalar,z/scalar); }
    vec3        operator*  (const float scalar)        const { return vec3(x*scalar,y*scalar,z*scalar); }
    vec3        operator*  (const double scalar)    const { return vec3(float(x*scalar),float(y*scalar),float(z*scalar)); }
    void    operator*= (const float scalar)        { x*=scalar;y*=scalar;z*=scalar; }
    void    operator/= (const float scalar)        { x/=scalar;y/=scalar;z/=scalar; }

};

inline vec3 operator* (const float s, const vec3 &v)
{
    return v*s;
}
inline vec3 Cross(const vec3 &v1, const vec3 &v2)
{
    return vec3(
        v1.y * v2.z - v2.y * v1.z,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
        );
}
inline float Dot(const vec3 &v1,const vec3 &v2)
{
    return (v1.x*v2.x+v1.y*v2.y+v1.z*v2.z);
}
inline float Angle(const vec3 &v1,const vec3 &v2)
{
    return acosf (Dot (v1.Unit(), v2.Unit()));
}//In radians
inline vec3 Projection(const vec3& v,const vec3& on_v)
{
    return on_v*Dot(v,on_v)/on_v.Length2();
}
inline vec3 ClosestPointOnLine(const vec3& l1,const vec3& l2,const vec3& point)
{
    vec3 l1p = point - l1;
    vec3 l12 = l2 - l1;
    float d = l12.Length();
    l12 = l12.Unit();
    float t = Dot (l12, l1p);
    if (t <= 0)
        return l1;
    if (t >= d)
        return l2;
    return l1 + t * l12;
}

struct Plane {

    vec3 n; // plane normal, should have length 1.0
    float d; // shortest distance from (0,0,0) to plane

    // n * d is a point on the plane
};

inline Plane Flip (const Plane  &plane)
{
    Plane r;
    r.n = -plane.n;
    r.d = -plane.d;

    return r;
}

struct Triangle {

    vec3 p[3];

    Triangle () {}

    Triangle (const vec3 &p0, const vec3 &p1, const vec3 &p2)
    {
        p[0] = p0;
        p[1] = p1;
        p[2] = p2;
    }

    vec3 Center () const
    {
        vec3 pb = 0.5f * (p[1] - p[0]),
             pz = pb + 0.333333f*(p[2] - pb);

        return pz;
    }

    Plane GetPlane () const
    {
        Plane plane;
        plane.n = Cross(p[1] - p[0], p[2] - p[0]).Unit();
        plane.d = -Dot(p[0], plane.n);
        return plane;
    }
};


inline bool SameSide (const vec3 &p1, const vec3 &p2, const vec3 &a, const vec3 &b)
{
    vec3 cp1 = Cross (b - a, p1 - a),
         cp2 = Cross (b - a, p2 - a);

    // make it -0.00001f instead of 0.0f to cover up the error in floating point
    return (Dot (cp1, cp2) >= -0.00001f);
}

inline bool PointInsideTriangle (const Triangle &t, const vec3 &p)
{
    return (SameSide (p, t.p[0], t.p[1], t.p[2]) &&
            SameSide (p ,t.p[1], t.p[0], t.p[2]) &&
            SameSide (p, t.p[2], t.p[0], t.p[1]));
}
/*
inline bool PointInsideTriangle(const Triangle &t, const vec3& point)
{
    if (point == t.p[0] || point == t.p[1] || point == t.p[2])
        return true;

    vec3 dp1 = t.p[0] - point;
    vec3 dp2 = t.p[1] - point;
    vec3 dp3 = t.p[2] - point;

    double angle = Angle (dp1, dp2) + Angle (dp2, dp3) + Angle (dp3, dp1);

    const double match_factor = 0.99; // Used to cover up the error in floating point

    if (angle >= (match_factor * (2.0 * PI)))
    // If the angle is greater than 2 PI, (360 degrees), the point is inside.
        return true;

    else return false;
}
 */
inline float DistanceFromPlane(const vec3& p, const Plane &plane)
{
    return Dot (plane.n, p) + plane.d;
}
inline vec3 PlaneProjection(const vec3& p, const Plane &plane)
{
    return p - DistanceFromPlane(p, plane) * plane.n;
}
#endif // VEC_H
