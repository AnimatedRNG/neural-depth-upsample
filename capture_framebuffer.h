#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/gl.h>

#include "miniz.h"
#include "hooks_dict.h"

typedef struct {
    GLuint fbo_name;
    GLuint color_name;
    GLuint depth_name;

    bool other_fbo_bound;

    GLsizei tex_res_x;
    GLsizei tex_res_y;

    GLint previous_viewport[4];
} FBO;

void init_fbo(HOOKS hooks, FBO* fbo);
void reset_textures(HOOKS hooks, FBO* fbo);
void clear_fbo(HOOKS hooks, FBO* fbo);
void guess_fbo_dims(HOOKS hooks, FBO* fbo);
void bind_fbo(HOOKS hooks, FBO* fbo);
void unbind_fbo(HOOKS hooks, FBO* fbo);
