//
// Created by User on 19/3/2025.
//

#include "Camera.h"
#include <cmath>
#include <iostream>

// Constructor sets default positions, directions, etc.
Camera::Camera()
        : mode_(CameraMode::Isometric),
          isoCamPos_(2.0f, 2.0f, 2.0f),
          freeCamPos_(2.0f, 2.0f, 2.0f),
          cameraFront_(0.0f, 0.0f, -1.0f),
          cameraUp_(0.0f, 1.0f,  0.0f),
          yaw_(-90.f),
          pitch_(0.f),
          firstMouse_(true),
          lastX_(400.f),
          lastY_(300.f),
          freeCamSpeed_(0.05f),
          mouseSensitivity_(0.1f),
          zoomLevel_(10.f) // default
{
}

void Camera::toggleMode()
{
    if (mode_ == CameraMode::Isometric)
        mode_ = CameraMode::Free;
    else
        mode_ = CameraMode::Isometric;

    firstMouse_ = true; // so mouse offsets don’t jump
}

// Update isometric camera – in your example code you keep it fixed at isoCamPos_,
// but if you want to animate it or do something else, you can put that logic here.
void Camera::updateIsometric()
{
    // The isometric “camera” doesn’t move in your sample.
    // If you want to do dynamic changes, handle them here.
}

// Update free camera
void Camera::updateFree(float deltaTime, bool upArrow, bool downArrow, bool leftArrow, bool rightArrow)
{
    // Move freeCamPos_ with arrow keys (or WASD, if you prefer).
    // We compute direction vectors for forward/back, left/right.
    glm::vec3 forward  = cameraFront_;
    glm::vec3 rightVec = glm::normalize(glm::cross(cameraFront_, cameraUp_));

    if (upArrow)
    {
        freeCamPos_ += forward * freeCamSpeed_ * deltaTime;
    }
    if (downArrow)
    {
        freeCamPos_ -= forward * freeCamSpeed_ * deltaTime;
    }
    if (leftArrow)
    {
        freeCamPos_ -= rightVec * freeCamSpeed_ * deltaTime;
    }
    if (rightArrow)
    {
        freeCamPos_ += rightVec * freeCamSpeed_ * deltaTime;
    }
}

// Handle mouse movement only for free camera
void Camera::handleMouse(double xpos, double ypos)
{
    if (mode_ != CameraMode::Free) {
        return;
    }

    if (firstMouse_) {
        lastX_ = float(xpos);
        lastY_ = float(ypos);
        firstMouse_ = false;
    }

    float xoffset = float(xpos) - lastX_;
    float yoffset = lastY_ - float(ypos); // reversed: screen coords
    lastX_ = float(xpos);
    lastY_ = float(ypos);

    xoffset *= mouseSensitivity_;
    yoffset *= mouseSensitivity_;

    yaw_   += xoffset;
    pitch_ += yoffset;

    // Constrain pitch
    if (pitch_ > 89.0f)  pitch_ = 89.0f;
    if (pitch_ < -89.0f) pitch_ = -89.0f;

    // Recompute cameraFront_
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    direction.y = sin(glm::radians(pitch_));
    direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    cameraFront_ = glm::normalize(direction);
}

// Return the current view matrix based on mode
glm::mat4 Camera::getViewMatrix() const
{
    if (mode_ == CameraMode::Isometric)
    {
        // Look from isoCamPos_ at the origin
        return glm::lookAt(isoCamPos_, glm::vec3(0.0f, 0.0f, 0.0f), cameraUp_);
    }
    else
    {
        // Free camera
        return glm::lookAt(freeCamPos_, freeCamPos_ + cameraFront_, cameraUp_);
    }
}