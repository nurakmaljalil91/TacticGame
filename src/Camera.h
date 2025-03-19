//
// Created by User on 19/3/2025.
//

#ifndef TACTICGAME_CAMERA_H
#define TACTICGAME_CAMERA_H


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMode
{
    Isometric,
    Free
};

class Camera
{
public:
    Camera();

    void toggleMode();
    CameraMode getMode() const { return mode_; }

    // Update camera each frame (handle keyboard, etc.)
    //  - pass in whatever input state you want (e.g. GLFW keys)
    void updateIsometric();
    void updateFree(float deltaTime, bool upArrow, bool downArrow, bool leftArrow, bool rightArrow);

    // Mouse movement for free camera
    void handleMouse(double xpos, double ypos);

    // Return current view matrix based on the mode
    glm::mat4 getViewMatrix() const;

    // Orthographic “zoom” for isometric
    float& zoomLevel() { return zoomLevel_; }

    // Positions (if you need them for lighting calculations, etc.)
    const glm::vec3& freeCamPos() const { return freeCamPos_; }
    const glm::vec3& isoCamPos()  const { return isoCamPos_; }

private:
    CameraMode mode_;

    // For isometric camera
    glm::vec3 isoCamPos_;

    // For free camera
    glm::vec3 freeCamPos_;
    glm::vec3 cameraFront_;
    glm::vec3 cameraUp_;

    // Euler angles
    float yaw_;
    float pitch_;

    // For mouse
    bool  firstMouse_;
    float lastX_;
    float lastY_;

    // Speeds, etc.
    float freeCamSpeed_;
    float mouseSensitivity_;

    // Zoom for orthographic isometric camera
    float zoomLevel_;
};


#endif //TACTICGAME_CAMERA_H
