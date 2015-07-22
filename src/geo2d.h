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


#ifndef GEOMETRY_H
#define GEOMETRY_H

struct vec2
{
    float x,y;

    vec2 operator* (const float& f) const;
    vec2 operator/ (const float& f) const;
    vec2 operator+ (const vec2& v2) const;
    vec2 operator- (const vec2& v2) const;
    void operator+= (const vec2& v2);
    void operator-= (const vec2& v2);
    bool operator== (const vec2& other) const;
    vec2 operator- () const;
    float length2() const;
    float length() const;
    float angle() const; // with x-axis
    vec2 unit() const;
    vec2 rotate(float angle) const;

    vec2();
    vec2(float x, float y);
};
vec2 operator* (const float& f, const vec2& v);
float distance2(const vec2& v1, const vec2& v2);
float distance(const vec2& v1, const vec2& v2);
float dot (const vec2& v1, const vec2& v2);
float angle(const vec2&v1,const vec2&v2);
vec2 projection(const vec2& v,const vec2& on_v);
vec2 lineIntersection(const vec2& a1, const vec2& a2, const vec2& b1, const vec2& b2);
vec2 pointOnBezierCurve(float t, const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3);

#endif
