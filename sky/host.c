/* host.c — stripped-down OpenGL host.
 *
 * Copy of ~/workspace/shader-raw-demo/gl/host.c; the only change is
 * the optional sky data file (uSun / uEarth / uCount uniforms, see
 * usage below). The original is untouched.
 *
 * No window, no engine, no loader library. The whole pipeline is:
 *   X display (Xvfb) -> GLX pbuffer context -> compile the two shaders
 *   from files -> fullscreen triangle -> FBO at the requested resolution
 *   -> glReadPixels -> PPM file.
 *
 * Usage:
 *   host <kernel.frag> <width> <height> <out.ppm> [time] [scale] [vert] [data]
 *     time  : uTime uniform, default 0.0
 *     scale : uScale uniform, default 1.0 (the resolution kernel's
 *             internal-resolution divisor)
 *     vert  : vertex shader path, default passthrough.vert
 *     data  : optional sky data file — lines "S <az> <el>" and
 *             "E <az> <el>" in degrees; uploaded as the uSun / uEarth
 *             vec2 arrays and uCount. Kernels that do not declare
 *             those uniforms simply ignore the file.
 *
 * Rendered with Mesa llvmpipe (software) under Xvfb; set
 * LIBGL_ALWAYS_SOFTWARE=1 to force it. Exit 0 on a clean render,
 * exit 1 with the shader info log on any compile/link failure.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "gl_min.h"

/* --- entry points resolved from libGL.so.1 / libX11.so.6 ---
 * The binary links only -ldl: every graphics call below is resolved
 * by name at startup, and a missing symbol fails loudly naming it. */
static PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfig;
static PFNGLXCREATENEWCONTEXTPROC glXCreateNewContext;
static PFNGLXCREATEPBUFFERPROC glXCreatePbuffer;
static PFNGLXMAKECONTEXTCURRENTPROC glXMakeContextCurrent;
static PFNGLXDESTROYPBUFFERPROC glXDestroyPbuffer;
static PFNGLXDESTROYCONTEXTPROC glXDestroyContext;
static PFNGLXGETPROCADDRESSPROC glXGetProcAddress;
static PFNXOPENDISPLAYPROC XOpenDisplay;
static PFNXCLOSEDISPLAYPROC XCloseDisplay;
static PFNXDEFAULTSCREENPROC XDefaultScreen;
static PFNXFREEPROC XFree;

static void *load_lib_sym(void *handle, const char *name)
{
    void *p = dlsym(handle, name);
    if (!p) {
        fprintf(stderr, "host: missing symbol %s\n", name);
        exit(1);
    }
    return p;
}

#define LIBSYM(handle, fn) do { void *p_ = load_lib_sym(handle, #fn); memcpy(&fn, &p_, sizeof p_); } while (0)

static void load_libs(void)
{
    void *gl = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!gl) {
        fprintf(stderr, "host: cannot open libGL.so.1: %s\n", dlerror());
        exit(1);
    }
    void *x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!x11) {
        fprintf(stderr, "host: cannot open libX11.so.6: %s\n", dlerror());
        exit(1);
    }
    LIBSYM(gl, glXChooseFBConfig);
    LIBSYM(gl, glXCreateNewContext);
    LIBSYM(gl, glXCreatePbuffer);
    LIBSYM(gl, glXMakeContextCurrent);
    LIBSYM(gl, glXDestroyPbuffer);
    LIBSYM(gl, glXDestroyContext);
    LIBSYM(gl, glXGetProcAddress);
    LIBSYM(x11, XOpenDisplay);
    LIBSYM(x11, XCloseDisplay);
    LIBSYM(x11, XDefaultScreen);
    LIBSYM(x11, XFree);
}

/* --- resolved GL entry points --- */
static PFNGLGETSTRINGPROC glGetString;
static PFNGLGETERRORPROC glGetError;
static PFNGLCLEARCOLORPROC glClearColor;
static PFNGLCLEARPROC glClear;
static PFNGLVIEWPORTPROC glViewport;
static PFNGLGENTEXTURESPROC glGenTextures;
static PFNGLDELETETEXTURESPROC glDeleteTextures;
static PFNGLBINDTEXTUREPROC glBindTexture;
static PFNGLTEXIMAGE2DPROC glTexImage2D;
static PFNGLTEXPARAMETERIPROC glTexParameteri;
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLGETSHADERIVPROC glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLGETPROGRAMIVPROC glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
static PFNGLDELETEPROGRAMPROC glDeleteProgram;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM2FPROC glUniform2f;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM2FVPROC glUniform2fv;
static PFNGLUNIFORM1IPROC glUniform1i;
static PFNGLDRAWARRAYSPROC glDrawArrays;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
static PFNGLREADPIXELSPROC glReadPixels;
static PFNGLFINISHPROC glFinish;

