/* gl_min.h — hand-written minimal OpenGL / GLX / X11 declarations.
 *
 * Copy of ~/workspace/shader-raw-demo/gl/gl_min.h, plus the two
 * uniform entry points the sky kernel needs (glUniform2fv,
 * glUniform1i). The original is untouched.
 *
 * This box has Mesa's libGL and Xvfb but no Khronos GL headers
 * installed, and no package route for them. So the harness declares
 * exactly the entry points it uses — nothing more. Every prototype
 * below matches the Khronos headers for that entry point.
 * GL functions are resolved at runtime through glXGetProcAddress;
 * the GLX and X11 entry points are resolved from libGL.so.1 and
 * libX11.so.6 by dlopen, so the binary links only -ldl.
 */
#ifndef GL_MIN_H
#define GL_MIN_H

#include <stddef.h>

/* --- GL types --- */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned long GLulong;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef long GLintptr;
typedef long GLsizeiptr;
typedef char GLchar;

/* --- GL constants used --- */
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RENDERER 0x1F01
#define GL_VENDOR 0x1F00
#define GL_VERSION 0x1F02
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84

/* --- GL function pointer types used --- */
typedef const GLubyte *(*PFNGLGETSTRINGPROC)(GLenum name);
typedef GLenum (*PFNGLGETERRORPROC)(void);
typedef void (*PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (*PFNGLCLEARPROC)(GLbitfield mask);
typedef void (*PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (*PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (*PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (*PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (*PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (*PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (*PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void (*PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (*PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETESHADERPROC)(GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (*PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (*PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (*PFNGLUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (*PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (*PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void (*PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
typedef void (*PFNGLFINISHPROC)(void);

/* --- X11 minimal types --- */
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef int Bool;
#ifndef True
#define True 1
#define False 0
#endif

/* --- GLX minimal types --- */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;
typedef XID GLXPbuffer;
typedef void (*__GLXextFuncPtr)(void);

#define GLX_RGBA_BIT 0x00000001
#define GLX_PBUFFER_BIT 0x00000004
#define GLX_DRAWABLE_TYPE 0x8010
#define GLX_RENDER_TYPE 0x8011
#define GLX_X_RENDERABLE 0x8012
#define GLX_RGBA_TYPE 0x8014
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_DEPTH_SIZE 12
#define GLX_PBUFFER_WIDTH 0x8041
#define GLX_PBUFFER_HEIGHT 0x8042
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

/* --- GLX function pointer types (resolved from libGL.so.1) --- */
typedef GLXFBConfig *(*PFNGLXCHOOSEFBCONFIGPROC)(Display *dpy, int screen, const int *attrib_list, int *nelements);
typedef GLXContext (*PFNGLXCREATENEWCONTEXTPROC)(Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct);
typedef GLXPbuffer (*PFNGLXCREATEPBUFFERPROC)(Display *dpy, GLXFBConfig config, const int *attrib_list);
typedef Bool (*PFNGLXMAKECONTEXTCURRENTPROC)(Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx);
typedef void (*PFNGLXDESTROYPBUFFERPROC)(Display *dpy, GLXPbuffer pbuf);
typedef void (*PFNGLXDESTROYCONTEXTPROC)(Display *dpy, GLXContext ctx);
typedef __GLXextFuncPtr (*PFNGLXGETPROCADDRESSPROC)(const GLubyte *procName);
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC)(Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);

/* --- X11 function pointer types (resolved from libX11.so.6) --- */
typedef Display *(*PFNXOPENDISPLAYPROC)(const char *display_name);
typedef int (*PFNXCLOSEDISPLAYPROC)(Display *display);
typedef int (*PFNXDEFAULTSCREENPROC)(Display *display);
typedef void (*PFNXFREEPROC)(void *data);

#endif /* GL_MIN_H */
