// Minimal OpenGL 4.6 function loader for the Kerr Black Hole Navigator.
// Loads only the entry points this project actually uses, via the loader
// callback supplied by GLFW (glfwGetProcAddress). Keeps the project free
// of an external glad dependency.

#ifndef KERR_GL_LOADER_H
#define KERR_GL_LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifndef APIENTRY
#  if defined(_WIN32)
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif
#endif

// ----- Base GL types ---------------------------------------------------------
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef char           GLchar;
typedef ptrdiff_t      GLintptr;
typedef ptrdiff_t      GLsizeiptr;

#define GL_FALSE 0
#define GL_TRUE  1
#define GL_NO_ERROR 0
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_LINES 0x0001
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_RGBA8 0x8058
#define GL_RGBA16F 0x881A
#define GL_RGBA32F 0x8814
#define GL_RGB16F 0x881B
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_ONLY  0x88B8
#define GL_READ_WRITE 0x88BA
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#define GL_FRAMEBUFFER_BARRIER_BIT 0x00000400
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_VIEWPORT 0x0BA2
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*KerrGLLoaderFn)(const char* name);
int kerrLoadGL(KerrGLLoaderFn loader);

// ----- Function pointer table -----------------------------------------------
#define KERR_GL_FUNCS(X) \
    X(void,    Viewport,            (GLint x, GLint y, GLsizei w, GLsizei h)) \
    X(void,    ClearColor,          (GLclampf r, GLclampf g, GLclampf b, GLclampf a)) \
    X(void,    Clear,               (GLbitfield mask)) \
    X(void,    Enable,              (GLenum cap)) \
    X(void,    Disable,             (GLenum cap)) \
    X(void,    BlendFunc,           (GLenum sf, GLenum df)) \
    X(void,    DrawArrays,          (GLenum mode, GLint first, GLsizei count)) \
    X(void,    GetIntegerv,         (GLenum pname, GLint* p)) \
    X(void,    ReadPixels,          (GLint x, GLint y, GLsizei w, GLsizei h, GLenum fmt, GLenum type, void* data)) \
    X(void,    PixelStorei,         (GLenum pname, GLint param)) \
    X(GLenum,  GetError,            (void)) \
    X(const GLubyte*, GetString,    (GLenum name)) \
    /* Buffers / VAOs */ \
    X(void,    GenBuffers,          (GLsizei n, GLuint* p)) \
    X(void,    BindBuffer,          (GLenum t, GLuint b)) \
    X(void,    BufferData,          (GLenum t, GLsizeiptr s, const void* d, GLenum u)) \
    X(void,    DeleteBuffers,       (GLsizei n, const GLuint* p)) \
    X(void,    GenVertexArrays,     (GLsizei n, GLuint* p)) \
    X(void,    BindVertexArray,     (GLuint a)) \
    X(void,    DeleteVertexArrays,  (GLsizei n, const GLuint* p)) \
    X(void,    VertexAttribPointer, (GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p)) \
    X(void,    EnableVertexAttribArray, (GLuint i)) \
    /* Shaders */ \
    X(GLuint,  CreateShader,        (GLenum t)) \
    X(void,    ShaderSource,        (GLuint s, GLsizei n, const GLchar* const* str, const GLint* l)) \
    X(void,    CompileShader,       (GLuint s)) \
    X(void,    GetShaderiv,         (GLuint s, GLenum p, GLint* v)) \
    X(void,    GetShaderInfoLog,    (GLuint s, GLsizei b, GLsizei* l, GLchar* log)) \
    X(void,    DeleteShader,        (GLuint s)) \
    X(GLuint,  CreateProgram,       (void)) \
    X(void,    AttachShader,        (GLuint p, GLuint s)) \
    X(void,    LinkProgram,         (GLuint p)) \
    X(void,    GetProgramiv,        (GLuint p, GLenum n, GLint* v)) \
    X(void,    GetProgramInfoLog,   (GLuint p, GLsizei b, GLsizei* l, GLchar* log)) \
    X(void,    UseProgram,          (GLuint p)) \
    X(void,    DeleteProgram,       (GLuint p)) \
    X(GLint,   GetUniformLocation,  (GLuint p, const GLchar* n)) \
    X(void,    Uniform1i,           (GLint l, GLint v)) \
    X(void,    Uniform1f,           (GLint l, GLfloat v)) \
    X(void,    Uniform2f,           (GLint l, GLfloat a, GLfloat b)) \
    X(void,    Uniform3f,           (GLint l, GLfloat a, GLfloat b, GLfloat c)) \
    X(void,    Uniform4f,           (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d)) \
    X(void,    UniformMatrix4fv,    (GLint l, GLsizei c, GLboolean t, const GLfloat* v)) \
    /* Textures */ \
    X(void,    GenTextures,         (GLsizei n, GLuint* p)) \
    X(void,    BindTexture,         (GLenum t, GLuint x)) \
    X(void,    TexImage2D,          (GLenum t, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum tp, const void* d)) \
    X(void,    TexStorage2D,        (GLenum t, GLsizei lvl, GLenum ifmt, GLsizei w, GLsizei h)) \
    X(void,    TexParameteri,       (GLenum t, GLenum p, GLint v)) \
    X(void,    ActiveTexture,       (GLenum u)) \
    X(void,    DeleteTextures,      (GLsizei n, const GLuint* p)) \
    X(void,    GenerateMipmap,      (GLenum t)) \
    /* Compute / images */ \
    X(void,    DispatchCompute,     (GLuint x, GLuint y, GLuint z)) \
    X(void,    MemoryBarrier,       (GLbitfield b)) \
    X(void,    BindImageTexture,    (GLuint u, GLuint tex, GLint lvl, GLboolean lay, GLint l, GLenum a, GLenum f)) \
    /* Debug */ \
    X(void,    DebugMessageCallback, (void* cb, const void* p))

#define KERR_GL_DECL(ret, name, args) typedef ret (APIENTRY *PFN_gl##name) args; extern PFN_gl##name gl##name;
KERR_GL_FUNCS(KERR_GL_DECL)
#undef KERR_GL_DECL

#ifdef __cplusplus
}
#endif
#endif