static void *load_proc(const char *name)
{
    void *p = (void *)glXGetProcAddress((const GLubyte *)name);
    if (!p) {
        fprintf(stderr, "host: missing GL entry point %s\n", name);
        exit(1);
    }
    return p;
}

#define LOAD(fn) do { void *p_ = load_proc(#fn); memcpy(&fn, &p_, sizeof p_); } while (0)

static void load_gl(void)
{
    LOAD(glGetString);
    LOAD(glGetError);
    LOAD(glClearColor);
    LOAD(glClear);
    LOAD(glViewport);
    LOAD(glGenTextures);
    LOAD(glDeleteTextures);
    LOAD(glBindTexture);
    LOAD(glTexImage2D);
    LOAD(glTexParameteri);
    LOAD(glGenFramebuffers);
    LOAD(glDeleteFramebuffers);
    LOAD(glBindFramebuffer);
    LOAD(glFramebufferTexture2D);
    LOAD(glCheckFramebufferStatus);
    LOAD(glCreateShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glGetShaderiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glDeleteShader);
    LOAD(glCreateProgram);
    LOAD(glAttachShader);
    LOAD(glLinkProgram);
    LOAD(glGetProgramiv);
    LOAD(glGetProgramInfoLog);
    LOAD(glDeleteProgram);
    LOAD(glUseProgram);
    LOAD(glGetUniformLocation);
    LOAD(glUniform2f);
    LOAD(glUniform1f);
    LOAD(glUniform2fv);
    LOAD(glUniform1i);
    LOAD(glDrawArrays);
    LOAD(glGenVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glDeleteVertexArrays);
    LOAD(glReadPixels);
    LOAD(glFinish);
}

static char *read_file(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "host: cannot open %s\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "host: cannot seek %s\n", path);
        exit(1);
    }
    long len = ftell(f);
    if (len < 0) {
        fprintf(stderr, "host: cannot size %s\n", path);
        exit(1);
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fprintf(stderr, "host: out of memory\n");
        exit(1);
    }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "host: cannot read %s\n", path);
        exit(1);
    }
    fclose(f);
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

static GLuint compile_shader(GLenum type, const char *path)
{
    char *src = read_file(path, NULL);
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, (const GLchar *const *)&src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint loglen = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &loglen);
        char *log = (char *)malloc((size_t)(loglen > 0 ? loglen : 1));
        glGetShaderInfoLog(sh, loglen, NULL, log);
        fprintf(stderr, "host: %s failed to compile:\n%s\n", path, log);
        free(log);
        free(src);
        exit(1);
    }
    free(src);
    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint loglen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &loglen);
        char *log = (char *)malloc((size_t)(loglen > 0 ? loglen : 1));
        glGetProgramInfoLog(prog, loglen, NULL, log);
        fprintf(stderr, "host: program failed to link:\n%s\n", log);
        free(log);
        exit(1);
    }
    return prog;
}

static void check_gl(const char *where)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "host: GL error 0x%04x after %s\n", err, where);
        exit(1);
    }
}

static void write_ppm(const char *path, int w, int h, const unsigned char *rgba)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "host: cannot open %s for writing\n", path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    /* glReadPixels is bottom-up; PPM is top-down: flip rows. */
    for (int y = h - 1; y >= 0; y--) {
        const unsigned char *row = rgba + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            if (fwrite(row + (size_t)x * 4, 1, 3, f) != 3) {
                fprintf(stderr, "host: write failed on %s\n", path);
                exit(1);
            }
        }
    }
    fclose(f);
}

/* Sky data file: lines "S <az> <el>" / "E <az> <el>" in degrees.
 * Returns the shared sample count, or 0 when no file was given.
 * Sun and Earth counts must agree; more than 64 samples is an error. */
