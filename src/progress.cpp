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

#include <stdio.h>
#include <SDL2/SDL.h>

#include "progress.h"

// The mutexes must be locked for every usage of n_passed & n_total.
Progress::Progress ()
{
    pPassedMutex = SDL_CreateMutex ();
    if (!pPassedMutex)
        fprintf (stderr, "WARNING Couldn't create progress::passed mutex, %s\n", SDL_GetError ());

    pTotalMutex = SDL_CreateMutex ();
    if (!pTotalMutex)
        fprintf (stderr, "WARNING Couldn't create progress::total mutex, %s\n", SDL_GetError ());

    n_passed = n_total = 0;
}
Progress::~Progress ()
{
    SDL_DestroyMutex (pPassedMutex);
    SDL_DestroyMutex (pTotalMutex);
}
void Progress::AddTotal (const std::size_t n)
{
    if (SDL_LockMutex (pTotalMutex) == 0)
    {
        n_total += n;

        SDL_UnlockMutex (pTotalMutex);
    }
    else
        fprintf (stderr, "WARNING Couldn't lock progress::total mutex, %s\n", SDL_GetError ());
}
void Progress::AddPassed (const std::size_t n)
{
    if (SDL_LockMutex (pPassedMutex) == 0)
    {
        n_passed += n;

        SDL_UnlockMutex (pPassedMutex);
    }
    else
        fprintf (stderr, "WARNING Couldn't lock progress::passed mutex, %s\n", SDL_GetError ());
}
std::size_t Progress::GetPassed ()
{
    std::size_t n = 0;

    if (SDL_LockMutex (pPassedMutex) == 0)
    {
        n = n_passed;

        SDL_UnlockMutex (pPassedMutex);
    }
    else
        fprintf(stderr, "WARNING Couldn't lock progress::passed mutex, %s\n", SDL_GetError ());

    return n;
}
std::size_t Progress::GetTotal ()
{
    std::size_t n = 0;

    if (SDL_LockMutex (pTotalMutex) == 0)
    {
        n = n_total;

        SDL_UnlockMutex (pTotalMutex);
    }
    else
        fprintf(stderr, "WARNING Couldn't lock progress::total mutex, %s\n", SDL_GetError ());

    return n;
}
