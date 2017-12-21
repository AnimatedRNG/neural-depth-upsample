#pragma once

#include <stdbool.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

typedef void* (*f_dlopen_t)(const char* filename, int flag);
typedef void* (*f_dlsym_t)(void*, const char*);
typedef void* (*f_dlvsym_t)(void*, const char*, const char*);

typedef void (*f_glx_swap_buffers_t)(Display* dpy, GLXDrawable drawable);
typedef void (*f_glx_ext_func_ptr_t)(void);
typedef Bool(*f_glx_make_current_t)(Display* dpy, GLXDrawable drawable,
                                    GLXContext ctx);

typedef void (*f_gl_viewport_t)(GLint x, GLint y, GLsizei width,
                                GLsizei height);
typedef void (*f_gl_bind_framebuffer_t)(GLenum target, GLuint framebuffer);
typedef void (*f_gl_draw_arrays_t)(GLenum mode, GLint first, GLsizei count);

typedef struct {
    bool init;
    bool init_GLX;
    bool init_GL;

    f_dlopen_t f_dlopen;
    f_dlsym_t f_dlsym;
    f_dlvsym_t f_dlvsym;

    void* libGL_handle;

    f_glx_swap_buffers_t __glXSwapBuffers;
    f_glx_ext_func_ptr_t(*__glXGetProcAddressARB)(const GLubyte*);
    f_glx_make_current_t __glXMakeCurrent;

    f_gl_viewport_t __glViewport;
    f_gl_bind_framebuffer_t __glBindFramebuffer;
} HOOKS;
