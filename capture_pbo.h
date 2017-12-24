#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <dirent.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "pipe.h"
#include "miniz.h"
#include "hooks_dict.h"

#define COLOR_TEXTURE_MAX_SIZE 8294400 * 3
#define DEPTH_TEXTURE_MAX_SIZE 8294400 * 4
#define ELEMENT_SIZE (sizeof(char*) + sizeof(float*) + sizeof(GLsizei) * 2 + sizeof(unsigned int))
#define DEPTH_UPSAMPLE_DIR "depth_upsample_data/"

typedef unsigned char uchar;

typedef struct {
    unsigned int ID;
    GLsizei width;
    GLsizei height;
    uchar* color_image;
    uchar* depth_image;
} buffer_element;

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
void render_image(HOOKS hooks,
                  const GLuint prog_id,
                  const GLuint texture[2],
                  const bool downsample,
                  const bool depth,
                  const GLsizei x_res,
                  const GLsizei y_res);
void write_image(const GLsizei x_res,
                 const GLsizei y_res,
                 const GLuint texture_name,
                 const unsigned int ID,
                 pipe_producer_t* producer);
void* frame_consumer_thread(void* consumer_ptr);
