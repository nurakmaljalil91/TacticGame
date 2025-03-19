//
// Created by User on 19/3/2025.
//

#ifndef TACTICGAME_SHADER_H
#define TACTICGAME_SHADER_H


#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader
{
public:
    Shader(const std::string& vertexCode, const std::string& fragmentCode);
    ~Shader();

    void use() const;

    // Set uniform helpers
    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
    unsigned int program_;

    unsigned int compileShader(const char* source, GLenum type);
    void linkProgram(unsigned int vs, unsigned int fs);
};


#endif //TACTICGAME_SHADER_H
