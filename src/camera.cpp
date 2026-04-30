#include "camera.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Camera
{
    static glm::vec3 position    = {0.0f, 0.05f, 0.5f};
    static float     yaw         = -90.0f;
    static float     pitch       = 0.0f;
    static float     moveSpeed   = 2.0f;
    static float     sensitivity = 0.1f;
    static float     fov         = 60.0f;
    static float     nearPlane   = 0.01f;
    static float     farPlane    = 100.0f;

    static glm::vec3 forward()
    {
        float y = glm::radians(yaw);
        float p = glm::radians(pitch);
        return glm::normalize(glm::vec3{
            std::cos(y) * std::cos(p),
            std::sin(p),
            std::sin(y) * std::cos(p)
        });
    }

    void init(glm::vec3 pos, float startYaw, float startPitch)
    {
        position = pos;
        yaw      = startYaw;
        pitch    = startPitch;
    }

    void processKeyboard(GLFWwindow* window, float dt)
    {
        float     speed = moveSpeed * dt;
        glm::vec3 fwd   = forward();
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3{0, 1, 0}));
        glm::vec3 up    = {0, 1, 0};

        if (glfwGetKey(window, GLFW_KEY_W)            == GLFW_PRESS) position += fwd   * speed;
        if (glfwGetKey(window, GLFW_KEY_S)            == GLFW_PRESS) position -= fwd   * speed;
        if (glfwGetKey(window, GLFW_KEY_A)            == GLFW_PRESS) position -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D)            == GLFW_PRESS) position += right * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) position += up    * speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) position -= up    * speed;
    }

    void processMouse(float dx, float dy)
    {
        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity; // subtract: screen Y down = look down
        pitch  = std::clamp(pitch, -89.0f, 89.0f);
    }

    glm::mat4 getMVP(float aspect, glm::mat4 model)
    {
        glm::mat4 view = glm::lookAt(position, position + forward(), glm::vec3{0, 1, 0});
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        proj[1][1] *= -1; // Vulkan Y flip
        return proj * view * model;
    }
}
