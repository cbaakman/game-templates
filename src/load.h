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

#ifndef LOAD_H
#define LOAD_H

#include <functional>
#include <list>
#include "progress.h"

/**
 * These functions represent jobs to be done.
 * Must return 'true' one success or 'false' on failure.
 */
typedef std::function <bool (void)> LoadFunc;

/**
 * A Loadable object could represent a resource.
 *
 * A Loadable object may not be deleted between the calls
 * to 'Loader::Add' and 'Loader::LoadAll'
 */
class Loadable
{
public:
    virtual bool Load (void) = 0; // works like LoadFunc
};

/**
 * Split up the preparation of the scene in small units of work and
 * pass them on to the loader as functions or loadable objects.
 *
 * First add jobs to the loader with 'Add' and then call 'LoadAll'.
 */
class Loader
{
private:
    std::list <LoadFunc> toLoad;
public:
    void Add (LoadFunc);
    void Add (Loadable *);

    /**
     * If a progress object is given, loading progress will be reported to it.
     */
    bool LoadAll (Progress *pProgress=NULL);
};

#endif // LOAD_H
