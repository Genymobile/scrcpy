#include "opengl.h"

#include <assert.h>
#include <stdio.h>
#include "SDL2/SDL.h"

void
sc_opengl_init(struct sc_opengl *gl) {
    gl->GetString = SDL_GL_GetProcAddress("glGetString");
    assert(gl->GetString);

    gl->TexParameterf = SDL_GL_GetProcAddress("glTexParameterf");
    assert(gl->TexParameterf);

    gl->TexParameteri = SDL_GL_GetProcAddress("glTexParameteri");
    assert(gl->TexParameteri);

    // optional
    gl->GenerateMipmap = SDL_GL_GetProcAddress("glGenerateMipmap");

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
