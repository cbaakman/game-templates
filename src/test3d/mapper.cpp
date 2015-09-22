#include "mapper.h"

#include "../shader.h"
#include "../err.h"
#include "../io.h"

#include "../matrix.h"

#define VIEW_ANGLE 45.0f
#define NEAR_VIEW 0.1f
#define FAR_VIEW 1000.0f

const char

displace_vsh [] = R"shader(

    uniform float max_displace;

    uniform vec3 eye;

    attribute vec3 tangent,
                   bitangent;

    varying vec3 textureSpaceViewDirection;

    void main()
    {

        vec4 pos = gl_ModelViewMatrix * gl_Vertex;
        vec3 n = gl_NormalMatrix * gl_Normal,
             t = gl_NormalMatrix * tangent,
             b = gl_NormalMatrix * bitangent,

             // eye position is (0,0,0) here:
             view = (eye - gl_Vertex.xyz);

        textureSpaceViewDirection.x = dot (view, tangent);
        textureSpaceViewDirection.y = dot (view, bitangent);
        textureSpaceViewDirection.z = dot (view, gl_Normal) / length (view);

        gl_Position = ftransform ();
        gl_TexCoord [0] = gl_MultiTexCoord0;
    }

)shader",

displace_fsh [] = R"shader(

    uniform sampler2D tex_color,
                      tex_displace;

    uniform float max_displace;

    varying vec3 textureSpaceViewDirection;

    vec2 trace (float height, vec2 coords, vec3 viewDirection)
    {
        vec2 newCoords = coords;
        vec2 deltaCoords = -viewDirection.xy * height * 0.08;
        float searchHeight = 1.0;
        float prev_hits = 0.0;
        float hit_h = 0.0;

        for (int i = 0; i < 10; i ++)
        {
            searchHeight -= 0.1;
            newCoords += deltaCoords;
            float currentHeight = texture2D (tex_displace, newCoords).r;
            float first_hit = clamp ((currentHeight - searchHeight - prev_hits) * 499999.0, 0.0, 1.0);
            hit_h += first_hit * searchHeight;
            prev_hits += first_hit;
        }
        newCoords = coords + deltaCoords * (1.0 - hit_h) * 10.0f - deltaCoords;

        vec2 tempCoords = newCoords;
        searchHeight = hit_h + 0.1;
        float startHeight = searchHeight;
        deltaCoords *= 0.2;
        prev_hits = 0.0;
        hit_h = 0.0;
        for (int i = 0; i < 5; i ++)
        {
            searchHeight -= 0.02;
            newCoords += deltaCoords;
            float currentHeight = texture2D (tex_displace, newCoords).r;
            float first_hit = clamp ((currentHeight - searchHeight - prev_hits) * 499999.0,0.0,1.0);
            hit_h += first_hit * searchHeight;
            prev_hits += first_hit;
        }
        newCoords = tempCoords + deltaCoords * (startHeight - hit_h) * 50.0f;

        return newCoords;
    }

    void main()
    {
        /*
            Unfortunately, there's a slight measurement error in the view angle.
            The closer the fragment gets to the camera, the more visible this error becomes.
         */
        vec3 viewDirection = normalize (textureSpaceViewDirection);

        vec2 texcoords = trace (max_displace, gl_TexCoord [0].st, viewDirection);

        gl_FragColor = texture2D (tex_color, texcoords);
    }
)shader";

