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


#include "random.h"

#include <stdlib.h>
#include <time.h>

void RandomSeed ()
{
    srand (time (NULL));
}

float RandomFloat(float _min, float _max)
{
    int n = 10000;
    float f = float(rand() % n) / n;

    // f is a random number between 0.0f and 1.0f

    return _min + f * (_max - _min);
}

float NoiseFrom (const int x)
{
    int y = (x << 13) ^ x;
    return (1.0 - ((y * (y * y * 15731 + 789221) + 1376312589) & 0x7fffffff)
            / 1073741824.0);
}
