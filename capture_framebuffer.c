#include "capture_framebuffer.h"

void init_fbo(HOOKS hooks, FBO* fbo) {
    printf("Rebuilding FBO...\n");

    fbo->other_fbo_bound = false;

    // Make a new FBO
    glGenFramebuffers(1, &(fbo->fbo_name));
    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo_name);

    // Make color and depth textures for the FBO
    glGenTextures(1, &(fbo->color_name));
    glGenTextures(1, &(fbo->depth_name));

    // Specify that we want to draw both the color and the depth
    GLenum DrawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT};
    glDrawBuffers(2, DrawBuffers);

    //assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    // Unbind the FBO
    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void reset_textures(HOOKS hooks, FBO* fbo, const unsigned int res_x,
                    const unsigned int res_y) {
    printf("Resetting textures to %ix%i\n", res_x, res_y);
    fbo->tex_res_x = res_x;
    fbo->tex_res_y = res_y;

    // Save the old texture
    GLint old_tex_id;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex_id);

    // Bind the color FBO
    glBindTexture(GL_TEXTURE_2D, fbo->color_name);

    // Set the color FBO to zero for every channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res_x, res_y, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, 0);

    // Nearest neighbor filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Bind the depth FBO
    glBindTexture(GL_TEXTURE_2D, fbo->depth_name);

    // Set the depth FBO to zero for the only channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, res_x, res_y, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);

    // Nearest neighbor filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

    GLint old_fbo_id;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo_id);

    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo_name);

    // Attach the textures to the FBO
    glBindTexture(GL_TEXTURE_2D, fbo->color_name);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbo->color_name, 0);
    glBindTexture(GL_TEXTURE_2D, fbo->depth_name);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           fbo->depth_name, 0);

    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, old_fbo_id);

    // Restore old texture
    glBindTexture(GL_TEXTURE_2D, old_tex_id);
}

void clear_fbo(HOOKS hooks, FBO* fbo) {
    printf("Clearing FBO\n");
    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo_name);
    GLfloat old_depth;
    GLfloat old_color[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_color);
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &old_depth);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(old_color[0], old_color[1], old_color[2], old_color[3]);
    glClearDepth(old_depth);
    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// TODO: Fix the viewport tracking so that it actually updates on glViewport calls
void bind_fbo(HOOKS hooks, FBO* fbo) {
    printf("Calling bind_fbo with fbo res %ix%i\n", fbo->tex_res_x, fbo->tex_res_y);
    if (fbo->tex_res_x == 0) {
        printf("Tried to bind FBO but the user hasn't called glViewport yet!\n");
        glGetIntegeri_v(GL_VIEWPORT, 0, &(fbo->previous_viewport));
        printf("Guessing viewport %i %i \n", fbo->previous_viewport[2],
               fbo->previous_viewport[3]);
        reset_textures(hooks, fbo,
                       fbo->previous_viewport[2],
                       fbo->previous_viewport[3]);
    } else {
        printf("Old viewport %i %i \n", fbo->previous_viewport[2],
               fbo->previous_viewport[3]);
    }
    //hooks.__glViewport(0, 0, fbo->tex_res_x, fbo->tex_res_y);
    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo_name);

    //assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

void unbind_fbo(HOOKS hooks, FBO* fbo) {
    typedef unsigned char uchar;
    int x_res = fbo->tex_res_x;
    int y_res = fbo->tex_res_y;

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    // rgb image
    glBindTexture(GL_TEXTURE_2D, fbo->color_name);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, x_res, y_res, 0);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uchar* raw_img = (uchar*) malloc(x_res * y_res * 3 * sizeof(uchar));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_img);

    size_t data_length;
    void* data = tdefl_write_image_to_png_file_in_memory(raw_img, x_res, y_res, 3,
                 &data_length);
    FILE* fp = fopen("first.png", "wb");
    fwrite(data, 1, data_length, fp);
    fclose(fp);

    hooks.__glBindFramebuffer(GL_FRAMEBUFFER, 0);
    //hooks.__glViewport(fbo->previous_viewport[0], fbo->previous_viewport[1],
    //fbo->previous_viewport[2], fbo->previous_viewport[3]);
}
