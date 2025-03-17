#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>

// Grid Size
const int GRID_SIZE = 10;
const float TILE_SIZE = 1.0f;

// Camera Settings
float cameraAngle = 45.0f; // Rotation around Y-axis
float cameraHeight = 10.0f; // Height above the grid
float cameraDistance = 15.0f; // Distance from grid center
const float minZoom = 5.0f;  // Minimum zoom distance
const float maxZoom = 25.0f; // Maximum zoom distance

void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_LEFT) {
            cameraAngle += 5.0f; // Rotate left
        }
        if (key == GLFW_KEY_RIGHT) {
            cameraAngle -= 5.0f; // Rotate right
        }
        if (key == GLFW_KEY_UP) {
            cameraDistance -= 1.0f; // Zoom in
            if (cameraDistance < minZoom) cameraDistance = minZoom;
        }
        if (key == GLFW_KEY_DOWN) {
            cameraDistance += 1.0f; // Zoom out
            if (cameraDistance > maxZoom) cameraDistance = maxZoom;
        }
    }
}


// Generates an isometric view matrix with zoom
glm::mat4 GetCameraViewMatrix() {
    float angleRad = glm::radians(cameraAngle);
    glm::vec3 cameraPos = glm::vec3(
            cos(angleRad) * cameraDistance,
            cameraHeight,
            sin(angleRad) * cameraDistance
    );

    return glm::lookAt(cameraPos, glm::vec3(GRID_SIZE / 2.0f, 0.0f, GRID_SIZE / 2.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

// Grid vertices generator (fixed to start at most-left/front)
std::vector<float> GenerateGridVertices() {
    std::vector<float> vertices;
    float offset = 0.0f; // Start at (0,0)

    // Vertical lines (along Z-axis)
    for (int i = 0; i <= GRID_SIZE; ++i) {
        float x = offset + i * TILE_SIZE;
        vertices.push_back(x); vertices.push_back(0); vertices.push_back(offset);
        vertices.push_back(x); vertices.push_back(0); vertices.push_back(offset + GRID_SIZE * TILE_SIZE);
    }

    // Horizontal lines (along X-axis)
    for (int i = 0; i <= GRID_SIZE; ++i) {
        float z = offset + i * TILE_SIZE;
        vertices.push_back(offset); vertices.push_back(0); vertices.push_back(z);
        vertices.push_back(offset + GRID_SIZE * TILE_SIZE); vertices.push_back(0); vertices.push_back(z);
    }

    return vertices;
}
// Shader Source Code
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 projection;
    uniform mat4 view;
    void main() {
        gl_Position = projection * view * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(0.8, 0.8, 0.8, 1.0);
    }
)";


unsigned int CompileShader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader Compilation Error: " << infoLog << std::endl;
    }
    return shader;
}

unsigned int CreateShaderProgram() {
    unsigned int vertexShader = CompileShader(vertexShaderSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = CompileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader Linking Error: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

void RenderGrid(unsigned int shaderProgram, unsigned int VAO) {
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, GRID_SIZE * 4 + 4);
}

int main() {
    if (!glfwInit()) return -1;

    GLFWwindow* window = glfwCreateWindow(800, 600, "Isometric Tactical RPG", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, KeyCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Create Shader Program
    unsigned int shaderProgram = CreateShaderProgram();

    // Generate Grid Data
    std::vector<float> vertices = GenerateGridVertices();
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Projection Matrix (for isometric perspective)
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = GetCameraViewMatrix(); // Get updated camera matrix

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);


        RenderGrid(shaderProgram, VAO);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
