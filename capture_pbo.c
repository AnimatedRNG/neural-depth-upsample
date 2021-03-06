#include "capture_pbo.h"

void check_err() {
    GLenum err = glGetError();

    printf("Checking for errors...\n");
    while (err != GL_NO_ERROR) {
        char* error;

        switch (err) {
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
                break;
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
                break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
                break;
        }

        printf("GL_%s\n", error);
        exit(1);
        err = glGetError();
    }
    printf("No errors found\n");
}

void create_pbo(GLuint* color_pbo,
                GLuint* depth_pbo) {
    glGenBuffers(1, color_pbo);
    glGenBuffers(1, depth_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, *color_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, COLOR_TEXTURE_MAX_SIZE, NULL,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, *depth_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, COLOR_TEXTURE_MAX_SIZE, NULL,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void create_textures(GLuint* color_texture,
                     GLuint* depth_texture,
                     const GLsizei x_res,
                     const GLsizei y_res) {
    printf("Creating textures....\n");
    glGenTextures(1, color_texture);
    glGenTextures(1, depth_texture);
    glBindTexture(GL_TEXTURE_2D, *color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x_res, y_res, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, *depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, x_res, y_res, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void read_into_pbo(const GLuint pbo[2],
                   const GLsizei x_res,
                   const GLsizei y_res) {
    printf("Reading pixels... (%i, %i)\n", x_res, y_res);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[0]);
    glReadPixels(0, 0, x_res, y_res, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1]);
    glReadPixels(0, 0, x_res, y_res, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void update_textures_from_pbo(const GLuint pbo[2],
                              const GLuint textures[2],
                              const GLsizei x_res,
                              const GLsizei y_res) {
    GLint old_unpack_alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_unpack_alignment);
    printf("Updating textures....\n");
    //glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, x_res, y_res,
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[1]);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, x_res, y_res,
                    GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack_alignment);
}

void write_image(const GLsizei x_res,
                 const GLsizei y_res,
                 const GLuint textures[2],
                 const unsigned int ID,
                 pipe_producer_t* producer) {
    //glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, x_res, y_res, 0);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uchar* raw_img = (uchar*) malloc(x_res * y_res * 3 * sizeof(uchar));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_img);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 0, 0, x_res, y_res, 0);
    uchar* depth_img = (uchar*) malloc(x_res * y_res * sizeof(unsigned short));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                  depth_img);

    buffer_element* elem = (buffer_element*) malloc(sizeof(buffer_element));
    elem->ID = ID;
    elem->width = x_res;
    elem->height = y_res;
    elem->color_image = raw_img;
    elem->depth_image = depth_img;

    printf("Pushing ID %i of size (%i, %i) into pipe\n", ID, x_res, y_res);
    printf("Color image pointer: %li\n", elem->color_image);
    printf("Depth image pointer: %li\n", elem->depth_image);
    pipe_push(producer, elem, 1);
}

void* frame_consumer_thread(void* consumer_ptr) {
    pipe_consumer_t* consumer = (pipe_consumer_t*) consumer_ptr;

    buffer_element* elem = (buffer_element*) malloc(sizeof(buffer_element));
    elem->ID = 1;
    while (elem->ID != 0) {
        if (!pipe_pop(consumer, elem, 1)) {
            printf("Failed to pop anything!\n");
        }

        printf("%u\n", elem->ID);
        printf("Found color image pointer: %li\n", elem->color_image);

        char color_file_path[300];
        char depth_file_path[300];
        char file_name[50];
        memset(color_file_path, 0, sizeof(color_file_path));
        memset(depth_file_path, 0, sizeof(depth_file_path));
        memset(file_name, 0, sizeof(file_name));
        strcat(color_file_path, DEPTH_UPSAMPLE_DIR);
        strcat(depth_file_path, DEPTH_UPSAMPLE_DIR);
        snprintf(file_name, sizeof(file_name), "%u", elem->ID);
        strcat(color_file_path, file_name);
        strcat(depth_file_path, file_name);
        strcat(color_file_path, "_color.png");
        strcat(depth_file_path, "_depth.pgm");

        FILE* fp = fopen(color_file_path, "wb");
        size_t data_length;
        void* data = tdefl_write_image_to_png_file_in_memory(
                         elem->color_image,
                         elem->width, elem->height, 3,
                         &data_length);
        fwrite(data, 1, data_length, fp);
        fclose(fp);

        FILE* fp_depth = fopen(depth_file_path, "wb");
        fprintf(fp_depth, "P5 %d %d %d\n", elem->width, elem->height, 65535);

        // Convert from little endian to big endian
        for (int j = 0; j < elem->height; j++) {
            for (int i = 0; i < elem->width; i++) {
                fwrite(elem->depth_image + 2 * (j * elem->width + i) + 1, 1,
                       sizeof(unsigned char), fp_depth);
                fwrite(elem->depth_image + 2 * (j * elem->width + i), 1,
                       sizeof(unsigned char), fp_depth);
            }
        }
        fclose(fp_depth);

        free(elem->color_image);
    }
}
