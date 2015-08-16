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

/**
 * Returns the absolute value: negative becomes positive,
 * positive stays positive.
 */
template <class Number>
Number abs (Number n)
{
    if (n < 0)
        return -n;
    else
        return n;
}
/**
 * Calculates the intersection points between
 * a sphere (center, radius) and a line (p1, p12)
 *
 * t1 and t2 will be set so that:
 *  intersection1 = p1 + t1 * p12
 *  intersection2 = p1 + t2 * p12
 *
 * :returns: the number of intersection points (0, 1 or 2)
 */
int LineSphereIntersections (const vec3& p1, const vec3& p12,
                             const vec3& center, float radius,

                             double *t1, double *t2)
{
    double a = Dot (p12, p12), // a is always positive
           b = 2.0 * Dot(p12, p1 - center),
           c = Dot (center, center) + Dot (p1, p1) - 2.0 * Dot (center, p1) - radius * radius,
           diskriminant = b * b - 4.0 * a * c;

    if (diskriminant < 0 || a == 0)
    {
        *t1 = 0;
        *t2 = 0;
        return 0;
    }
    if (diskriminant == 0)
    {
        *t2 = *t1 = (-b / (2.0 * a));
        return 1;
    }
    double sqrt_diskriminant = sqrt (diskriminant);
    *t1 = ((-b + sqrt_diskriminant) / (2.0 * a));
    *t2 = ((-b - sqrt_diskriminant) / (2.0 * a));

    // because a is always positive, t1 > t2
    return 2;
}
/**
 * Calculates intersection point between a line (p1, p12) and a plane
 * (there's always one, unless the line is parallel to the plane)
 *
 * The return value is set so that:
 *  intersection = p1 + return_value * p12
 */
float LinePlaneIntersectionDirParameter(const vec3& p1, const vec3& p12, const Plane &plane)
{
    vec3 p12_norm = p12.Unit();

    float distance_plane_p1 = DistanceFromPlane (p1, plane);
    float divisor = Dot (plane.n, p12_norm);

    if (divisor == 0.0f)
        return 0.0f; // line is parallel to plane

    float t = -distance_plane_p1 / divisor;
    return t / p12.Length(); // at p1 t=0, at p2 t=1.0f
}
/**
 * Calculates intersection point between a line (p1, p12) and a plane
 * (there's always one, unless the line is parallel to the plane)
 *
 * :returns: intersection point
 */
vec3 LinePlaneIntersectionDir (const vec3& p1, const vec3& p12, const Plane &plane)
{
    vec3 p12_norm = p12.Unit();

    float distance_plane_p1 = DistanceFromPlane (p1, plane);
    float divisor = Dot (plane.n, p12_norm);

    if(divisor == 0.0f)
        return p1; // line parallel to plane

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
        if (Dot (movement, (center - closestPointOnSphere)) > -0.000001f * radius)
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

        vec3    tri_c12 = ClosestPointOnLine (tri.p[0], tri.p[1], contactPointSphereToPlane),
                tri_c23 = ClosestPointOnLine (tri.p[1], tri.p[2], contactPointSphereToPlane),
                tri_c13 = ClosestPointOnLine (tri.p[0], tri.p[2], contactPointSphereToPlane);
        float   dist_c12 = (tri_c12 - contactPointSphereToPlane).Length2(),
                dist_c23 = (tri_c23 - contactPointSphereToPlane).Length2(),
                dist_c13 = (tri_c13 - contactPointSphereToPlane).Length2();

        if (dist_c12 < dist_c13)
        {
            if (dist_c12 < dist_c23)
                contactPointSphereToTriangle = tri_c12;
            else
                contactPointSphereToTriangle = tri_c23;
        }
        else
        {
            if(dist_c13 < dist_c23)
                contactPointSphereToTriangle = tri_c13;
            else
                contactPointSphereToTriangle = tri_c23;
        }

        double t1, t2;
        if (LineSphereIntersections (contactPointSphereToTriangle, movement, center, radius, &t1, &t2))
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

    if (abs (Dot (plane.n, -toFeet)) < 0.0001f)
        return false; // plane is parallel to feet direction

    vec3 pOnGroundUnderP = LinePlaneIntersectionDir (p, toFeet, plane),
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

    if (!PointInsideTriangle (tri, contactPointFeetToPlane))
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

        // Iterate over every triangle and every collider to detect hits:
        for(int i = 0; i < n_triangles; i++)
        {
            for (int c = 0; c < n_colliders; c++)
            {
                ColliderP pCollider = colliders [c];

                if (pCollider->HitsTriangle (triangles [i], current_p, targetMovement,
                        testClosestPointOnObject, testContactPoint, testNormal))
                {
                    // Collider hit a triangle

                    dist2 = (testContactPoint - testClosestPointOnObject).Length2();
                    if (dist2 < smallestDist)
                    {
                        // The closest hit on the path gets priority

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

            /*
                We moved up to the wall and adjusted our movement,
                but we might encounter a new wall while moving along this one.
                Do another iteration to find out..
             */
        }
        else // unhindered movement
        {
            current_p += targetMovement;

            return current_p;
        }
    }
    // Don't go on iterating forever in corners!
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
                // Collider hits floor when moved down, now see if the floor's close enough:

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
                // Collider hits floor when moved down, now see if the floor's close enough:

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
                    // The collider hit the triangle on the way.

                    dist2 = (testContactPoint - testClosestPointOnObject).Length2();
                    if (dist2 < smallestDist)
                    {
                        // The closest hit gets priority:

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

            /*
                We moved up to the wall and adjusted our movement,
                but we might encounter a new wall while moving along this one.
                Do another iteration to find out..
             */
        }
        else // unhindered movement
        {
            current_p += targetMovement;

            // Put it back on the ground, if any
            ground_p = PutOnGround (current_p, colliders, n_colliders, triangles, n_triangles, min_cosine, up);

            /*
                Distance to unground position must not be too large.
                Take target movement as benchmark.
             */
            if ((ground_p - current_p).Length2() < targetMovement.Length2())
            {
                return ground_p;
            }

            return current_p;
        }
    }
    // Don't go on iterating forever in corners!
    while ((targetMovement.Length2() > 1.0e-8f) && j < COLLISION_MAXITERATION);

    return current_p;
}

vec3 CollisionTraceBeam(const vec3 &p1, const vec3 &p2, const Triangle *triangles, const int n_triangles)
{
    if(p1 == p2)
        return p1; // otherwise we might get unexpected results

    vec3 p12 = p2 - p1, isect = p2, newisect;

    // For every triangle, see if the beam goes through:
    for(int i = 0; i < n_triangles; i++)
    {
        newisect = LinePlaneIntersectionDir (p1, p12, triangles[i].GetPlane());

        if (!PointInsideTriangle (triangles[i], newisect))
            continue;

        vec3 delta = newisect - p1;

        if (delta.Length2() < (p1 - isect).Length2() && Dot (delta, p12) > 0)

            isect = newisect;
    }
    return isect;
}