#define MAX_SKY_SAMPLES 64
static int load_sky_data(const char *path, GLfloat *sun, GLfloat *earth)
{
    if (!path) return 0;
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "host: cannot open sky data %s\n", path);
        exit(1);
    }
    int nsun = 0, nearth = 0;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char tag = 0;
        double az = 0.0, el = 0.0;
        if (sscanf(line, " %c %lf %lf", &tag, &az, &el) != 3) {
            fprintf(stderr, "host: bad sky data line: %s\n", line);
            exit(1);
        }
        if (tag == 'S') {
            if (nsun >= MAX_SKY_SAMPLES) { fprintf(stderr, "host: too many Sun samples\n"); exit(1); }
            sun[nsun * 2] = (GLfloat)az; sun[nsun * 2 + 1] = (GLfloat)el; nsun++;
        } else if (tag == 'E') {
            if (nearth >= MAX_SKY_SAMPLES) { fprintf(stderr, "host: too many Earth samples\n"); exit(1); }
            earth[nearth * 2] = (GLfloat)az; earth[nearth * 2 + 1] = (GLfloat)el; nearth++;
        } else {
            fprintf(stderr, "host: bad sky data tag '%c'\n", tag);
            exit(1);
        }
    }
    fclose(f);
    if (nsun != nearth || nsun == 0) {
        fprintf(stderr, "host: sky data has %d Sun and %d Earth samples\n", nsun, nearth);
        exit(1);
    }
    return nsun;
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s <kernel.frag> <width> <height> <out.ppm> [time] [scale] [vert] [data]\n", argv[0]);
        return 1;
    }
    const char *frag_path = argv[1];
    int width = atoi(argv[2]);
    int height = atoi(argv[3]);
    const char *out_path = argv[4];
    float time = argc > 5 ? (float)strtod(argv[5], NULL) : 0.0f;
    float scale = argc > 6 ? (float)strtod(argv[6], NULL) : 1.0f;
    const char *vert_path = argc > 7 ? argv[7] : "passthrough.vert";
    const char *data_path = argc > 8 ? argv[8] : NULL;
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "host: width and height must be positive\n");
        return 1;
    }

    /* --- display + context (GLX pbuffer, no window) --- */
    load_libs();
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "host: cannot open X display (run under xvfb-run)\n");
        return 1;
    }
    int screen = XDefaultScreen(dpy);
    const int fb_attribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        0
    };
    int ncfg = 0;
    GLXFBConfig *cfgs = glXChooseFBConfig(dpy, screen, fb_attribs, &ncfg);
    if (!cfgs || ncfg == 0) {
        fprintf(stderr, "host: no matching FBConfig\n");
        return 1;
    }
    GLXFBConfig cfg = cfgs[0];

    /* Prefer a 3.3 core context; fall back to the legacy constructor. */
    GLXContext ctx = NULL;
    PFNGLXCREATECONTEXTATTRIBSARBPROC create_attribs =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    if (create_attribs) {
        const int ctx_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        ctx = create_attribs(dpy, cfg, NULL, True, ctx_attribs);
    }
    if (!ctx) {
        ctx = glXCreateNewContext(dpy, cfg, GLX_RGBA_TYPE, NULL, True);
    }
    if (!ctx) {
        fprintf(stderr, "host: cannot create GL context\n");
        return 1;
    }
    const int pb_attribs[] = { GLX_PBUFFER_WIDTH, 16, GLX_PBUFFER_HEIGHT, 16, 0 };
    GLXPbuffer pbuf = glXCreatePbuffer(dpy, cfg, pb_attribs);
    if (!pbuf || !glXMakeContextCurrent(dpy, pbuf, pbuf, ctx)) {
        fprintf(stderr, "host: cannot make context current\n");
        return 1;
    }
    XFree(cfgs);

    load_gl();
    printf("renderer: %s\n", glGetString(GL_RENDERER));
    printf("version: %s\n", glGetString(GL_VERSION));
    printf("vendor: %s\n", glGetString(GL_VENDOR));

    /* --- shaders --- */
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_path);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_path);
    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    /* --- FBO at the requested resolution --- */
    GLuint tex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "host: FBO incomplete\n");
        return 1;
    }

    /* Core profile requires a VAO bound for any draw, even though the
     * fullscreen triangle needs no vertex attributes. */
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    /* --- render the fullscreen triangle --- */
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    GLint loc = glGetUniformLocation(prog, "uResolution");
    if (loc >= 0) glUniform2f(loc, (GLfloat)width, (GLfloat)height);
    loc = glGetUniformLocation(prog, "uTime");
    if (loc >= 0) glUniform1f(loc, time);
    loc = glGetUniformLocation(prog, "uScale");
    if (loc >= 0) glUniform1f(loc, scale);
    /* Sky data: the arrays are only uploaded (and only looked up)
     * when a data file was given; other kernels never notice. */
    GLfloat sun[MAX_SKY_SAMPLES * 2], earth[MAX_SKY_SAMPLES * 2];
    int nsky = load_sky_data(data_path, sun, earth);
    if (nsky > 0) {
        loc = glGetUniformLocation(prog, "uSun");
        if (loc >= 0) glUniform2fv(loc, nsky, sun);
        loc = glGetUniformLocation(prog, "uEarth");
        if (loc >= 0) glUniform2fv(loc, nsky, earth);
        loc = glGetUniformLocation(prog, "uCount");
        if (loc >= 0) glUniform1i(loc, nsky);
        printf("sky data: %d samples\n", nsky);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    check_gl("draw");

    unsigned char *pixels = (unsigned char *)malloc((size_t)width * height * 4);
    if (!pixels) {
        fprintf(stderr, "host: out of memory\n");
        return 1;
    }
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    check_gl("readback");
    write_ppm(out_path, width, height, pixels);
    printf("wrote %s (%dx%d)\n", out_path, width, height);

    free(pixels);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
    glXDestroyPbuffer(dpy, pbuf);
    glXDestroyContext(dpy, ctx);
    XCloseDisplay(dpy);
    return 0;
}
