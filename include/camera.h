#pragma once
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Camera
{
    void      init(glm::vec3 pos = {0.0f, 0.1f, 0.5f},
                   float yaw = -90.0f, float pitch = 0.0f);
    void      processKeyboard(GLFWwindow* window, float dt);
    void      processMouse(float dx, float dy);
    glm::mat4 getMVP(float aspect);
    glm::vec3 getPosition();
    float     getFovY();
}
