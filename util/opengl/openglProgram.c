#pragma once

#include "../win32.c"
#include <gl/gl.h>

#include "glFlags.h"
#include "glFunctions.c"

// #define FLOATS_PER_VERTEX 4
// float vertices[] = {
//     // Position      UV coords
//     1.0f, 0.0f, 1.0f, 0.0f,
//     0.0f, 0.0f, 0.0f, 0.0f,
//     1.0f, 1.0f, 1.0f, 1.0f,
//     0.0f, 1.0f, 0.0f, 1.0f};

// GLuint currentProgram;

// void UseProgram(GLuint program)
// {
//     glUseProgram(program);
//     currentProgram = program;
// }

// inline Mat4 CreateViewMatrix(float x, float y, float w, float h)
// {
//     return (Mat4){
//         w,
//         0,
//         0,
//         x,
//         0,
//         h,
//         0,
//         y,
//         0,
//         0,
//         1,
//         0,
//         0,
//         0,
//         0,
//         1,
//     };
// }

// inline void SetV3f(char *name, V3f vec)
// {
//     glUniform3f(glGetUniformLocation(currentProgram, name), vec.x, vec.y, vec.z);
// }

// inline void SetV4f(char *name, V4f vec)
// {
//     glUniform4f(glGetUniformLocation(currentProgram, name), vec.r, vec.g, vec.b, vec.a);
// }

// inline void Set1f(char *name, f32 v)
// {
//     glUniform1f(glGetUniformLocation(currentProgram, name), v);
// }

// inline void Set1i(char *name, i32 v)
// {
//     glUniform1i(glGetUniformLocation(currentProgram, name), v);
// }

// inline void SetMat4(char *name, Mat4 mat)
// {
//     glUniformMatrix4fv(glGetUniformLocation(currentProgram, name), 1, GL_TRUE, &mat.values[0]);
// }

// inline void SetProjection(V2i screen)
// {
//     SetMat4("projection", CreateViewMatrix(-1, -1, 2.0f / (f32)screen.x, 2.0f / (f32)screen.y));
// }

// inline void SetView(f32 x, f32 y, f32 w, f32 h)
// {
//     SetMat4("view", CreateViewMatrix(x, y, w, h));
// }

// inline void SetColor(V4f vec)
// {
//     SetV4f("color", vec);
// }

// inline void DrawRect()
// {
//     glDrawArrays(GL_TRIANGLE_STRIP, 0, ArrayLength(vertices) / FLOATS_PER_VERTEX);
// }

// inline void SetTexture(GLuint textureId)
// {
//     glBindTexture(GL_TEXTURE_2D, textureId);
// }

GLuint CompileShader(GLuint shaderEnum, const char *source)
{
    GLuint shader = glCreateShader(shaderEnum);
    glShaderSource(shader, 1, &source, NULL);

    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    char *shaderName = shaderEnum == GL_VERTEX_SHADER ? "Vertex" : "Fragmment";
    if (success)
    {
        OutputDebugStringA(shaderName);
        OutputDebugStringA("Shader Compiled\n");
    }
    else
    {
        OutputDebugStringA(shaderName);
        OutputDebugStringA("Shader Errors\n");

        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        OutputDebugStringA(infoLog);
        OutputDebugStringA("\n");
    }
    return shader;
}

GLuint CreateProgram(char *vertexShaderPath, char *fragmentShaderPath)
{
    FileContent vertexFile = ReadMyFileImp(vertexShaderPath);
    FileContent fragmentFile = ReadMyFileImp(fragmentShaderPath);

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexFile.content);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentFile.content);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success)
        OutputDebugStringA("Program Linked\n");
    else
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        OutputDebugStringA("Error during linking: \n");
        OutputDebugStringA(infoLog);
        OutputDebugStringA("\n");
    }

    // TODO: add error checking
    VirtualFreeMemory(fragmentFile.content);
    VirtualFreeMemory(vertexFile.content);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// GLuint vertexBuffer;
// GLuint vertexArray;

// void InitOpenGL()
// {
//     InitFunctions();

//     glGenBuffers(1, &vertexBuffer);
//     glGenVertexArrays(1, &vertexArray);

//     glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

//     glBindVertexArray(vertexArray);
//     size_t stride = FLOATS_PER_VERTEX * sizeof(float);
//     glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
//     glEnableVertexAttribArray(0);

//     glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)0);
//     glEnableVertexAttribArray(1);
// }

// #endif
