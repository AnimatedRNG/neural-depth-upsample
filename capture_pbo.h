#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "miniz.h"

void create_pbo(GLuint* color_pbo,
                GLuint* depth_pbo);
void create_textures(GLuint* color_texture,
                     GLuint* depth_texture,
                     const GLsizei x_res,
                     const GLsizei y_res);
void read_into_pbo(const GLuint pbo[2],
                   const GLsizei x_res,
                   const GLsizei y_res);
void update_textures_from_pbo(const GLuint pbo[2],
                              const GLuint textures[2],
                              const GLsizei x_res,
                              const GLsizei y_res);
GLuint create_shaders();
void render_image(const GLuint prog_id,
                  const bool downsample,
                  const bool depth,
                  const GLsizei x_res,
                  const GLsizei y_res);
void write_image(const GLsizei x_res,
                 const GLsizei y_res,
                 const GLuint texture_name);
