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


#ifndef UTIL_H
#define UTIL_H

#ifndef M_PI
#define M_PI    3.14159265358979323846264338327950288   /* pi */
#endif

#include <stdlib.h>

/**
 * square function, x must be a number
 */
template <typename T>
T sqr (T x)
{
    return x * x;
}

/**
 * Swaps the values of n1 and n2
 */
template <class Value>
void swap (Value& n1, Value& n2)
{
    Value r=n1;
    n1=n2;
    n2=r ;
}

/**
 * Sets memory to zeros.
 */
void Zero(void *p, size_t size);

template <typename Type>
Type InterpolateCubic (const Type &p0, const Type &p1,
                       const Type &p2, const Type &p3, const float t)
{
    Type a = 0.5f * (p3 - p0 + 3 * (p1 - p2)),
         b = p0 - 0.5f * (5 * p1 + p3) + 2 * p2,
         c = 0.5f * (p2 - p0),
         d = p1;

    return t*t*t * a + t*t * b + t * c + d;
}

#endif // UTIL_H
