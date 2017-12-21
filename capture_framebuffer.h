#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

    GLsizei tex_res_x;
    GLsizei tex_res_y;

    GLint previous_viewport[4];
} FBO;

FBO init_fbo(HOOKS hooks, const unsigned int res_x, const unsigned int res_y);
void bind_fbo(HOOKS hooks, FBO fbo);
void unbind_fbo(HOOKS hooks, FBO fbo);
