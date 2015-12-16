#ifndef BUFFER_H
#define BUFFER_H

#include <GL/glew.h>
#include <GL/gl.h>

struct VertexBuffer
{
    GLuint handle;
    size_t n_vertices;
};

struct IndexBuffer
{
    GLuint handle;
    size_t n_indices;
};

inline void glGenBuffer (VertexBuffer *p) { glGenBuffers (1, &p->handle); }
inline void glGenBuffer (IndexBuffer *p) { glGenBuffers (1, &p->handle); }
inline void glDeleteBuffer (VertexBuffer *p) { glDeleteBuffers (1, &p->handle); }
inline void glDeleteBuffer (IndexBuffer *p) { glDeleteBuffers (1, &p->handle); }

#endif // BUFFER_H
