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


#include "collision.h"
#include <math.h>
#include <stdio.h>
#include "../util.h"

template <class Number>
Number abs(Number n)
{
    if (n < 0)
        return -n;
    else
        return n;
}
int LineSphereIntersections(const vec3& p1, const vec3& p12, const vec3& center, float radius, double *t1, double *t2)
{
    double a = Dot(p12, p12);        // a is always positive
    double b = 2.0 * Dot(p12, p1 - center);
    double c = Dot(center,center) + Dot(p1,p1) - 2.0 * Dot(center, p1) - radius * radius;
    double diskriminant = b * b - 4.0 * a * c;

    if (diskriminant < 0 || a == 0)
    {
        *t1=0;
        *t2=0;
        return 0;
    }
    if (diskriminant == 0)
    {
        *t2 = *t1 = (-b / (2.0 * a));
        return 1;
    }
    double sqrt_diskriminant = sqrt(diskriminant);
    *t1 = ((-b + sqrt_diskriminant) / (2.0 * a));
    *t2 = ((-b - sqrt_diskriminant) / (2.0 * a));
    // because a is always positive, t1 > t2
    return 2;
}
float LinePlaneIntersectionDirParameter(const vec3& p1, const vec3& p12, const Plane &plane)
{
    vec3 p12_norm = p12.Unit();
    float distance_plane_p1 = DistanceFromPlane(p1, plane);
    float divisor = Dot(plane.n, p12_norm);
    if (divisor == 0.0f)
        return 0.0f; // line lies on plane
    float t = -distance_plane_p1 / divisor;
    return t / p12.Length(); // at p1 t=0, at p2 t=1.0f
}
vec3 LinePlaneIntersectionDir(const vec3& p1,const vec3& p12, const Plane &plane)
{
    vec3 p12_norm = p12.Unit();

    float distance_plane_p1 = DistanceFromPlane(p1, plane);
    float divisor = Dot(plane.n, p12_norm);

    if(divisor == 0.0f)
        return p1; // line lies on plane

    float t = -distance_plane_p1 / divisor;

    vec3 result = (p1 + t * p12_norm);

    return result;
}
SphereCollider :: SphereCollider (const vec3 &c, const float r) : relativeCenter(c), radius(r) {}
bool SphereCollider :: HitsTriangle (const Triangle &tri, const vec3 &startP, const vec3 &movement,
                        vec3 &outClosestPointOnSphere, vec3 &outContactPointSphereToTriangle, vec3 &outContactNormal)
{
    const Plane plane = tri.GetPlane();
    const vec3 center = startP + relativeCenter;

    vec3 closestPointOnSphere;

    float distanceCenterToPlane = DistanceFromPlane(center, plane);

    if (distanceCenterToPlane > 0)
        closestPointOnSphere = center - radius * plane.n;
    else
        closestPointOnSphere = center + radius * plane.n;

    vec3 contactPointSphereToPlane;
    if (fabs (distanceCenterToPlane) < radius * 0.9999f)

        // sphere lies too close to plane
        contactPointSphereToPlane = center - distanceCenterToPlane * plane.n;

    else // at p1, the sphere is still far enough from the plane
    {
        if (Dot(movement, (center - closestPointOnSphere)) > -0.000001f * radius)
            // Movement is away from plane
            return false;

        float t = LinePlaneIntersectionDirParameter(closestPointOnSphere, movement, plane);
        if((t > 1.0f) || (t < -radius / movement.Length()))

            // Movement doesn't reach plane
            return false;
        else
            contactPointSphereToPlane = closestPointOnSphere + t * movement;
    }

    vec3 contactPointSphereToTriangle;
    if(PointInsideTriangle(tri, contactPointSphereToPlane))
    {
        // Then it's easy
        contactPointSphereToTriangle = contactPointSphereToPlane;
    }
    else // hit point is outside triangle
    {
        // See if the sphere hits the edge during movement

        vec3    tri_c12 = ClosestPointOnLine(tri.p[0], tri.p[1], contactPointSphereToPlane),
                tri_c23 = ClosestPointOnLine(tri.p[1], tri.p[2], contactPointSphereToPlane),
                tri_c13 = ClosestPointOnLine(tri.p[0], tri.p[2], contactPointSphereToPlane);
        float    dist_c12 = (tri_c12 - contactPointSphereToPlane).Length2(),
                dist_c23 = (tri_c23 - contactPointSphereToPlane).Length2(),
                dist_c13 = (tri_c13 - contactPointSphereToPlane).Length2();

        if(dist_c12 < dist_c13)
        {
            if(dist_c12 < dist_c23) contactPointSphereToTriangle = tri_c12;
            else                    contactPointSphereToTriangle = tri_c23;
        }
        else
        {
            if(dist_c13 < dist_c23) contactPointSphereToTriangle = tri_c13;
            else                    contactPointSphereToTriangle = tri_c23;
        }

        double t1, t2;
        if(LineSphereIntersections(contactPointSphereToTriangle, movement, center, radius, &t1, &t2))
        {
            if(t1 <= 0 && t2 < 0)
            {
                if(t1 < -1.0f)
                    return false; // Too far away.

                closestPointOnSphere = t1 * movement + contactPointSphereToTriangle;
            }
            else if(t1 > 0 && t2 < 0)
            {
                if(abs(t1) < abs(t2))
                    closestPointOnSphere = t1 * movement + contactPointSphereToTriangle;
                else
                    closestPointOnSphere = t2 * movement + contactPointSphereToTriangle;
            }
            else /*if(t1>0 && t2>0)*/
                return false; // Too far away.
        }
        else
            return false; // No intersections with this sphere.
    }

    outClosestPointOnSphere = closestPointOnSphere;
    outContactPointSphereToTriangle = contactPointSphereToTriangle;
    outContactNormal = (center - closestPointOnSphere).Unit();

    return true;
}
FeetCollider :: FeetCollider (const vec3 pivotToFeet) : toFeet (pivotToFeet) {}
bool FeetCollider :: HitsTriangle (const Triangle &tri, const vec3 &p, const vec3 &movement,
                      vec3 &closestPointOnFeet, vec3 &outContactPoint, vec3 &outContactNormal)
{
    const Plane plane = tri.GetPlane();

    if (abs (Dot(plane.n, -toFeet)) < 0.0001f)
        return false; // plane is parallel to feet direction

    vec3 pOnGroundUnderP = LinePlaneIntersectionDir(p, toFeet, plane),
         feet_start = p + toFeet,
         contactPointFeetToPlane;

    float feet_start_above = DistanceFromPlane (feet_start, plane),
          head_start_above = DistanceFromPlane (p, plane);

    if (feet_start_above * head_start_above < 0) // feet sticking through ground
    {
        contactPointFeetToPlane = pOnGroundUnderP;
    }
    else
    {
        vec3 projection = PlaneProjection(feet_start, plane);

        if (Dot ((projection - feet_start), movement) < 0.0001f)
            return false; // not moving towards floor

        float t = LinePlaneIntersectionDirParameter(feet_start, movement, plane);
        if((t > 1.0f) || (t < 0))
            return false; // contact point is too far
        else
            contactPointFeetToPlane = feet_start + t * movement;
    }

    if (!PointInsideTriangle(tri, contactPointFeetToPlane))
    {
        return false;
    }

    // move it back up from feet to the head position
    closestPointOnFeet = feet_start;
    outContactPoint = contactPointFeetToPlane;
    outContactNormal = plane.n;
    return true;
}
#define COLLISION_MAXITERATION 10
#define MIN_WALL_DISTANCE 1.0e-3f
vec3 CollisionMove (const vec3& p1, const vec3& p2,
                    const ColliderP *colliders, const int n_colliders,
                    const Triangle *triangles, const int n_triangles)
{
    vec3 contactPoint,
         closestPointOnObject,

         testContactPoint,
         testClosestPointOnObject,
         testNormal,

         current_p = p1,
         targetMovement = p2 - p1,
         freeMovement = targetMovement,
         pushingMovement;

    const vec3 inputMovement = targetMovement;

    Plane pushedPlane;

    int j = 0;
    do
    {
        float smallestDist = 1.0e+15f,
              dist2;

        bool pushingSomething = false;

        for(int i = 0; i < n_triangles; i++)
        {
            for (int c = 0; c < n_colliders; c++)
            {
                ColliderP pCollider = colliders [c];

                if (pCollider->HitsTriangle (triangles [i], current_p, targetMovement,
                        testClosestPointOnObject, testContactPoint, testNormal))
                {
                    dist2 = (testContactPoint - testClosestPointOnObject).Length2();
                    if (dist2 < smallestDist)
                    {
                        smallestDist = dist2;

                        contactPoint = testContactPoint;
                        closestPointOnObject = testClosestPointOnObject;
                        pushedPlane.n = testNormal;
                    }
                    pushingSomething = true;
                }
            }
        }

        if (pushingSomething)
        {
            // movement up to the plane
            freeMovement = contactPoint - closestPointOnObject;

            pushedPlane.d = -Dot (pushedPlane.n, contactPoint);

            // plane projection of the movement:
            pushingMovement = PlaneProjection (closestPointOnObject + targetMovement, pushedPlane) - contactPoint;

            // Don't go in opposite direction:
            if (Dot (inputMovement, pushingMovement) < 0)
            {
                pushingMovement = VEC_O;
            }

            current_p += freeMovement + pushedPlane.n * MIN_WALL_DISTANCE;

            // Change goal:
            targetMovement = pushingMovement;

            j++;
        }
        else
        {
            current_p += targetMovement;

            return current_p;
        }
    }
    while ((targetMovement.Length2() > 1.0e-8f) && j < COLLISION_MAXITERATION);

    return current_p;
}
bool TestOnGround (const vec3& p,
                   const ColliderP *colliders, const int n_colliders,
                   const Triangle *triangles, const int n_triangles,
                   const float min_cosine, const vec3 &_up)
{
    float dist, cosine, max_dist = (1.0f + sqrt(0.5f)) * MIN_WALL_DISTANCE;
    Plane plane, fakePlane;
    vec3 testClosestPointOnObject,
         testContactPoint,
         testNormal;

    const vec3 up = _up.Unit();

    for(int i = 0; i < n_triangles; i++)
    {
        plane = triangles[i].GetPlane();

        // Make sure the normal points up
        if (Dot (plane.n, up) < 0)
            plane = Flip (plane);

        cosine = Dot (plane.n, up);
        if (cosine < min_cosine)
        { // skip walls
            continue;
        }

        for (int c = 0; c < n_colliders; c++)
        {
            ColliderP pCollider = colliders [c];

            // Move it down and see if it hits the triangle:

            if (pCollider->HitsTriangle (triangles [i], p, -up,
                    testClosestPointOnObject, testContactPoint, testNormal))
            {
                fakePlane.n = testNormal;
                fakePlane.d = -Dot (fakePlane.n, testContactPoint);

                dist = DistanceFromPlane (testClosestPointOnObject, fakePlane);

                if (dist < max_dist)
                {
                    return true;
                }
            }
        }
    }

    return false;
}
vec3 PutOnGround (const vec3& p,
                  const ColliderP *colliders, const int n_colliders,
                  const Triangle *triangles, const int n_triangles,
                  const float min_cosine, const vec3 &_up)
{
    float dist2,
          smallest = 1.0e+15f,
          cosine;
    Plane plane;
    vec3 testClosestPointOnObject,
         testContactPoint,
         testNormal,
         movement,
         smallest_p = p;

    const vec3 &up = _up.Unit();

    for(int i = 0; i < n_triangles; i++)
    {
        plane = triangles[i].GetPlane();

        // Make sure the normal points up
        if (Dot (plane.n, up) < 0)
            plane = Flip (plane);

        cosine = Dot (plane.n, up);
        if (cosine < min_cosine)
        { // skip walls
            continue;
        }

        for (int c = 0; c < n_colliders; c++)
        {
            ColliderP pCollider = colliders [c];

            // Move it down and see if it hits the triangle:

            if (pCollider->HitsTriangle (triangles [i], p, -smallest * up,
                    testClosestPointOnObject, testContactPoint, testNormal))
            {
                movement = testContactPoint - testClosestPointOnObject;
                dist2 = movement.Length2();
                if (dist2 < smallest)
                {
                    smallest = dist2;

                    smallest_p = p + movement + up * MIN_WALL_DISTANCE / cosine;
                }
            }
        }
    }

    return smallest_p;
}
vec3 CollisionWalk (const vec3& p1, const vec3& p2,
                    const ColliderP *colliders, const int n_colliders,
                    const Triangle *triangles, const int n_triangles,
                    const float min_cosine, const vec3 &_up)
{
    vec3 ground_p,
         contactPoint,
         closestPointOnObject,

         testContactPoint,
         testClosestPointOnObject,
         testNormal,

         current_p = p1,
         targetMovement = p2 - p1,
         freeMovement = targetMovement,
         pushingMovement;

    const vec3 inputMovement = targetMovement, up = _up.Unit();

    Plane pushedPlane;

    int j = 0;
    do
    {
        float smallestDist = 1.0e+15f,
              dist2;

        bool pushingSomething = false;

        for(int i = 0; i < n_triangles; i++)
        {
            for (int c = 0; c < n_colliders; c++)
            {
                ColliderP pCollider = colliders [c];

                if (pCollider->HitsTriangle (triangles [i], current_p, targetMovement,
                        testClosestPointOnObject, testContactPoint, testNormal))
                {
                    dist2 = (testContactPoint - testClosestPointOnObject).Length2();
                    if (dist2 < smallestDist)
                    {
                        smallestDist = dist2;

                        contactPoint = testContactPoint;
                        closestPointOnObject = testClosestPointOnObject;
                        pushedPlane.n = testNormal;
                    }
                    pushingSomething = true;
                }
            }
        }

        if (pushingSomething)
        {
            // movement up to the plane
            freeMovement = contactPoint - closestPointOnObject;

            bool wall = false;
            float slope_cosine = Dot (pushedPlane.n, up);
            if (slope_cosine < min_cosine)
            {
                // treat steep slopes as vertical walls
                pushedPlane.n = pushedPlane.n - slope_cosine * up;
                wall = true;
            }

            pushedPlane.d = -Dot (pushedPlane.n, contactPoint);

            // plane projection of the movement:
            pushingMovement = PlaneProjection (closestPointOnObject + targetMovement, pushedPlane) - contactPoint;

            // Don't go in opposite direction:
            if (Dot (inputMovement, pushingMovement) < 0)
            {
                pushingMovement = VEC_O;
            }

            current_p += freeMovement + pushedPlane.n * MIN_WALL_DISTANCE;

            if (wall)
            {
                // Put it back on the ground, if any
                ground_p = PutOnGround (current_p, colliders, n_colliders, triangles, n_triangles, min_cosine, up);
                if ((ground_p - current_p).Length2() < targetMovement.Length2())
                {
                    pushingMovement += ground_p - current_p;
                    j --;
                }
            }

            // Change goal:
            targetMovement = pushingMovement;

            j++;
        }
        else
        {
            current_p += targetMovement;

            // Put it back on the ground, if any
            ground_p = PutOnGround (current_p, colliders, n_colliders, triangles, n_triangles, min_cosine, up);
            if ((ground_p - current_p).Length2() < targetMovement.Length2())
            {
                return ground_p;
            }

            return current_p;
        }
    }
    while ((targetMovement.Length2() > 1.0e-8f) && j < COLLISION_MAXITERATION);

    return current_p;
}

