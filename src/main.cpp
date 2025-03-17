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

// We keep the camera fixed in an isometric position
glm::vec3 cameraPos(2.0f, 2.0f, 2.0f);

// We no longer move the camera with WASD; the user controls the player's sphere
// We'll track the player's position in (x, y, z) world space.
glm::vec3 playerPos(0.0f);  // We'll set an initial position once we know the grid

float zoomLevel = 10.0f;
float playerMoveSpeed = 0.02f;  // Movement speed for the sphere

// Forward declarations
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
unsigned int createShaderProgram(const char* vertexSrc, const char* fragmentSrc);
unsigned int loadTexture(const char* path);

// --------------------------------------------------------------------------------
// Vertex Shader
// --------------------------------------------------------------------------------
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

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
    TexCoord = aTexCoord;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// --------------------------------------------------------------------------------
// Fragment Shader (with "sky" lighting + optional solid color)
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

// If true, ignore texture and use solidColor
uniform bool useSolidColor;
uniform vec3 solidColor;

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

    // Combine them
    vec3 lighting = ambient + diffuse + specular;

    if(useSolidColor) {
        FragColor = vec4(lighting * solidColor, 1.0);
    } else {
        // Use a texture
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
// Helper: Compile & link shaders
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
// Load texture from file
// --------------------------------------------------------------------------------
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

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
// Generate a UV-sphere for the "player" object
// --------------------------------------------------------------------------------
void createSphereVAO(float radius, int sectorCount, int stackCount,
                     unsigned int &VAO, unsigned int &VBO, unsigned int &EBO)
{
    std::vector<float> vertices; // (x, y, z, u, v, nx, ny, nz)
    std::vector<unsigned int> indices;

    float lengthInv = 1.0f / radius;

    // Build the sphere vertices
    for(int i = 0; i <= stackCount; ++i) {
        // from +PI/2 down to -PI/2
        float stackAngle =  (float)M_PI/2.0f - (float)i * (float)M_PI / (float)stackCount;
        float xy = radius * cosf(stackAngle);
        float y  = radius * sinf(stackAngle);

        for(int j = 0; j <= sectorCount; ++j) {
            float sectorAngle = (float)j * 2.0f * (float)M_PI / (float)sectorCount; // 0..2PI

            float x = xy * cosf(sectorAngle);
            float z = xy * sinf(sectorAngle);

            // normalized normal
            float nx = x * lengthInv;
            float ny = y * lengthInv;
            float nz = z * lengthInv;

            // basic tex coords
            float u = (float)j / (float)sectorCount;
            float v = (float)i / (float)stackCount;

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

    // Build indices
    int k1, k2;
    for(int i = 0; i < stackCount; ++i) {
        k1 = i * (sectorCount + 1);
        k2 = k1 + sectorCount + 1;
        for(int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
            if(i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if(i != (stackCount-1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    // Create VAO
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

    // Positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);

    // Tex coords
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Normals
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// --------------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------------
int main() {
    // Init GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Isometric + Player Movement", nullptr, nullptr);
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

    // Use scroll to zoom
    glfwSetScrollCallback(window, scroll_callback);

    // Build shader program
    unsigned int shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // --------------------------------------------------------------------------------
    // Create the cubes (for the grid)
    // --------------------------------------------------------------------------------
    float cubeVertices[] = {
            // positions           // tex coords  // normals
            // Back face
            -0.5f, -0.5f, -0.5f,   0.f, 0.f,      0.f, 0.f, -1.f,
            0.5f, -0.5f, -0.5f,   1.f, 0.f,      0.f, 0.f, -1.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,      0.f, 0.f, -1.f,
            -0.5f,  0.5f, -0.5f,   0.f, 1.f,      0.f, 0.f, -1.f,

            // Front face
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,      0.f, 0.f,  1.f,
            0.5f, -0.5f,  0.5f,   1.f, 0.f,      0.f, 0.f,  1.f,
            0.5f,  0.5f,  0.5f,   1.f, 1.f,      0.f, 0.f,  1.f,
            -0.5f,  0.5f,  0.5f,   0.f, 1.f,      0.f, 0.f,  1.f,

            // Left face
            -0.5f,  0.5f,  0.5f,   1.f, 0.f,     -1.f, 0.f,  0.f,
            -0.5f,  0.5f, -0.5f,   1.f, 1.f,     -1.f, 0.f,  0.f,
            -0.5f, -0.5f, -0.5f,   0.f, 1.f,     -1.f, 0.f,  0.f,
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,     -1.f, 0.f,  0.f,

            // Right face
            0.5f,  0.5f,  0.5f,   1.f, 0.f,      1.f, 0.f,  0.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,      1.f, 0.f,  0.f,
            0.5f, -0.5f, -0.5f,   0.f, 1.f,      1.f, 0.f,  0.f,
            0.5f, -0.5f,  0.5f,   0.f, 0.f,      1.f, 0.f,  0.f,

            // Bottom face
            -0.5f, -0.5f, -0.5f,   0.f, 1.f,      0.f, -1.f, 0.f,
            0.5f, -0.5f, -0.5f,   1.f, 1.f,      0.f, -1.f, 0.f,
            0.5f, -0.5f,  0.5f,   1.f, 0.f,      0.f, -1.f, 0.f,
            -0.5f, -0.5f,  0.5f,   0.f, 0.f,      0.f, -1.f, 0.f,

            // Top face
            -0.5f,  0.5f, -0.5f,   0.f, 1.f,      0.f,  1.f, 0.f,
            0.5f,  0.5f, -0.5f,   1.f, 1.f,      0.f,  1.f, 0.f,
            0.5f,  0.5f,  0.5f,   1.f, 0.f,      0.f,  1.f, 0.f,
            -0.5f,  0.5f,  0.5f,   0.f, 0.f,      0.f,  1.f, 0.f
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    // Normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Load texture for cubes
    unsigned int textureForCubes = loadTexture("resources/textures/texture_08.png");

    // --------------------------------------------------------------------------------
    // Create the sphere (player)
    // --------------------------------------------------------------------------------
    unsigned int sphereVAO, sphereVBO, sphereEBO;
    float sphereRadius = 0.3f;
    int sectors = 16;
    int stacks  = 16;
    createSphereVAO(sphereRadius, sectors, stacks, sphereVAO, sphereVBO, sphereEBO);

    // We'll place the sphere in the middle of the map to start
    // Suppose the map is a 10x10 grid, each cell around the origin (like your code).
    const int gridSize = 10;
    // Let's place the player around the center: e.g. x=3, z=2
    // The top of each cube is y=+0.5 from that cell center
    playerPos.x = (3 - gridSize / 2.0f);
    playerPos.y = 0.5f + sphereRadius; // resting on top
    playerPos.z = (2 - gridSize / 2.0f);

    // Light, sky
    glm::vec3 lightPos(0.0f, 20.0f, 0.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    glm::vec3 skyColor(0.5f, 0.7f, 1.0f);
    float skyStrength = 0.2f;

    while (!glfwWindowShouldClose(window)) {
        // --- INPUT: Use WASD to move the player sphere, NOT the camera ---
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            // Move "up" => decrease Z
            playerPos.z -= playerMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            // Move "down" => increase Z
            playerPos.z += playerMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            // Move "left" => decrease X
            playerPos.x -= playerMoveSpeed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            // Move "right" => increase X
            playerPos.x += playerMoveSpeed;
        }

        // --- RENDER ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build view matrix: camera is fixed
        glm::mat4 view = glm::lookAt(
                cameraPos,
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
        );

        // Orthographic projection => isometric style
        glm::mat4 projection = glm::ortho(
                -zoomLevel, zoomLevel,
                -zoomLevel, zoomLevel,
                -10.f, 10.f
        );

        // Use our main shader
        glUseProgram(shaderProgram);

        // Common uniforms
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "skyColor"), 1, glm::value_ptr(skyColor));
        glUniform1f(glGetUniformLocation(shaderProgram, "skyStrength"), skyStrength);

        int useSolidLoc   = glGetUniformLocation(shaderProgram, "useSolidColor");
        int solidColorLoc = glGetUniformLocation(shaderProgram, "solidColor");

        // Pass view & projection
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 1) Draw the grid of cubes
        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureForCubes);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

        glUniform1i(useSolidLoc, 0);  // Use texture, not solid color
        for (int i = 0; i < gridSize; ++i) {
            for (int j = 0; j < gridSize; ++j) {
                glm::mat4 model(1.0f);
                // Each cell is placed around (i - gridSize/2, j - gridSize/2)
                model = glm::translate(model, glm::vec3(
                        i - gridSize / 2.0f,
                        0.0f,
                        j - gridSize / 2.0f
                ));
                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

                glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
            }
        }

        // 2) Draw the player sphere
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, playerPos);

            // If you want the sphere to rotate, you can add rotation here.
            // But for a simple "marker", no rotation is needed.

            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

            // Use a solid color
            glUniform1i(useSolidLoc, 1);
            glm::vec3 playerColor(1.0f, 0.2f, 0.2f); // e.g. red
            glUniform3fv(solidColorLoc, 1, glm::value_ptr(playerColor));

            glBindVertexArray(sphereVAO);

            // Typically # of indices for a sphere: 6 * (sectors) * (stacks-1)
            // We used 'sectors=16' and 'stacks=16' => 6 * 16 * (16-1) = 6 * 16 * 15 = 1440
            int sphereIndexCount = 6 * sectors * (stacks - 1);
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
