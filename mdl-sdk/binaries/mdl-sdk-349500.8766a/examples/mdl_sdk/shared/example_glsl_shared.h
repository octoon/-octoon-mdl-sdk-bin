/******************************************************************************
 * Copyright 2022 NVIDIA Corporation. All rights reserved.
 *****************************************************************************/

// Code shared by glsl MDL SDK examples

#ifndef EXAMPLE_GLSL_SHARED_H
#define EXAMPLE_GLSL_SHARED_H

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "example_shared.h"

// Shut down OpenGL and terminate the application.
#define terminate()           \
    do {                      \
        glfwTerminate();      \
        exit_failure();       \
    } while (0)

// Helper macro. Checks whether an OpenGL error occurred and if so prints a message and exits.
#define check_gl_success()                                                  \
    do {                                                                    \
        GLenum err = glGetError();                                          \
        if (err != GL_NO_ERROR) {                                           \
            fprintf(stderr, "OpenGL error 0x%.4X in file %s, line %u.\n",   \
                err, __FILE__, __LINE__);                                   \
            terminate();                                                    \
        }                                                                   \
    } while (false)

// Return a textual representation of the given value.
template <typename T>
static std::string to_string(T val)
{
    std::ostringstream stream;
    stream << val;
    return stream.str();
}

// Reads the content of the given file.
static std::string read_text_file(const std::string& filename)
{
    std::ifstream file(filename.c_str());

    if(!file.is_open())
    {
        fprintf( stderr, "Cannot open file: \"%s\".\n", filename.c_str());
        check_success(file.is_open());
    }

    std::stringstream string_stream;
    string_stream << file.rdbuf();

    return string_stream.str();
}


// Dump information of shader compilation to the console.
static void dump_shader_info(GLuint shader, const char* text)
{
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        GLchar *log = new GLchar[length + 1];
        glGetShaderInfoLog(shader, length + 1, nullptr, log);
        std::cerr << text << log << std::endl;
        delete [] log;
    } else {
        std::cerr << text << std::endl;
    }
}

// Dump information of program linking/validation to the console.
static void dump_program_info(GLuint program, const char* text)
{
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        GLchar *log = new GLchar[length + 1];
        glGetProgramInfoLog(program, length + 1, nullptr, log);
        std::cerr << text << log << std::endl;
        delete [] log;
    } else {
        std::cerr << text << std::endl;
    }
}

// Add a shader to the given program
static void add_shader(GLenum shader_type, const std::string& source_code, GLuint program)
{
    const GLchar* src_buffers[1] = { source_code.c_str() };
    GLuint shader = glCreateShader(shader_type);
    check_success(shader);
    glShaderSource(shader, 1, src_buffers, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        dump_shader_info(shader, "Error compiling the fragment shader: ");
        terminate();
    }
    glAttachShader(program, shader);
    check_gl_success();
}

#endif // EXAMPLE_GLSL_SHARED_H