MapperScene::MapperScene (App *pApp) : Scene (pApp),
    angleY(0), angleX(0), distCamera(3.0f),

    shaderProgram (0)
{

    texColor.tex = texDisplace.tex = 0;
}
MapperScene::~MapperScene ()
{
    glDeleteTextures (1, &texColor.tex);
    glDeleteTextures (1, &texDisplace.tex);

    glDeleteProgram (shaderProgram);
}
bool MapperScene::Init (void)
{
    std::string resPath = std::string(SDL_GetBasePath()) + "test3d.zip";

    SDL_RWops *f;
    bool success;

    // Load color texture
    f = SDL_RWFromZipArchive (resPath.c_str(), "granite.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texColor);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing granite.png: %s", GetError ());
        return false;
    }

    // Load displace texture
    f = SDL_RWFromZipArchive (resPath.c_str(), "granite_d.png");
    if (!f)
        return false;
    success = LoadPNG(f, &texDisplace);
    f->close(f);

    if (!success)
    {
        SetError ("error parsing granite_d.png: %s", GetError ());
        return false;
    }

    // Create shader from sources:
    shaderProgram = CreateShaderProgram (displace_vsh, displace_fsh);
    if (!shaderProgram)
    {
        SetError ("error creating mapper shader program: %s", GetError ());
        return false;
    }


    // Pick tangent and bitangent index:
    GLint max_attribs;
    glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &max_attribs);

    index_tangent = max_attribs - 2;
    index_bitangent = max_attribs - 1;


    return true;
}
void MapperScene::Render (void)
{
    GLint loc;

    glClearColor (0, 0, 0, 1.0);
    glClear (GL_COLOR_BUFFER_BIT);

    int w, h;
    SDL_GL_GetDrawableSize (pApp->GetMainWindow (), &w, &h);


    // Set perspective 3d projection matrix:
    glMatrixMode (GL_PROJECTION);
    matrix4 matCamera = matPerspec (VIEW_ANGLE, (GLfloat) w / (GLfloat) h, NEAR_VIEW, FAR_VIEW);
    glLoadMatrixf (matCamera.m);

    // Set the model matrix to camera view:
    glMatrixMode (GL_MODELVIEW);
    vec3 posCamera = matRotY (-angleY) * matRotX (-angleX) * vec3 (0, 0, -distCamera);
    const matrix4 matCameraView = matLookAt (posCamera, VEC_O, VEC_UP);
    glLoadMatrixf (matCameraView.m);

    glEnable (GL_TEXTURE_2D);
    glUseProgram (shaderProgram);

    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texColor.tex);

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, texDisplace.tex);

    loc = glGetUniformLocation (shaderProgram, "tex_color");
    glUniform1i (loc, 0);

    loc = glGetUniformLocation (shaderProgram, "tex_displace");
    glUniform1i (loc, 1);

    loc = glGetUniformLocation (shaderProgram, "max_displace");
    glUniform1f (loc, 0.05);

    loc = glGetUniformLocation (shaderProgram, "eye");
    glUniform3fv (loc, 1, posCamera.v);

    glBindAttribLocation (shaderProgram, index_tangent, "tangent");
    glBindAttribLocation (shaderProgram, index_bitangent, "bitangent");

    glEnable (GL_CULL_FACE);
    glFrontFace (GL_CCW);
    glCullFace (GL_FRONT);

    glBegin (GL_QUADS);

    glTexCoord2f (0.0f, 1.0f);
    glVertexAttrib3f (index_tangent, 1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, -1.0f);
    glVertex3f (-1.0f, 1.0f, 0.0f);

    glTexCoord2f (1.0f, 1.0f);
    glVertexAttrib3f (index_tangent, 1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, -1.0f);
    glVertex3f (1.0f, 1.0f, 0.0f);

    glTexCoord2f (1.0f, 0.0f);
    glVertexAttrib3f (index_tangent, 1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, -1.0f);
    glVertex3f (1.0f, -1.0f, 0.0f);

    glTexCoord2f (0.0f, 0.0f);
    glVertexAttrib3f (index_tangent, 1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, -1.0f);
    glVertex3f (-1.0f, -1.0f, 0.0f);

    // The Back has normals and tangents 180 degrees twisted.

    glTexCoord2f (1.0f, 1.0f);
    glVertexAttrib3f (index_tangent, -1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, 1.0f);
    glVertex3f (1.0f, 1.0f, 0.0f);

    glTexCoord2f (0.0f, 1.0f);
    glVertexAttrib3f (index_tangent, -1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, 1.0f);
    glVertex3f (-1.0f, 1.0f, 0.0f);

    glTexCoord2f (0.0f, 0.0f);
    glVertexAttrib3f (index_tangent, -1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, 1.0f);
    glVertex3f (-1.0f, -1.0f, 0.0f);

    glTexCoord2f (1.0f, 0.0f);
    glVertexAttrib3f (index_tangent, -1.0f, 0.0f, 0.0f);
    glVertexAttrib3f (index_bitangent, 0.0f, 1.0f, 0.0f);
    glNormal3f (0.0f, 0.0f, 1.0f);
    glVertex3f (1.0f, -1.0f, 0.0f);

    glEnd ();

    glUseProgram (NULL);
}
void MapperScene::OnMouseMove (const SDL_MouseMotionEvent *event)
{
    if(event -> state & SDL_BUTTON_LMASK)
    {
        // Change camera angles

        angleY -= 0.01f * event -> xrel;
        angleX -= 0.01f * event -> yrel;

        if (angleX > 1.5f)
            angleX = 1.5f;
        if (angleX < -1.5f)
            angleX = -1.5f;
    }
}
void MapperScene::OnMouseWheel (const SDL_MouseWheelEvent *event)
{
    // zoom in or out

    distCamera -= 0.3f * event->y;

    if (distCamera < 0.5f)
        distCamera = 0.5f;
}
