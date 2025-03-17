#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --------------------------------------------------------------------------------
// Global variables
// --------------------------------------------------------------------------------
float zoomLevel = 10.0f;
// Start the camera angled above the origin
glm::vec3 cameraPos(2.0f, 2.0f, 2.0f);
float cameraMoveSpeed = 0.05f;

// Forward declarations
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
unsigned int createShaderProgram(const char* vertexSrc, const char* fragmentSrc);
unsigned int loadTexture(const char* path);

// --------------------------------------------------------------------------------
// Vertex Shader
// --------------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;     // Position
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;  // Normal

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord; // We'll just pass it through if we need a texture
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// --------------------------------------------------------------------------------
// Fragment Shader (with simple sky lighting + optional solid color)
// --------------------------------------------------------------------------------
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D texture1;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

// "Sky" color and strength
uniform vec3 skyColor;
uniform float skyStrength;

// An optional solidColor for items that don't need texturing
uniform vec3 solidColor;
uniform bool useSolidColor;  // if true, ignore texture and use solidColor

void main()
{
    // Ambient lighting
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
    ambient += skyStrength * skyColor;

    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine lighting
    vec3 lighting = ambient + diffuse + specular;

    // If "useSolidColor" is true, we ignore the texture and just tint with solidColor
    if(useSolidColor) {
        FragColor = vec4(lighting * solidColor, 1.0);
    } else {
        vec3 texColor = texture(texture1, TexCoord).rgb;
        FragColor = vec4(lighting * texColor, 1.0);
    }
}
)";

// --------------------------------------------------------------------------------
// Scroll callback for zoom
// --------------------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    zoomLevel -= (float)yoffset * 0.1f;
    if (zoomLevel < 0.1f) zoomLevel = 0.1f;
}

// --------------------------------------------------------------------------------
// Compile & link shader helpers
// --------------------------------------------------------------------------------
unsigned int compileShader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram(const char* vertexSrc, const char* fragmentSrc) {
    unsigned int vs = compileShader(vertexSrc, GL_VERTEX_SHADER);
    unsigned int fs = compileShader(fragmentSrc, GL_FRAGMENT_SHADER);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// --------------------------------------------------------------------------------
// Load texture
// --------------------------------------------------------------------------------
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 1)      format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format,
                     width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        // Wrapping/filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    return textureID;
}

