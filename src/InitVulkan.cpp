#include "InitVulkan.hpp"
#include <stdexcept>
#include <vector>
#include <optional>
#include <limits>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iostream>

#define VK_CHECK(fn)                              \
    if ((fn) != VK_SUCCESS)                       \
    {                                             \
        throw std::runtime_error("Vulkan error"); \
    }

// helper functions
static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        std::cerr << "Error: failed to open " << filename << std::endl;
        throw std::runtime_error("failed to open file");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

uint32_t findMemoryType(VkPhysicalDevice t_phys,
                        uint32_t t_typeFilter,
                        VkMemoryPropertyFlags t_props)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(t_phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
    {
        if ((t_typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & t_props) == t_props)
            return i;
    }
    throw std::runtime_error("No suitable memory type on physical device");
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> m_graphicsFamily;
    std::optional<uint32_t> m_presentFamily;
    bool isComplete() const
    {
        return m_graphicsFamily.has_value() && m_presentFamily.has_value();
    }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice t_dev,
                                            VkSurfaceKHR t_surf)
{
    QueueFamilyIndices idx;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(t_dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(t_dev, &count, props.data());

    for (uint32_t i = 0; i < count; i++)
    {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            idx.m_graphicsFamily = i;
        VkBool32 presentOK = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(t_dev, i, t_surf, &presentOK);
        if (presentOK)
            idx.m_presentFamily = i;
        if (idx.isComplete())
            break;
    }
    return idx;
}

struct SwapChainSupport
{
    VkSurfaceCapabilitiesKHR m_caps;
    std::vector<VkSurfaceFormatKHR> m_formats;
    std::vector<VkPresentModeKHR> m_presentModes;
};

static SwapChainSupport querySwapChainSupport(VkPhysicalDevice t_dev,
                                              VkSurfaceKHR t_surf)
{
    SwapChainSupport sup;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(t_dev, t_surf, &sup.m_caps);

    uint32_t cnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(t_dev, t_surf, &cnt, nullptr);
    if (cnt)
    {
        sup.m_formats.resize(cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(t_dev, t_surf, &cnt,
                                             sup.m_formats.data());
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(t_dev, t_surf, &cnt, nullptr);
    if (cnt)
    {
        sup.m_presentModes.resize(cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            t_dev, t_surf, &cnt, sup.m_presentModes.data());
    }
    return sup;
}

static VkSurfaceFormatKHR pickSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &t_avail)
{
    for (auto &f : t_avail)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return t_avail[0];
}

static VkPresentModeKHR pickPresentMode(
    const std::vector<VkPresentModeKHR> &t_avail)
{
    for (auto &m : t_avail)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D pickExtent(const VkSurfaceCapabilitiesKHR &t_caps,
                             GLFWwindow *t_window)
{
    if (!t_window)
    {
        throw std::runtime_error("GLFW window is null. Ensure to create a window.");
    }
    if (t_caps.currentExtent.width !=
        std::numeric_limits<uint32_t>::max())
        return t_caps.currentExtent;
    int w, h;
    glfwGetFramebufferSize(t_window, &w, &h);
    VkExtent2D actual{
        (uint32_t)w,
        (uint32_t)h};
    actual.width = std::clamp(actual.width,
                              t_caps.minImageExtent.width,
                              t_caps.maxImageExtent.width);
    actual.height = std::clamp(actual.height,
                               t_caps.minImageExtent.height,
                               t_caps.maxImageExtent.height);
    return actual;
}

static void createBuffer(VkPhysicalDevice t_phys,
                         VkDevice t_dev,
                         VkDeviceSize t_size,
                         VkBufferUsageFlags t_usage,
                         VkMemoryPropertyFlags t_props,
                         VkBuffer &t_buffer,
                         VkDeviceMemory &t_bufferMem)
{
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = t_size;
    bi.usage = t_usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(t_dev, &bi, nullptr, &t_buffer));

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(t_dev, t_buffer, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex =
        findMemoryType(t_phys, mr.memoryTypeBits, t_props);
    VK_CHECK(vkAllocateMemory(t_dev, &ai, nullptr, &t_bufferMem));
    vkBindBufferMemory(t_dev, t_buffer, t_bufferMem, 0);
}

// general Vulkan Declerations
namespace VulkanHelpers
{

    // Create Vulkan instance
    void createInstance(VulkanContext &t_context)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Trigonometricly";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        createInfo.enabledLayerCount = 0;

        if (vkCreateInstance(&createInfo, nullptr, &t_context.m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    // Create Vulkan surface
    void createSurface(GLFWwindow *t_window, VulkanContext &t_context)
    {
        if (glfwCreateWindowSurface(t_context.m_instance, t_window, nullptr, &t_context.m_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface");
        }
    }

    // Pick physical device
    void pickPhysicalDevice(VulkanContext &t_context)
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(t_context.m_instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(t_context.m_instance, &deviceCount, devices.data());

        for (const auto &device : devices)
        {
            if (true /* Add device suitability checks here */)
            {
                t_context.m_physicalDevice = device;
                break;
            }
        }

        if (t_context.m_physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find a suitable GPU");
        }
    }

    // Create logical device
    void createLogicalDevice(VulkanContext &t_context)
    {
        QueueFamilyIndices indices = findQueueFamilies(t_context.m_physicalDevice, t_context.m_surface);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.m_graphicsFamily.value(), indices.m_presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(t_context.m_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = t_context.m_deviceExtensions.data();

        if (vkCreateDevice(t_context.m_physicalDevice, &createInfo, nullptr, &t_context.m_device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(t_context.m_device, indices.m_graphicsFamily.value(), 0, &t_context.m_graphicsQueue);
        vkGetDeviceQueue(t_context.m_device, indices.m_presentFamily.value(), 0, &t_context.m_presentQueue);
    }

    // Create swap chain
    void createSwapChain(VulkanContext &t_context)
    {
        SwapChainSupport swapChainSupport = querySwapChainSupport(t_context.m_physicalDevice, t_context.m_surface);

        VkSurfaceFormatKHR surfaceFormat = pickSurfaceFormat(swapChainSupport.m_formats);
        VkPresentModeKHR presentMode = pickPresentMode(swapChainSupport.m_presentModes);
        VkExtent2D extent = pickExtent(swapChainSupport.m_caps, t_context.m_window);

        uint32_t imageCount = swapChainSupport.m_caps.minImageCount + 1;
        if (swapChainSupport.m_caps.maxImageCount > 0 && imageCount > swapChainSupport.m_caps.maxImageCount)
        {
            imageCount = swapChainSupport.m_caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = t_context.m_surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(t_context.m_physicalDevice, t_context.m_surface);
        uint32_t queueFamilyIndices[] = {indices.m_graphicsFamily.value(), indices.m_presentFamily.value()};

        if (indices.m_graphicsFamily != indices.m_presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.m_caps.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(t_context.m_device, &createInfo, nullptr, &t_context.m_swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(t_context.m_device, t_context.m_swapChain, &imageCount, nullptr);
        t_context.m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(t_context.m_device, t_context.m_swapChain, &imageCount, t_context.m_swapChainImages.data());

        t_context.m_swapChainImageFormat = surfaceFormat.format;
        t_context.m_swapChainExtent = extent;
    }

    // Create image views
    void createImageViews(VulkanContext &t_context)
    {
        t_context.m_swapChainImageViews.resize(t_context.m_swapChainImages.size());

        for (size_t i = 0; i < t_context.m_swapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = t_context.m_swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = t_context.m_swapChainImageFormat;

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(t_context.m_device, &createInfo, nullptr, &t_context.m_swapChainImageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }

    // Create render pass
    void createRenderPass(VulkanContext &t_context)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = t_context.m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(t_context.m_device, &renderPassInfo, nullptr, &t_context.m_renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    // Create graphics pipeline
    void createGraphicsPipeline(VulkanContext &t_context, VkPipelineShaderStageCreateInfo *t_shaderStages)
    {
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDescription{};
        attributeDescription.binding = 0;
        attributeDescription.location = 0;
        attributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescription.offset = offsetof(Vertex, position);

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = 1;
        vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(t_context.m_swapChainExtent.width);
        viewport.height = static_cast<float>(t_context.m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = t_context.m_swapChainExtent;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(t_context.m_device, &pipelineLayoutInfo, nullptr, &t_context.m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = t_shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = t_context.m_pipelineLayout;
        pipelineInfo.renderPass = t_context.m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(t_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &t_context.m_graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
    }

    void createFramebuffers(VulkanContext &t_context)
    {
        t_context.m_swapChainFramebuffers.resize(t_context.m_swapChainImageViews.size());

        for (size_t i = 0; i < t_context.m_swapChainImageViews.size(); i++)
        {
            VkImageView attachments[] = {
                t_context.m_swapChainImageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = t_context.m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = t_context.m_swapChainExtent.width;
            framebufferInfo.height = t_context.m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(t_context.m_device, &framebufferInfo, nullptr, &t_context.m_swapChainFramebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void createCommandPool(VulkanContext &t_context)
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(t_context.m_physicalDevice, t_context.m_surface);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.m_graphicsFamily.value();
        poolInfo.flags = 0;

        if (vkCreateCommandPool(t_context.m_device, &poolInfo, nullptr, &t_context.m_commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    void createCommandBuffers(VulkanContext &t_context)
    {
        t_context.m_commandBuffers.resize(t_context.m_swapChainFramebuffers.size());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = t_context.m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)t_context.m_commandBuffers.size();

        if (vkAllocateCommandBuffers(t_context.m_device, &allocInfo, t_context.m_commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void createSyncObjects(VulkanContext &t_context)
    {
        t_context.m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        t_context.m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        t_context.m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(t_context.m_device, &semaphoreInfo, nullptr, &t_context.m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(t_context.m_device, &semaphoreInfo, nullptr, &t_context.m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(t_context.m_device, &fenceInfo, nullptr, &t_context.m_inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create synchronization objects for a frame!");
            }
        }
    }

    void createVertexBuffer(VulkanContext &t_context)
    {
        VkDeviceSize bufferSize = sizeof(Vertex) * 200; // Example size

        createBuffer(t_context.m_physicalDevice, t_context.m_device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, t_context.m_vertexBuffer, t_context.m_vertexBufferMemory);
    }

} // namespace VulkanHelpers

// Add shader module creation
VkShaderModule createShaderModule(const std::vector<char> &t_code, VkDevice t_device)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = t_code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(t_code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(t_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shader module");
    }

    return shaderModule;
}

// Initialize Vulkan
namespace InitVulkan
{
    void initialize(GLFWwindow *t_window, VulkanContext &t_context)
    {
        if (t_window == nullptr)
        {
            throw std::runtime_error("GLFW window is null. Ensure the window is created before initializing Vulkan.");
        }

        t_context.m_window = t_window;

        VulkanHelpers::createInstance(t_context);
        VulkanHelpers::createSurface(t_window, t_context);
        VulkanHelpers::pickPhysicalDevice(t_context);
        VulkanHelpers::createLogicalDevice(t_context);
        VulkanHelpers::createSwapChain(t_context);
        VulkanHelpers::createImageViews(t_context);
        VulkanHelpers::createRenderPass(t_context);

        // Load shaders
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, t_context.m_device);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, t_context.m_device);

        // Update pipeline creation to use shaders
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // Pass shaderStages to VulkanHelpers::createGraphicsPipeline
        VulkanHelpers::createGraphicsPipeline(t_context, shaderStages);

        // Destroy shader modules after pipeline creation
        vkDestroyShaderModule(t_context.m_device, vertShaderModule, nullptr);
        vkDestroyShaderModule(t_context.m_device, fragShaderModule, nullptr);

        VulkanHelpers::createGraphicsPipeline(t_context, shaderStages);
        VulkanHelpers::createFramebuffers(t_context);
        VulkanHelpers::createCommandPool(t_context);
        VulkanHelpers::createCommandBuffers(t_context);
        VulkanHelpers::createSyncObjects(t_context);
        VulkanHelpers::createVertexBuffer(t_context);
    }

    void renderFrame(VulkanContext &t_context, const std::vector<Vertex> &t_vertices)
    {
        vkWaitForFences(t_context.m_device, 1,
                        &t_context.m_inFlightFences[t_context.m_currentFrame],
                        VK_TRUE,
                        UINT64_MAX);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(
            t_context.m_device,
            t_context.m_swapChain,
            UINT64_MAX,
            t_context.m_imageAvailableSemaphores[t_context.m_currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);

        // update vertex buffer
        void *data;
        vkMapMemory(t_context.m_device,
                    t_context.m_vertexBufferMemory,
                    0,
                    t_vertices.size() * sizeof(Vertex),
                    0,
                    &data);
        memcpy(data, t_vertices.data(),
               t_vertices.size() * sizeof(Vertex));
        vkUnmapMemory(t_context.m_device, t_context.m_vertexBufferMemory);

        // record command buffer
        vkResetCommandBuffer(t_context.m_commandBuffers[imageIndex], 0);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(t_context.m_commandBuffers[imageIndex], &bi);

        VkRenderPassBeginInfo rpbi{};
        rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass = t_context.m_renderPass;
        rpbi.framebuffer = t_context.m_swapChainFramebuffers[imageIndex];
        rpbi.renderArea.offset = {0, 0};
        rpbi.renderArea.extent = t_context.m_swapChainExtent;
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpbi.clearValueCount = 1;
        rpbi.pClearValues = &clearColor;

        vkCmdBeginRenderPass(t_context.m_commandBuffers[imageIndex],
                             &rpbi,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(t_context.m_commandBuffers[imageIndex],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          t_context.m_graphicsPipeline);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(t_context.m_commandBuffers[imageIndex],
                               0, 1,
                               &t_context.m_vertexBuffer,
                               offsets);
        vkCmdDraw(t_context.m_commandBuffers[imageIndex],
                  static_cast<uint32_t>(t_vertices.size()),
                  1, 0, 0);
        vkCmdEndRenderPass(t_context.m_commandBuffers[imageIndex]);
        vkEndCommandBuffer(t_context.m_commandBuffers[imageIndex]);

        // submit command buffer
        VkSemaphore waitSemaphores[] = {
            t_context.m_imageAvailableSemaphores[t_context.m_currentFrame]};
        VkSemaphore signalSemaphores[] = {
            t_context.m_renderFinishedSemaphores[t_context.m_currentFrame]};
        VkPipelineStageFlags waitStages[] = {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = waitSemaphores;
        si.pWaitDstStageMask = waitStages;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &t_context.m_commandBuffers[imageIndex];
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = signalSemaphores;

        vkResetFences(t_context.m_device, 1,
                      &t_context.m_inFlightFences[t_context.m_currentFrame]);
        VK_CHECK(vkQueueSubmit(t_context.m_graphicsQueue,
                               1, &si,
                               t_context.m_inFlightFences[t_context.m_currentFrame]));

        // present image
        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = signalSemaphores;
        pi.swapchainCount = 1;
        pi.pSwapchains = &t_context.m_swapChain;
        pi.pImageIndices = &imageIndex;
        vkQueuePresentKHR(t_context.m_presentQueue, &pi);

        t_context.m_currentFrame =
            (t_context.m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup(VulkanContext &t_context)
    {
        vkDeviceWaitIdle(t_context.m_device);

        vkDestroyBuffer(t_context.m_device, t_context.m_vertexBuffer, nullptr);
        vkFreeMemory(t_context.m_device, t_context.m_vertexBufferMemory, nullptr);

        for (auto &f : t_context.m_inFlightFences)
            vkDestroyFence(t_context.m_device, f, nullptr);
        for (auto &s : t_context.m_renderFinishedSemaphores)
            vkDestroySemaphore(t_context.m_device, s, nullptr);
        for (auto &s : t_context.m_imageAvailableSemaphores)
            vkDestroySemaphore(t_context.m_device, s, nullptr);

        vkDestroyCommandPool(t_context.m_device, t_context.m_commandPool, nullptr);

        for (auto &fb : t_context.m_swapChainFramebuffers)
            vkDestroyFramebuffer(t_context.m_device, fb, nullptr);

        vkDestroyPipeline(t_context.m_device, t_context.m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(t_context.m_device, t_context.m_pipelineLayout, nullptr);
        vkDestroyRenderPass(t_context.m_device, t_context.m_renderPass, nullptr);

        for (auto &iv : t_context.m_swapChainImageViews)
            vkDestroyImageView(t_context.m_device, iv, nullptr);

        vkDestroySwapchainKHR(t_context.m_device, t_context.m_swapChain, nullptr);
        vkDestroyDevice(t_context.m_device, nullptr);
        vkDestroySurfaceKHR(t_context.m_instance, t_context.m_surface, nullptr);
        vkDestroyInstance(t_context.m_instance, nullptr);
    }
}