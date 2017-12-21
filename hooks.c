#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include "elfhacks.h"
#include "hooks_dict.h"
#include "capture_framebuffer.h"

#define __PUBLIC __attribute__ ((visibility ("default")))

FBO fbo;
HOOKS hooks;

void get_real_dlsym(f_dlopen_t* f_dlopen,
                    f_dlsym_t* f_dlsym,
                    f_dlvsym_t* f_dlvsym) {
    eh_obj_t libdl;

    if (eh_find_obj(&libdl, "*libdl.so*")) {
        fprintf(stderr, "(glc) libdl.so is not present in memory\n");
        exit(1);
    }

    if (eh_find_sym(&libdl, "dlopen", (void*) f_dlopen)) {
        fprintf(stderr, "(glc) can't get real dlopen()\n");
        exit(1);
    }

    if (eh_find_sym(&libdl, "dlsym", (void*) f_dlsym)) {
        fprintf(stderr, "(glc) can't get real dlsym()\n");
        exit(1);
    }

    if (eh_find_sym(&libdl, "dlvsym", (void*) f_dlvsym)) {
        fprintf(stderr, "(glc) can't get real dlvsym()\n");
        exit(1);
    }

    eh_destroy_obj(&libdl);
}

void init_hook_info(const bool need_glx_calls, const bool need_gl_calls) {
    hooks.init = true;
    get_real_dlsym(&(hooks.f_dlopen), &(hooks.f_dlsym), &(hooks.f_dlvsym));

    if (need_glx_calls) {
        hooks.init_GLX = true;
        hooks.libGL_handle = hooks.f_dlopen("libGL.so.1", RTLD_LAZY);

        hooks.__glXSwapBuffers = (f_glx_swap_buffers_t) hooks.f_dlsym(
                                     hooks.libGL_handle, "glXSwapBuffers");

        hooks.__glXGetProcAddressARB = (f_glx_ext_func_ptr_t(*)(const GLubyte*))
                                       hooks.f_dlsym(hooks.libGL_handle,
                                               "glXGetProcAddressARB");

        hooks.__glXMakeCurrent = (f_glx_make_current_t) hooks.f_dlsym(
                                     hooks.libGL_handle, "glXMakeCurrent");
    }

    if (need_gl_calls) {
        hooks.init_GL = true;
        hooks.__glViewport = (f_gl_viewport_t) hooks.f_dlsym(
                                 hooks.libGL_handle, "glViewport");
    }
}

void before_swap_buffers(f_glx_swap_buffers_t swap_buffers,
                         Display* dpy,
                         GLXDrawable drawable) {
    printf("Before swap buffers\n");
    unbind_fbo(hooks, fbo);
    swap_buffers(dpy, drawable);
    bind_fbo(hooks, fbo);
    printf("After swap buffers\n");
}

void after_make_current() {
    printf("Just made current\n");
    fbo = init_fbo(hooks, 1920, 1080);
    bind_fbo(hooks, fbo);
}

__PUBLIC void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }
    before_swap_buffers(hooks.__glXSwapBuffers, dpy, drawable);
}

__PUBLIC f_glx_ext_func_ptr_t glXGetProcAddressARB(const GLubyte* proc_name) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }

    printf("Calling glXGetProcAddressARB\n");

    if (!strcmp((char*) proc_name, "glXSwapBuffers")) {
        return (f_glx_ext_func_ptr_t) &glXSwapBuffers;
    } else if (!strcmp((char*) proc_name, "glXGetProcAddressARB")) {
        return (f_glx_ext_func_ptr_t) &glXGetProcAddressARB;
    } else {
        return hooks.__glXGetProcAddressARB(proc_name);
    }
}

__PUBLIC Bool glXMakeCurrent(Display* dpy, GLXDrawable drawable,
                             GLXContext ctx) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }
    Bool ret = hooks.__glXMakeCurrent(dpy, drawable, ctx);
    after_make_current();
    return ret;
}

__PUBLIC void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    printf("Trying to change the viewport to %ix%i\n", width, height);
    fbo.previous_viewport[0] = x;
    fbo.previous_viewport[1] = y;
    fbo.previous_viewport[2] = width;
    fbo.previous_viewport[3] = height;
    hooks.__glViewport(x, y, width, height);
}

void* get_wrapped_func(const char* symbol) {
    if (!strcmp(symbol, "glXSwapBuffers"))
        return (void*) &glXSwapBuffers;
    else if (!strcmp(symbol, "glXGetProcAddressARB"))
        return (void*) &glXGetProcAddressARB;
    else if (!strcmp(symbol, "glXMakeCurrent"))
        return (void*) &glXMakeCurrent;
    else if (!strcmp(symbol, "glViewport"))
        return (void*) &glViewport;
    else
        return NULL;
}

void* dlsym(void* handle, const char* symbol) {
    if (!hooks.init) {
        init_hook_info(false, false);
    }

    void* ret = get_wrapped_func(symbol);
    if (ret == NULL) {
        return hooks.f_dlsym(handle, symbol);
    } else {
        return ret;
    }
}

void* dlvsym(void* handle, const char* symbol, const char* version) {
    if (!hooks.init) {
        init_hook_info(false, false);
    }

    void* ret = get_wrapped_func(symbol);
    if (ret == NULL) {
        return hooks.f_dlvsym(handle, symbol, version);
    } else {
        return ret;
    }
}