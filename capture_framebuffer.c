#include "capture_framebuffer.h"

#include <stdio.h>
#include <assert.h>
#include <GL/gl.h>

FBO init_fbo(const unsigned int res_x, const unsigned int res_y) {
    FBO fbo;

    fbo.tex_res_x = res_x;
    fbo.tex_res_y = res_y;

    // Make a new FBO
    glGenFramebuffers(1, &(fbo.fbo_name));
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.fbo_name);

    // Make color and depth textures for the FBO
    glGenTextures(1, &(fbo.color_name));
    glGenTextures(1, &(fbo.depth_name));

    // Save the old texture
    GLint old_tex_id;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex_id);

    // Bind the color FBO
    glBindTexture(GL_TEXTURE_2D, fbo.color_name);

    // Set the color FBO to zero for every channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, res_x, res_y, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, 0);

    // Nearest neighbor filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Bind the depth FBO
    glBindTexture(GL_TEXTURE_2D, fbo.depth_name);

    // Set the depth FBO to zero for the only channel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, res_x, res_y, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);

    // Nearest neighbor filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

    // Attach the textures to the FBO
    glBindTexture(GL_TEXTURE_2D, fbo.color_name);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           fbo.color_name, 0);
    glBindTexture(GL_TEXTURE_2D, fbo.depth_name);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           fbo.depth_name, 0);

    // Specify that we want to draw both the color and the depth
    GLenum DrawBuffers[2] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT};
    glDrawBuffers(2, DrawBuffers);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    // Restore old texture
    glBindTexture(GL_TEXTURE_2D, old_tex_id);

    // Unbind the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return fbo;
}

// TODO: Fix the viewport tracking so that it actually updates on glViewport calls
void bind_fbo(FBO fbo) {
    glGetIntegeri_v(GL_VIEWPORT, 0, &(fbo.previous_viewport));
    printf("Old viewport %i %i \n", fbo.previous_viewport[2],
           fbo.previous_viewport[3]);
    glViewport(0, 0, fbo.tex_res_x, fbo.tex_res_y);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.fbo_name);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

void unbind_fbo(FBO fbo) {
    typedef unsigned char uchar;
    int x_res = fbo.tex_res_x;
    int y_res = fbo.tex_res_y;

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    // rgb image
    glBindTexture(GL_TEXTURE_2D, fbo.color_name);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, x_res, y_res, 0);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uchar* raw_img = (uchar*) malloc(x_res * y_res * 3 * sizeof(uchar));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_img);

    FILE* fp = fopen("first.ppm", "wb"); /* b - binary mode */
    (void) fprintf(fp, "P6\n%d %d\n255\n", x_res, y_res);
    for (int j = 0; j < x_res; ++j) {
        for (int i = 0; i < y_res; ++i) {
            static unsigned char color[4];
            color[0] = raw_img[i + j * y_res];
            color[1] = raw_img[i + j * y_res + 1];
            color[2] = raw_img[i + j * y_res + 2];
            (void) fwrite(color, 1, 4, fp);
        }
    }
    fclose(fp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(fbo.previous_viewport[0], fbo.previous_viewport[1],
               fbo.previous_viewport[2], fbo.previous_viewport[3]);
}
