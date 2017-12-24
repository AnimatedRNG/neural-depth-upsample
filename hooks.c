#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

#include "elfhacks.h"
#include "pipe.h"
#include "hooks_dict.h"
#include "capture_pbo.h"

#define __PUBLIC __attribute__ ((visibility ("default")))

#define THREADS 8

HOOKS hooks;
GLuint pbo[2];
GLuint texture[2];
pthread_t threads[THREADS];
pipe_producer_t* frame_producer;
pipe_consumer_t* frame_writer[THREADS];
GLsizei window_res_x = 100;
GLsizei window_res_y = 100;
int i = 1;
bool init_dir = false;
bool init_pipes = false;

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

void init_hook_info(const bool need_glx_calls,
                    const bool need_gl_calls) {
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
        hooks.__glBindFramebuffer = (f_gl_bind_framebuffer_t)
                                    hooks.f_dlsym(hooks.libGL_handle,
                                            "glBindFramebuffer");
    }

    if (!init_pipes) {
        // Create our file pipes
        // ID, width, height, color texture pointer, depth texture pointer
        pipe_t* pipe = pipe_new(sizeof(buffer_element), 200);

        frame_producer = pipe_producer_new(pipe);
        for (int j = 0; j < THREADS; j++) {
            frame_writer[j] = pipe_consumer_new(pipe);
        }
        pipe_free(pipe);

        for (int j = 0; j < THREADS; j++) {
            pthread_create(&(threads[j]), NULL, frame_consumer_thread,
                           (void*) frame_writer[j]);
        }

        init_pipes = true;
    }

    if (!init_dir) {
        DIR* dir = opendir(DEPTH_UPSAMPLE_DIR);
        struct dirent* ret;
        if (dir) {
            while ((ret = readdir(dir)) != NULL) {
                if (strcmp(ret->d_name, ".") == 0 ||
                        strcmp(ret->d_name, "..") == 0) {
                    continue;
                }
                char file_path[300];
                memset(file_path, 0, sizeof(file_path));
                strcat(file_path, DEPTH_UPSAMPLE_DIR);
                strcat(file_path, ret->d_name);
                printf("Deleting %s\n", file_path);
                remove(file_path);
            }
        } else {
            printf("Creating dir\n");
            mkdir(DEPTH_UPSAMPLE_DIR, 0700);
        }
        init_dir = true;
    }
}

void before_swap_buffers(Display* dpy,
                         GLXDrawable drawable) {
    printf("Before swap buffers\n");
    read_into_pbo(pbo, window_res_x, window_res_y);
    update_textures_from_pbo(pbo, texture, window_res_x, window_res_y);
    write_image(window_res_x, window_res_y, texture[0], ++i, frame_producer);
    hooks.__glXSwapBuffers(dpy, drawable);
    printf("After swap buffers\n");
}

void after_make_current() {
    printf("Just made current\n");
    create_pbo(&(pbo[0]), &(pbo[1]));
}

__PUBLIC void glXSwapBuffers(Display* dpy, GLXDrawable drawable) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }
    before_swap_buffers(dpy, drawable);
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
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }

    printf("Trying to change the viewport to %ix%i\n", width, height);
    hooks.__glViewport(x, y, width, height);

    GLint old_fbo_id;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo_id);
    if (old_fbo_id == 0) {
        printf("Default framebuffer bound, resetting window size\n");
        if (width != window_res_x || height != window_res_y) {
            printf("Resizing textures to (%i, %i)\n", width, height);
            window_res_x = width;
            window_res_y = height;
            create_textures(&(texture[0]), &(texture[1]),
                            width, height);
        }
    }
}

__PUBLIC void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }

    printf("Trying to bind framebuffer %i\n", framebuffer);
    hooks.__glBindFramebuffer(target, framebuffer);
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
    else if (!strcmp(symbol, "glBindFramebuffer"))
        return (void*) &glBindFramebuffer;
    else
        return NULL;
}

__PUBLIC f_glx_ext_func_ptr_t glXGetProcAddressARB(const GLubyte* proc_name) {
    if (!hooks.init_GLX) {
        init_hook_info(true, true);
    }

    printf("Calling glXGetProcAddressARB\n");

    void* ret = get_wrapped_func((char*) proc_name);
    if (ret) {
        return ret;
    } else {
        return hooks.__glXGetProcAddressARB(proc_name);
    }
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
