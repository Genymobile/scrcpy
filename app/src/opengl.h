#ifndef SC_OPENGL_H
#define SC_OPENGL_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL_opengl.h>

struct sc_opengl {
    const char *version;
    bool is_opengles;
    int version_major;
    int version_minor;

    const GLubyte *
    (*GetString)(GLenum name);

    void
    (*TexParameterf)(GLenum target, GLenum pname, GLfloat param);

    void
    (*TexParameteri)(GLenum target, GLenum pname, GLint param);

    void
    (*GenerateMipmap)(GLenum target);
};

void
sc_opengl_init(struct sc_opengl *gl);

bool
sc_opengl_version_at_least(struct sc_opengl *gl,
                           int minver_major, int minver_minor,
                           int minver_es_major, int minver_es_minor);

#endif
