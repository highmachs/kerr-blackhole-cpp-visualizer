// Implementation of the minimal GL loader.
#include "glad/glad.h"
#include <string.h>

#define KERR_GL_DEFN(ret, name, args) PFN_gl##name gl##name = 0;
KERR_GL_FUNCS(KERR_GL_DEFN)
#undef KERR_GL_DEFN

int kerrLoadGL(KerrGLLoaderFn loader) {
    if (!loader) return 0;
    int missing = 0;
#define KERR_GL_LOAD(ret, name, args) \
    gl##name = (PFN_gl##name)loader("gl" #name); \
    if (!gl##name) missing++;
    KERR_GL_FUNCS(KERR_GL_LOAD)
#undef KERR_GL_LOAD
    // A few entry points (DebugMessageCallback) are allowed to be missing.
    // Treat the load as successful if the core ones we always need are there.
    return (glClear && glDrawArrays && glCreateShader && glDispatchCompute) ? 1 : 0;
}
