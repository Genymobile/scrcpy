#include "opengl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

void
sc_opengl_init(struct sc_opengl *gl) {
    gl->GetString = (const GLubyte *(*)(GLenum))
                    SDL_GL_GetProcAddress("glGetString");
    assert(gl->GetString);

    gl->BindTexture = (void (*)(GLenum, GLuint))
                      SDL_GL_GetProcAddress("glBindTexture");
    assert(gl->BindTexture);

    gl->TexParameterf = (void (*)(GLenum, GLenum, GLfloat))
                        SDL_GL_GetProcAddress("glTexParameterf");
    assert(gl->TexParameterf);

    gl->TexParameteri = (void (*)(GLenum, GLenum, GLint))
                        SDL_GL_GetProcAddress("glTexParameteri");
    assert(gl->TexParameteri);

    // optional
    gl->GenerateMipmap = (void (*)(GLenum))
                         SDL_GL_GetProcAddress("glGenerateMipmap");

    const char *version = (const char *) gl->GetString(GL_VERSION);
    assert(version);
    gl->version = version;

#define OPENGL_ES_PREFIX "OpenGL ES "
    /* starts with "OpenGL ES " */
    gl->is_opengles = !strncmp(gl->version, OPENGL_ES_PREFIX,
                               sizeof(OPENGL_ES_PREFIX) - 1);
    if (gl->is_opengles) {
        /* skip the prefix */
        version += sizeof(OPENGL_ES_PREFIX) - 1;
    }

    int r = sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);
    if (r != 2) {
        // failed to parse the version
        gl->version_major = 0;
        gl->version_minor = 0;
    }
}

bool
sc_opengl_version_at_least(struct sc_opengl *gl,
                           int minver_major, int minver_minor,
                           int minver_es_major, int minver_es_minor)
{
    if (gl->is_opengles) {
        return gl->version_major > minver_es_major
            || (gl->version_major == minver_es_major
             && gl->version_minor >= minver_es_minor);
    }

    return gl->version_major > minver_major
        || (gl->version_major == minver_major
         && gl->version_minor >= minver_minor);
}

void
sc_opengl_enable_mipmaps(struct sc_opengl *gl, uint32_t *tex_ids, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        assert(tex_ids[i]);
        gl->BindTexture(GL_TEXTURE_2D, tex_ids[i]);

        gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                         GL_LINEAR_MIPMAP_LINEAR);
        if (!gl->is_opengles) {
            // GL_TEXTURE_LOD_BIAS is not available in OpenGL ES
            gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.f);
        }
    }

    gl->BindTexture(GL_TEXTURE_2D, 0);
}

void
sc_opengl_generate_mipmaps(struct sc_opengl *gl, uint32_t *tex_ids, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        assert(tex_ids[i]);
        gl->BindTexture(GL_TEXTURE_2D, tex_ids[i]);
        gl->GenerateMipmap(GL_TEXTURE_2D);
    }

    gl->BindTexture(GL_TEXTURE_2D, 0);
}