// --------------------------------------------------------------------------------
// Create a UV-sphere as a triangle mesh
//   - radius: the sphere radius
//   - sectorCount: how many slices around the Y-axis
//   - stackCount: how many stacks from bottom to top
// --------------------------------------------------------------------------------
void createSphereVAO(float radius, int sectorCount, int stackCount,
                     unsigned int &VAO, unsigned int &VBO, unsigned int &EBO)
{
    std::vector<float> vertices;   // will hold (x, y, z,  texU, texV,  nx, ny, nz)
    std::vector<unsigned int> indices;

    float lengthInv = 1.0f / radius;

    // Generate vertex data
    // ----------------------------------
    for(int i = 0; i <= stackCount; ++i) {
        // stackAngle goes from +PI/2 (top) down to -PI/2 (bottom)
        float stackAngle =  (float)M_PI/2.0f - (float)i * (float)M_PI / (float)stackCount;
        float xy = radius * cosf(stackAngle);  // r * cos(u)
        float y  = radius * sinf(stackAngle);  // r * sin(u)

        for(int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = (float)j * 2.0f * (float)M_PI / (float)sectorCount; // 0..2PI

            // vertex position (x, y, z)
            float x = xy * cosf(sectorAngle);
            float z = xy * sinf(sectorAngle);

            // normalized vector for the normal
            float nx = x * lengthInv;
            float ny = y * lengthInv;
            float nz = z * lengthInv;

            // some basic texture coords (u, v) between [0,1]
            float u = (float)j / (float)sectorCount;
            float v = (float)i / (float)stackCount;

            // push back
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(u);
            vertices.push_back(v);
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    // Generate index data
    // ----------------------------------
    // Each stack has sectorCount "quads", each quad is two triangles
    // (i, j) => i-th stack, j-th sector
    int k1, k2;
    for(int i = 0; i < stackCount; ++i) {
        k1 = i * (sectorCount + 1);
        k2 = k1 + sectorCount + 1;
        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            // 2 triangles per sector
            if(i != 0) {
                // triangle 1
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if(i != (stackCount-1)) {
                // triangle 2
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    // Create VAO/VBO/EBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                 vertices.data(), GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers
    // layout (location=0): position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    // layout (location=1): texture coords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout (location=2): normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

int main() {
    // Init GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Isometric + Sphere Player", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);

    // Grey background
    glClearColor(0.7f, 0.7f, 0.7f, 1.0f);

    // Scroll callback for zoom
    glfwSetScrollCallback(window, scroll_callback);

    // Build our shaders & program
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // ----------------------------------
    // 1) Create the cube geometry (same as earlier)
    // ----------------------------------
    float cubeVertices[] = {
            // positions            // tex coords // normals
            // Back face
            -0.5f, -0.5f, -0.5f,   0.f, 0.f,     0.f, 0.f, -1.f,
            0.5f, -0.5f, -0.5f,   1.f, 0.f,     0.f, 0.f, -1.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,     0.f, 0.f, -1.f,
            -0.5f,  0.5f, -0.5f,   0.f, 1.f,     0.f, 0.f, -1.f,

            // Front face
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,     0.f, 0.f,  1.f,
            0.5f, -0.5f,  0.5f,   1.f, 0.f,     0.f, 0.f,  1.f,
            0.5f,  0.5f,  0.5f,   1.f, 1.f,     0.f, 0.f,  1.f,
            -0.5f,  0.5f,  0.5f,   0.f, 1.f,     0.f, 0.f,  1.f,

            // Left face
            -0.5f,  0.5f,  0.5f,   1.f, 0.f,    -1.f, 0.f,  0.f,
            -0.5f,  0.5f, -0.5f,   1.f, 1.f,    -1.f, 0.f,  0.f,
            -0.5f, -0.5f, -0.5f,   0.f, 1.f,    -1.f, 0.f,  0.f,
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,    -1.f, 0.f,  0.f,

            // Right face
            0.5f,  0.5f,  0.5f,   1.f, 0.f,     1.f, 0.f,  0.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,     1.f, 0.f,  0.f,
            0.5f, -0.5f, -0.5f,   0.f, 1.f,     1.f, 0.f,  0.f,
            0.5f, -0.5f,  0.5f,   0.f, 0.f,     1.f, 0.f,  0.f,

            // Bottom face
            -0.5f, -0.5f, -0.5f,   0.f, 1.f,     0.f, -1.f, 0.f,
            0.5f, -0.5f, -0.5f,   1.f, 1.f,     0.f, -1.f, 0.f,
            0.5f, -0.5f,  0.5f,   1.f, 0.f,     0.f, -1.f, 0.f,
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,     0.f, -1.f, 0.f,

            // Top face
            -0.5f,  0.5f, -0.5f,   0.f, 1.f,     0.f,  1.f, 0.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,     0.f,  1.f, 0.f,
            0.5f,  0.5f,  0.5f,   1.f, 0.f,     0.f,  1.f, 0.f,
            -0.5f,  0.5f,  0.5f,   0.f, 0.f,     0.f,  1.f, 0.f
    };

    unsigned int cubeIndices[] = {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            8, 9,10,10,11, 8,
            12,13,14,14,15,12,
            16,17,18,18,19,16,
            20,21,22,22,23,20
    };

    unsigned int cubeVAO, cubeVBO, cubeEBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);
    // Texture
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Load a texture for the cubes
    unsigned int textureForCubes = loadTexture("resources/textures/texture_08.png");

    // ----------------------------------
    // 2) Create the sphere for the "player"
    // ----------------------------------
    unsigned int sphereVAO, sphereVBO, sphereEBO;
    float sphereRadius = 0.3f;
    int sectors = 16;
    int stacks  = 16;
    createSphereVAO(sphereRadius, sectors, stacks, sphereVAO, sphereVBO, sphereEBO);

    // Light and "sky" settings
    glm::vec3 lightPos(0.0f, 20.0f, 0.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    glm::vec3 skyColor(0.5f, 0.7f, 1.0f);
    float skyStrength = 0.2f;

    // Grid size
    const int gridSize = 10;

    while (!glfwWindowShouldClose(window)) {
        // -- INPUT (WASD) --
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            cameraPos.y += cameraMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            cameraPos.y -= cameraMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            cameraPos.x -= cameraMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            cameraPos.x += cameraMoveSpeed;
        }

        // -- RENDER --
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build view + ortho projection for isometric
        glm::mat4 view = glm::lookAt(
                cameraPos,
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glm::mat4 projection = glm::ortho(
                -zoomLevel, zoomLevel,
                -zoomLevel, zoomLevel,
                -10.f, 10.f
        );

        // Use our shader
        glUseProgram(shaderProgram);

        // Update common uniforms
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "skyColor"), 1, glm::value_ptr(skyColor));
        glUniform1f(glGetUniformLocation(shaderProgram, "skyStrength"), skyStrength);

        // We'll pass the "useSolidColor" uniform = false for cubes, true for the sphere
        int useSolidLoc = glGetUniformLocation(shaderProgram, "useSolidColor");
        int solidColorLoc = glGetUniformLocation(shaderProgram, "solidColor");

        // Pass view & projection
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 3) Draw the grid of cubes
        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureForCubes);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        // For cubes, we use the texture (not solid color)
        glUniform1i(useSolidLoc, 0);

        for (int i = 0; i < gridSize; ++i) {
            for (int j = 0; j < gridSize; ++j) {
                glm::mat4 model(1.0f);
                model = glm::translate(model, glm::vec3(
                        i - gridSize / 2.0f,
                        0.0f,  // each cube extends from y=-0.5 to +0.5
                        j - gridSize / 2.0f
                ));
                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        // 4) Draw the "player" sphere on top of one cube
        {
            // Example: put it at grid coords (3,2)
            // The top of a cube is y=+0.5 from its center.
            // We'll place the center of the sphere at y= 0.5 + sphereRadius
            float targetX = 3 - gridSize / 2.0f;
            float targetZ = 2 - gridSize / 2.0f;
            float aboveY = 0.5f + sphereRadius;

            glm::mat4 model(1.0f);
            model = glm::translate(model, glm::vec3(targetX, aboveY, targetZ));

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            // Use a solid color
            glUniform1i(useSolidLoc, 1);
            glm::vec3 playerColor(1.0f, 0.2f, 0.2f); // a red-ish sphere
            glUniform3fv(solidColorLoc, 1, glm::value_ptr(playerColor));

            glBindVertexArray(sphereVAO);
            // Indices used: each "stack band" has sectorCount * 2 triangles => 6 indices per sector,
            // total = 6 * sectorCount * stackCount. But we can just pass the size from the EBO:
            // we know createSphereVAO put all in an EBO. Let’s get the index count:
            int sphereIndexCount = 6 * sectors * (stacks - 1);
            // Actually the formula can vary with how we handle the poles, but typically:
            // 6*(sectors)*(stacks-1). Or we can store the size in a variable. Let's do:
            glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);

    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
