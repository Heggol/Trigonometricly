#include <iostream>
#include <GLFW/glfw3.h>

#include "src/sine.hpp"
#include "src/InitVulkan.hpp"

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* m_mainWindow = glfwCreateWindow(800, 600, "Trigonometricly", nullptr, nullptr);
    if (!m_mainWindow) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    VulkanContext m_vulkanContext;

    try {
        // Initialize Vulkan
        InitVulkan::initialize(m_mainWindow, m_vulkanContext);

        // Main loop
        while (!glfwWindowShouldClose(m_mainWindow)) {
            glfwPollEvents();

            // Generate sine wave vertices
            auto sineVertices = sine::generateSineWave(0.5f, 1.0f, static_cast<float>(glfwGetTime()), 200);

            // Draw frame
            InitVulkan::renderFrame(m_vulkanContext, sineVertices);
        }

        // Cleanup Vulkan
        InitVulkan::cleanup(m_vulkanContext);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        InitVulkan::cleanup(m_vulkanContext);
        glfwDestroyWindow(m_mainWindow);
        glfwTerminate();
        return -1;
    }

    // Cleanup GLFW
    glfwDestroyWindow(m_mainWindow);
    glfwTerminate();

    return 0;
}