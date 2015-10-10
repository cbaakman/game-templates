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

#ifndef VECS_H
#define VECS_H

#include "app.h"

#include "../font.h"

enum VecDisplayMode {DISPLAY_AXIS, DISPLAY_CROSS};

class VecScene : public App::Scene
{
private:

    // camera angles
    GLfloat angleY, angleX, distCamera;
    VecDisplayMode mode;

    float t;

    const Font *pFont;
public:

    VecScene (App *, const Font *);

    void Update (const float dt);
    void Render (void);
    void OnKeyPress(const SDL_KeyboardEvent *);
    void OnMouseMove (const SDL_MouseMotionEvent *);
    void OnMouseWheel (const SDL_MouseWheelEvent *);
};

#endif // VECS_H
