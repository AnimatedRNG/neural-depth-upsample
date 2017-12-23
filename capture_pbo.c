#include "capture_pbo.h"

void create_pbo(GLuint* color_pbo,
                GLuint* depth_pbo) {
    glGenBuffers(1, color_pbo);
    glGenBuffers(1, depth_pbo);
}

void create_textures(GLuint* color_texture,
                     GLuint* depth_texture,
                     const GLsizei x_res,
                     const GLsizei y_res) {
    printf("Creating textures....\n");
    glGenTextures(1, color_texture);
    glGenTextures(1, depth_texture);
    glBindTexture(GL_TEXTURE_2D, *color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, x_res, y_res, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, *depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, x_res, y_res, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void read_into_pbo(const GLuint pbo[2],
                   const GLsizei x_res,
                   const GLsizei y_res) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[0]);
    printf("Reading pixels... (%i, %i)\n", x_res, y_res);
    glReadPixels(0, 0, x_res, y_res, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void update_textures_from_pbo(const GLuint pbo[2],
                              const GLuint textures[2],
                              const GLsizei x_res,
                              const GLsizei y_res) {
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[0]);
    printf("Updating textures....\n");
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, x_res, y_res,
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void write_image(const GLsizei x_res,
                 const GLsizei y_res,
                 const GLuint texture_name) {
    typedef unsigned char uchar;

    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    // rgb image
    glBindTexture(GL_TEXTURE_2D, texture_name);
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
}
