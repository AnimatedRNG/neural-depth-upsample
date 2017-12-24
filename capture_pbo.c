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
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void read_into_pbo(const GLuint pbo[2],
                   const GLsizei x_res,
                   const GLsizei y_res) {
    printf("Reading pixels... (%i, %i)\n", x_res, y_res);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[0]);
    glReadPixels(0, 0, x_res, y_res, GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[1]);
    glReadPixels(0, 0, x_res, y_res, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void update_textures_from_pbo(const GLuint pbo[2],
                              const GLuint textures[2],
                              const GLsizei x_res,
                              const GLsizei y_res) {
    printf("Updating textures....\n");
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, x_res, y_res,
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[1]);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0, x_res, y_res,
                    GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint create_shaders(const char* vertex_shader_path,
                      const char* fragment_shader_path) {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    char basedir[2000];
    char complete_vs_path[2000];
    char complete_fs_path[2000];
    readlink("/proc/self/exe", basedir, sizeof(basedir));
    strcat(basedir, "/");
    strcpy(complete_vs_path, basedir);
    strcpy(complete_fs_path, basedir);
    strcat(complete_vs_path, vertex_shader_path);
    strcat(complete_fs_path, vertex_shader_path);
    printf("Found vertex shader: %s\n", complete_vs_path);
    printf("Found fragment shader: %s\n", complete_fs_path);

    // Read the VS
    char* vertex_shader_str;
    FILE* vertex_shader_file = fopen(complete_vs_path, "r");
    if (vertex_shader_file) {
        fseek(vertex_shader_file, 0L, SEEK_END);
        int file_size = ftell(vertex_shader_file);
        printf("VS is %i bytes long\n", file_size);
        rewind(vertex_shader_file);

        vertex_shader_str = malloc(file_size);
        fread(vertex_shader_str, 1, file_size, vertex_shader_file);

        fclose(vertex_shader_file);
    } else {
        printf("Unable to open %s\n", complete_vs_path);
        exit(1);
    }

    // Read the FS
    char* fragment_shader_str;
    FILE* fragment_shader_file = fopen(complete_fs_path, "r");
    if (fragment_shader_file) {
        fseek(fragment_shader_file, 0L, SEEK_END);
        int file_size = ftell(fragment_shader_file);
        printf("FS is %i bytes long\n", file_size);
        rewind(fragment_shader_file);

        fragment_shader_str = malloc(file_size);
        fread(fragment_shader_str, 1, file_size, fragment_shader_file);

        fclose(fragment_shader_file);
    } else {
        printf("Unable to open %s\n", complete_fs_path);
        exit(1);
    }

    GLint ret = GL_FALSE;
    int log_size;

    // Compile Vertex Shader
    printf("Compiling VS\n");
    glShaderSource(vertex_shader, 1, vertex_shader_str, NULL);
    glCompileShader(vertex_shader);

    // Vertex Shader Log
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
    glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = malloc(log_size + 1);
        glGetShaderInfoLog(vertex_shader, log_size, NULL, error_message);
        printf("%s\n", error_message);
        free(error_message);
    }

    // Compile Fragment Shader
    printf("Compiling FS\n");
    glShaderSource(fragment_shader, 1, fragment_shader_str, NULL);
    glCompileShader(fragment_shader);

    // Fragment Shader Log
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = malloc(log_size + 1);
        glGetShaderInfoLog(fragment_shader, log_size, NULL, error_message);
        printf("%s\n", error_message);
        free(error_message);
    }

    // Link the program
    printf("Linking program...\n");
    GLuint prog_id = glCreateProgram();
    glAttachShader(prog_id, vertex_shader);
    glAttachShader(prog_id, vertex_shader);
    glLinkProgram(prog_id);

    // Check the program
    glGetProgramiv(prog_id, GL_LINK_STATUS, &ret);
    glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = malloc(log_size + 1);
        glGetShaderInfoLog(prog_id, log_size, NULL, error_message);
        printf("%s\n", error_message);
    }

    glDetachShader(prog_id, vertex_shader);
    glDetachShader(prog_id, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    free(vertex_shader_str);
    free(fragment_shader_str);

    return prog_id;
}

void render_image(const GLuint prog_id,
                  const bool downsample,
                  const bool depth,
                  const GLsizei x_res,
                  const GLsizei y_res) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    static const GLfloat vbo_data[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,
    };

    GLuint quad_vbo;
    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data),
                 vbo_data, GL_STATIC_DRAW);

    GLuint texID = glGetUniformLocation(prog_id, "renderedTexture");

    glViewport(0, 0, x_res, y_res);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glUniform1i(texID, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    // Draw the triangles !
    glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles

    glDisableVertexAttribArray(0);

    glDeleteVertexArrays(1, &vao);
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
