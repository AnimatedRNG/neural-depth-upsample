#include "capture_pbo.h"

const char* vertex_shader_str =
    "#version 310 es\n"
    "precision mediump float;\n"
    "layout(location = 0) in vec3 vertexPosition_modelspace;\n"
    "out vec2 UV;\n"
    "void main(){\n"
    "    gl_Position =  vec4(vertexPosition_modelspace,1);\n"
    "    UV = (vertexPosition_modelspace.xy+vec2(1,1))/2.0;\n"
    "}\n";

const char* fragment_shader_str =
    "#version 310 es\n"
    "precision highp float;\n"
    "in vec2 UV;\n"
    "out vec3 color;\n"
    "uniform sampler2D color_texture;\n"
    "uniform float time;\n"
    "void main(){\n"
    "    color = texture(color_texture, UV).bgr;\n"
    "    //color = vec3(UV, 0.0);\n"
    "}";

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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, x_res, y_res, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
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
    glReadPixels(0, 0, x_res, y_res, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void update_textures_from_pbo(const GLuint pbo[2],
                              const GLuint textures[2],
                              const GLsizei x_res,
                              const GLsizei y_res) {
    GLint old_unpack_alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &old_unpack_alignment);
    printf("Updating textures....\n");
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
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
                    GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, old_unpack_alignment);
}

GLuint create_shaders() {
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    GLint ret = GL_FALSE;
    int log_size;

    // Compile Vertex Shader
    printf("Compiling VS\n");
    glShaderSource(vertex_shader, 1, &vertex_shader_str, NULL);
    glCompileShader(vertex_shader);

    // Vertex Shader Log
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &ret);
    glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = (char*) malloc(log_size + 1);
        glGetShaderInfoLog(vertex_shader, log_size, NULL, error_message);
        printf("%s\n", error_message);
        free((void*) error_message);
    }

    // Compile Fragment Shader
    printf("Compiling FS\n");
    glShaderSource(fragment_shader, 1, &fragment_shader_str, NULL);
    glCompileShader(fragment_shader);

    // Fragment Shader Log
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &ret);
    glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = (char*) malloc(log_size + 1);
        glGetShaderInfoLog(fragment_shader, log_size, NULL, error_message);
        printf("%s\n", error_message);
        free((void*) error_message);
    }

    // Link the program
    printf("Linking program...\n");
    GLuint prog_id = glCreateProgram();
    glAttachShader(prog_id, vertex_shader);
    glAttachShader(prog_id, fragment_shader);
    glLinkProgram(prog_id);

    // Check the program
    glGetProgramiv(prog_id, GL_LINK_STATUS, &ret);
    glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &log_size);
    if (log_size > 0) {
        char* error_message = (char*) malloc(log_size + 1);
        glGetShaderInfoLog(prog_id, log_size, NULL, error_message);
        printf("%s\n", error_message);
        free((void*) error_message);
    }

    glDetachShader(prog_id, vertex_shader);
    glDetachShader(prog_id, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return prog_id;
}

void render_image(HOOKS hooks,
                  const GLuint prog_id,
                  const GLuint texture[2],
                  const bool downsample,
                  const bool depth,
                  const GLsizei x_res,
                  const GLsizei y_res) {
    GLint old_vao, old_buffer, old_program;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_buffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);

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

    GLuint tex_loc = glGetUniformLocation(prog_id, "color_texture");
    printf("Tex loc: %u\n", tex_loc);
    printf("Texture: %u\n", texture[0]);

    hooks.__glViewport(0, 0, x_res, y_res);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    glUniform1i(tex_loc, 0);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);

    glDeleteVertexArrays(1, &vao);

    glUseProgram(old_program);
    glBindBuffer(GL_ARRAY_BUFFER, old_buffer);
    glBindVertexArray(old_vao);

    glEnable(GL_DEPTH_TEST);
    check_err();
}

void write_image(const GLsizei x_res,
                 const GLsizei y_res,
                 const GLuint texture_name,
                 const unsigned int ID,
                 pipe_producer_t* producer) {
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    // rgb image
    glBindTexture(GL_TEXTURE_2D, texture_name);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, x_res, y_res, 0);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    uchar* raw_img = (uchar*) malloc(x_res * y_res * 3 * sizeof(uchar));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_img);

    buffer_element* elem = (buffer_element*) malloc(sizeof(buffer_element));
    elem->ID = ID;
    elem->width = x_res;
    elem->height = y_res;
    elem->color_image = raw_img;

    printf("Pushing ID %i of size (%i, %i) into pipe\n", ID, x_res, y_res);
    printf("Color image pointer: %li\n", elem->color_image);
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

        char file_path[300];
        char file_name[50];
        memset(file_path, 0, sizeof(file_path));
        memset(file_name, 0, sizeof(file_name));
        strcat(file_path, DEPTH_UPSAMPLE_DIR);
        snprintf(file_name, sizeof(file_name), "%u", elem->ID);
        strcat(file_name, ".png");
        strcat(file_path, file_name);

        FILE* fp = fopen(file_path, "wb");
        size_t data_length;
        void* data = tdefl_write_image_to_png_file_in_memory(
                         elem->color_image,
                         elem->width, elem->height, 3,
                         &data_length);
        fwrite(data, 1, data_length, fp);
        fclose(fp);

        free(elem->color_image);
    }
}
