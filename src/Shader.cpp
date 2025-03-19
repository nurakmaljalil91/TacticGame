//
// Created by User on 19/3/2025.
//

#include "Shader.h"
#include <iostream>

Shader::Shader(const std::string& vertexCode, const std::string& fragmentCode)
{
    // Compile
    unsigned int vs = compileShader(vertexCode.c_str(), GL_VERTEX_SHADER);
    unsigned int fs = compileShader(fragmentCode.c_str(), GL_FRAGMENT_SHADER);

    // Link
    linkProgram(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader()
{
    glDeleteProgram(program_);
}

void Shader::use() const
{
    glUseProgram(program_);
}

void Shader::setBool(const std::string &name, bool value) const
{
    glUniform1i(glGetUniformLocation(program_, name.c_str()), (int)value);
}

void Shader::setInt(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(program_, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(program_, name.c_str()), value);
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    glUniform3fv(glGetUniformLocation(program_, name.c_str()), 1, &value[0]);
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(program_, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

unsigned int Shader::compileShader(const char* source, GLenum type)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR: Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

void Shader::linkProgram(unsigned int vs, unsigned int fs)
{
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    int success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if(!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program_, 512, nullptr, infoLog);
        std::cerr << "ERROR: Shader linking failed: " << infoLog << std::endl;
    }
}