#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include "elfhacks.h"

#define __PUBLIC __attribute__ ((visibility ("default")))

typedef void* (*f_dlopen_t)(const char* filename, int flag);
typedef void* (*f_dlsym_t)(void*, const char*);
typedef void* (*f_dlvsym_t)(void*, const char*, const char*);

typedef void (*f_glx_swap_buffers_t)(Display* dpy, GLXDrawable drawable);
typedef void (*f_glx_ext_func_ptr_t)(void);

typedef struct {
    f_dlopen_t f_dlopen;
    f_dlsym_t f_dlsym;
    f_dlvsym_t f_dlvsym;

    void* libGL_handle;

    f_glx_swap_buffers_t __glXSwapBuffers;
    f_glx_ext_func_ptr_t(*__glXGetProcAddressARB)(const GLubyte*);
} HOOKS;

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

HOOKS init_hook_info() {
    HOOKS hooks;
    get_real_dlsym(&(hooks.f_dlopen), &(hooks.f_dlsym), &(hooks.f_dlvsym));

    hooks.libGL_handle = hooks.f_dlopen("libGL.so", RTLD_LAZY);

    hooks.__glXSwapBuffers = (f_glx_swap_buffers_t) hooks.f_dlsym(
                                 hooks.libGL_handle, "glXSwapBuffers");

    hooks.__glXGetProcAddressARB = (f_glx_ext_func_ptr_t(*)(const GLubyte*))
                                   hooks.f_dlsym(hooks.libGL_handle,
                                           "glXGetProcAddressARB");
    return hooks;
}

__PUBLIC void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    /*f_dlopen_t f_dlopen;
    f_dlsym_t f_dlsym;
    f_dlvsym_t f_dlvsym;
    get_real_dlsym(&f_dlopen, &f_dlsym, &f_dlvsym);

    void* libGL_handle;
    libGL_handle = f_dlopen("libGL.so", RTLD_LAZY);

    f_glx_swap_buffers_t __glXSwapBuffers = (f_glx_swap_buffers_t) f_dlsym(
            libGL_handle,
            "glXSwapBuffers");*/
    HOOKS hooks = init_hook_info();
    printf("Testing\n");
    return hooks.__glXSwapBuffers(dpy, drawable);
}

__PUBLIC f_glx_ext_func_ptr_t glXGetProcAddressARB(const GLubyte* proc_name) {
    f_dlopen_t f_dlopen;
    f_dlsym_t f_dlsym;
    f_dlvsym_t f_dlvsym;
    get_real_dlsym(&f_dlopen, &f_dlsym, &f_dlvsym);

    void* libGL_handle;
    libGL_handle = f_dlopen("libGL.so", RTLD_LAZY);

    void (*__glXSwapBuffers)(Display * dpy, GLXDrawable drawable) =
        (void (*)(Display*, GLXDrawable))
        f_dlsym(libGL_handle, "glXSwapBuffers");

    f_glx_ext_func_ptr_t(*__glXGetProcAddressARB)(const GLubyte*) =
        (f_glx_ext_func_ptr_t(*)(const GLubyte*)) f_dlsym(libGL_handle,
                "glXGetProcAddressARB");

    if (!strcmp((char*) proc_name, "glXSwapBuffers")) {
        return (f_glx_ext_func_ptr_t) &glXSwapBuffers;
    } else if (!strcmp((char*) proc_name, "glXGetProcAddressARB")) {
        return (f_glx_ext_func_ptr_t) &glXGetProcAddressARB;
    } else {
        return __glXGetProcAddressARB(proc_name);
    }
}