vec3 CollisionTraceBeam(const vec3 &p1, const vec3 &p2, const Triangle *triangles, const int n_triangles)
{
    if(p1 == p2)
        return p1; // otherwise we might get unexpected results

    vec3 p12 = p2 - p1, isect = p2, newisect;
    for(int i = 0; i < n_triangles; i++)
    {
        newisect = LinePlaneIntersectionDir(p1, p12, triangles[i].GetPlane());

        if (!PointInsideTriangle(triangles[i], newisect))
            continue;

        vec3 delta = newisect - p1;

        if( delta.Length2() < (p1 - isect).Length2() && Dot(delta, p12) > 0)

            isect = newisect;
    }
    return isect;
}
/* float GetGroundSlope (const vec3 &p1, const vec3 &p2, const Triangle *triangles, const int n_triangles)
{
    if(p1 == p2)
        return 0;

    float smallest = 1.0e+15f, dist2, slope;
    Plane plane;
    vec3 isect1, isect2, p12;
    for(int i = 0; i < n_triangles; i++)
    {
        plane = triangles[i].GetPlane();

        if (abs(Dot (plane.n, VEC_UP)) < 1.0e-8f)

            // skip parallels
            continue;

        isect1 = LinePlaneIntersectionDir(p1, VEC_DOWN, plane);

        dist2 = (isect1 - p1).Length2();
        if (dist2 < smallest)
        {
            smallest = dist2;
            isect2 = LinePlaneIntersectionDir(p2, VEC_DOWN, plane);

            p12 = isect2 - isect1;
            slope = p12.y / sqrt(sqr (p12.x) + sqr (p12.z));
        }
    }

    return slope;
}
vec3 CollisionMoveFeet (const vec3& p1, const vec3& p2, const float height, const Triangle *triangles, const int n_triangles)
{
    vec3 feet_start = {p1.x, p1.y - height, p1.z},

         pOnGroundUnderP1,
         pContactPlane,

         desiredMovement = p2 - p1,
         allowedMovement = desiredMovement;

    const vec3 requested_mv = desiredMovement;

    Plane closestPlane;

    int j = 0;
    do
    {
        float smallestDist = 1.0e+15f,
              start_above,
              head_start_above,
              dist2;
        bool pushing = false;

        for(int i = 0; i < n_triangles; i++)
        {
            const Triangle *tri = &triangles[i];
            const Plane plane = tri->GetPlane();

            if (abs (Dot(plane.n, VEC_UP)) < 0.0001f)
                continue; // parallel to fall direction

            pOnGroundUnderP1 = LinePlaneIntersectionDir(p1, VEC_DOWN, plane);

            start_above = (feet_start - pOnGroundUnderP1).y;
            head_start_above = start_above + height;

            if (start_above < 0 && head_start_above > 0) // feet sticking through ground
            {
                pContactPlane = {feet_start.x, feet_start.y - start_above, feet_start.z};
            }
            else
            {
                vec3 projection = PlaneProjection(feet_start, plane);

                if (Dot ((projection - feet_start), desiredMovement) < 0.0001f)
                    continue; // not moving towards floor

                float t = LinePlaneIntersectionDirParameter(feet_start, desiredMovement, plane);
                if((t > 1.0f) || (t < 0))
                    continue; // no contact on the way to p2
                else
                    pContactPlane = feet_start + t * desiredMovement;
            }

            if (!PointInsideTriangle(*tri, pContactPlane))
                continue;

            dist2 = (pContactPlane - feet_start).Length2();
            if (dist2 < smallestDist)
            {
                smallestDist = dist2;
                allowedMovement = pContactPlane - feet_start;
                closestPlane = plane;
            }
            pushing = true;
        }

        if (pushing)
        {
            //plane projection of the movement:
            vec3 delta = LinePlaneIntersectionDir(feet_start, desiredMovement, closestPlane) - feet_start;

            if(Dot(requested_mv, delta) < 0)
            {
                delta = {0, 0, 0};
            }

            feet_start += allowedMovement + closestPlane.n * 1.0e-3f;
            desiredMovement = delta;
            allowedMovement = desiredMovement;

            j++;
        }
        else
        {
            feet_start += allowedMovement;
            return {feet_start.x, feet_start.y + height, feet_start.z};
        }
    }
    while ((desiredMovement.Length2() > 1.0e-8f) && j < COLLISION_MAXITERATION);

    return {feet_start.x, feet_start.y + height, feet_start.z};
}
vec3 CollisionMoveSphere (const vec3& _p1, const vec3& _p2, float radius, const Triangle *triangles, const int n_triangles)
{
    if(_p1==_p2)
        return _p1;

    vec3 p1 = _p1,
        p2 = _p2,
        p12 = p2 - p1;

    const vec3 ori_p12 = p12;
    unsigned int j = 0;
    float radius2 = radius * radius;

    vec3 newmove = p12,
        newClosestPointOnSphere,
        newContactPointSphereToTriangle;

    do
    {
        float distanceCenterToTriangle=1.0e+15f;
        bool pushing=false;
        for(int i = 0; i < n_triangles; i++)
        {
            const Triangle *tri = &triangles[i];
            const Plane plane = tri->GetPlane();

            vec3 normal = plane.n,
                p_move,
                center = p1,
                ClosestPointOnSphere;

            float distanceCenterToPlane = DistanceFromPlane(center, plane);

            if (distanceCenterToPlane > 0)
                ClosestPointOnSphere = center - radius * normal;
            else
                ClosestPointOnSphere = center + radius * normal;

            vec3 contactPointSphereToPlane;
            if (fabs (distanceCenterToPlane) < radius * 0.9999f)

                // sphere lies too close to plane
                contactPointSphereToPlane = center - distanceCenterToPlane * normal;

            else // at p1, the sphere is still far enough from the plane
            {
                if (Dot(p12, (center - ClosestPointOnSphere)) > -0.000001f * radius)
                    // Movement is away from plane
                    continue;

                float t = LinePlaneIntersectionDirParameter (ClosestPointOnSphere, p12, plane);
                if ((t > 1.0f) || (t < -radius/p12.Length()))

                    // Movement doesn't reach plane
                    continue;
                else
                    contactPointSphereToPlane = ClosestPointOnSphere + t * p12;
            }

            vec3 contactPointSphereToTriangle;
            if (PointInsideTriangle (*tri, contactPointSphereToPlane))
            {
                // Then it's easy
                contactPointSphereToTriangle = contactPointSphereToPlane;
            }
            else // hit point is outside triangle
            {
                // See if the sphere hits the edge during movement

                vec3    tri_c12 = ClosestPointOnLine(tri->p[0], tri->p[1], contactPointSphereToPlane),
                        tri_c23 = ClosestPointOnLine(tri->p[1], tri->p[2], contactPointSphereToPlane),
                        tri_c13 = ClosestPointOnLine(tri->p[0], tri->p[2], contactPointSphereToPlane);
                float   dist_c12 = (tri_c12 - contactPointSphereToPlane).Length2(),
                        dist_c23 = (tri_c23 - contactPointSphereToPlane).Length2(),
                        dist_c13 = (tri_c13 - contactPointSphereToPlane).Length2();

                if(dist_c12 < dist_c13)
                {
                    if(dist_c12 < dist_c23) contactPointSphereToTriangle = tri_c12;
                    else                    contactPointSphereToTriangle = tri_c23;
                }
                else
                {
                    if(dist_c13 < dist_c23) contactPointSphereToTriangle = tri_c13;
                    else                    contactPointSphereToTriangle = tri_c23;
                }

                double t1, t2;
                if(LineSphereIntersections(contactPointSphereToTriangle, p12, center, radius, &t1, &t2))
                {
                    if (t1 <= 0 && t2 < 0)
                    {
                        if (t1 < -1.0f)
                            continue; // Too far away.

                        ClosestPointOnSphere = t1 * p12 + contactPointSphereToTriangle;
                    }
                    else if(t1 > 0 && t2 < 0)
                    {
                        if (abs(t1) < abs(t2))  ClosestPointOnSphere = t1 * p12 + contactPointSphereToTriangle;
                        else                    ClosestPointOnSphere = t2 * p12 + contactPointSphereToTriangle;
                    }
                    else // if(t1>0 && t2>0)
                        continue; // Too far away.
                }
                else
                    continue; // No intersections with this sphere.
            }
            p_move = contactPointSphereToTriangle - ClosestPointOnSphere;

            // Pick the shortest possible moving distance of all triangles
            float dist2 = (contactPointSphereToTriangle - center).Length2();
            if(dist2 < distanceCenterToTriangle)
            {
                distanceCenterToTriangle = dist2;
                newmove = p_move;
                newClosestPointOnSphere = ClosestPointOnSphere;
                newContactPointSphereToTriangle = contactPointSphereToTriangle;
            }
            pushing = true;
        }

        if (pushing) // movement needs to be adjusted
        {
            // Calculate a fictional plane
            Plane plane;
            plane.n = (p1 - newClosestPointOnSphere).Unit();
            plane.d = -Dot(plane.n, newContactPointSphereToTriangle);

            // plane projection of the movement:
            vec3 delta = LinePlaneIntersectionDir( newClosestPointOnSphere + p12, plane.n, plane)- newContactPointSphereToTriangle;

            if (Dot(ori_p12, delta) < 0) // moves in opposite direction, let's not!
            {
                delta = {0, 0, 0};
            }

            // Do the moving part that collision allows
            p1 += newmove + 0.0001f * radius * plane.n;

            // Reset our goal to what last plane's projection allows
            p2 = p1 + delta;
            p12 = p2 - p1;
            newmove = p12;

            // count the number of pushes
            j++;
        }
        else
        {
            p1 += newmove;
            return p1;
        }
    }
    while((p12.Length2() > 1.0e-8f * radius2) && j < COLLISION_MAXITERATION);

    return p1;
} */
