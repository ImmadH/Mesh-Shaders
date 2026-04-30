#include "camera.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace Camera
{
    static glm::vec3 position    = {0.0f, 0.05f, 0.5f};
    static float     yaw         = -90.0f;
    static float     pitch       = 0.0f;
    static float     moveSpeed   = 0.5f;
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

    glm::mat4 getMVP(float aspect)
    {
        glm::mat4 view = glm::lookAt(position, position + forward(), glm::vec3{0, 1, 0});
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        proj[1][1] *= -1; // Vulkan Y flip
        return proj * view;
    }
}

void getFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6])
{
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j) {
            float sign = j ? 1.f : -1.f;
            for (int k = 0; k < 4; ++k)
                planes[2 * i + j][k] = vp[k][3] + sign * vp[k][i];
        }

    for (int i = 0; i < 6; ++i)
        planes[i] /= glm::length(glm::vec3(planes[i]));
}

bool isVisible(glm::vec4 planes[6], glm::vec3 center, float radius)
{
    // skip near(2,3), test left(0) right(1) top(4) bottom(5)
    std::array<int, 4> idx{0, 1, 4, 5};
    for (int i : idx)
        if (glm::dot(center, glm::vec3(planes[i])) + planes[i].w + radius < 0)
            return false;
    return true;
}
