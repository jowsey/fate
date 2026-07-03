#include "Renderer.h"

#include <print>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

#define VOLK_IMPLEMENTATION
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vulkan/vk_enum_string_helper.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.inl"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"

#include "spdlog/spdlog.h"

#include "CubemapData.h"
#include "GPUMeshHandle.h"
#include "TextureData.h"
#include "Scene.h"
#include "utils/Files.h"
#include "utils/Paths.h"

namespace Fate {
    void Renderer::vkChk(const VkResult result) {
        if (result != VK_SUCCESS) {
            spdlog::error("Vulkan call returned an error ({})", string_VkResult(result));
            std::exit(result);
        }
    }

    void Renderer::vkChkSwapchain(const VkResult result) {
        if (result < VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                updateSwapchain = true;
                return;
            }

            spdlog::error("Vulkan call returned an error ({}),", string_VkResult(result));
            std::exit(result);
        }
    }

    std::vector<std::uint32_t> loadShader(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open shader file: " + path.string());
        }

        const std::size_t fileSize = file.tellg();
        std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));

        file.seekg(0);
        file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

    Renderer::Renderer() {
        if (!(SDL_Init(SDL_INIT_VIDEO) && SDL_Vulkan_LoadLibrary(nullptr))) {
            spdlog::error("Failed to initialize SDL: {}", SDL_GetError());
            std::exit(-1);
        }

        volkInitialize();

        // Instance
        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "fate", // todo pull from a project file?
            .pEngineName = "fate",
            .engineVersion = VK_MAKE_VERSION(FATE_VERSION_MAJOR, FATE_VERSION_MINOR, FATE_VERSION_PATCH),
            .apiVersion = VK_API_VERSION_1_3
        };

        const char* validationLayers[] = {
            "VK_LAYER_KHRONOS_validation"
        };

        std::uint32_t sdlExtensionCount{0};
        char const* const* sdlExtensions{SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount)};

        std::vector<char const *> instanceExtensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        VkInstanceCreateInfo instanceCI{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = validationLayers,
            .enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data()
        };
        vkChk(vkCreateInstance(&instanceCI, nullptr, &instance));
        volkLoadInstance(instance);

        // Device
        std::uint32_t physicalDeviceCount{0};
        vkChk(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr));
        std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
        vkChk(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data()));

        std::uint32_t physicalDeviceIndex{0};
        physicalDevice = devices[physicalDeviceIndex];

        VkPhysicalDeviceProperties2 physDeviceProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        vkGetPhysicalDeviceProperties2(physicalDevice, &physDeviceProperties);
        spdlog::info("Selected physical device: {}", physDeviceProperties.properties.deviceName);

        // Find a queue family for graphics
        std::uint32_t queueFamilyCount{0};
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        for (std::size_t i = 0; i < queueFamilies.size(); i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queueFamilyIndex = i;
                break;
            }
        }

        if (!SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, queueFamilyIndex)) {
            spdlog::error("Selected device does not support presentation to the window surface");
            std::exit(-1);
        }

        // Logical device
        constexpr float queueFamilyPriorities{1.0f};
        VkDeviceQueueCreateInfo queueCI{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamilyIndex, .queueCount = 1, .pQueuePriorities = &queueFamilyPriorities};

        VkPhysicalDeviceFeatures enabledVk10Features{
            .multiDrawIndirect = VK_TRUE,
            .samplerAnisotropy = VK_TRUE
        };
        VkPhysicalDeviceVulkan12Features enabledVk12Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing = true,
            .shaderSampledImageArrayNonUniformIndexing = true,
            .descriptorBindingSampledImageUpdateAfterBind = true,
            .descriptorBindingPartiallyBound = true,
            .descriptorBindingVariableDescriptorCount = true,
            .runtimeDescriptorArray = true,
            .bufferDeviceAddress = true
        };
        VkPhysicalDeviceVulkan13Features enabledVk13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &enabledVk12Features,
            .synchronization2 = true,
            .dynamicRendering = true
        };

        const std::vector<const char *> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo deviceCI{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &enabledVk13Features,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCI,
            .enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = &enabledVk10Features
        };
        vkChk(vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device));
        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

        // VMA
        VmaVulkanFunctions vkFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr, .vkCreateImage = vkCreateImage};
        VmaAllocatorCreateInfo allocatorCI{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT, .physicalDevice = physicalDevice, .device = device, .pVulkanFunctions = &vkFunctions, .instance = instance};
        vkChk(vmaCreateAllocator(&allocatorCI, &allocator));

        // Window and surface
        window = SDL_CreateWindow("fate", 1280u, 720u, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            spdlog::error("Failed to create Vulkan surface: {}", SDL_GetError());
            std::exit(-1);
        }
        SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

        VkSurfaceCapabilitiesKHR surfaceCaps{};
        vkChk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));
        VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
        if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
            swapchainExtent = {.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y)};
        }

        spdlog::debug("Window created with size {}x{}", windowSize.x, windowSize.y);

        // Swap chain
        VkSwapchainCreateInfoKHR swapchainCI{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = surfaceCaps.minImageCount,
            .imageFormat = FrameImageFormat,
            .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .imageExtent{.width = swapchainExtent.width, .height = swapchainExtent.height},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR
        };
        vkChk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

        std::uint32_t swapchainImageCount{0};
        vkChk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr));
        swapchainImages.resize(swapchainImageCount);
        vkChk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()));
        swapchainImageViews.resize(swapchainImageCount);

        for (auto i = 0; i < swapchainImageCount; i++) {
            VkImageViewCreateInfo viewCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = FrameImageFormat,
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
            };
            vkChk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
        }

        spdlog::trace("Swapchain created with {} images", swapchainImageCount);

        // Depth attachment
        std::vector<VkFormat> idealDepthFormats{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat& idealFormat: idealDepthFormats) {
            VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
            vkGetPhysicalDeviceFormatProperties2(physicalDevice, idealFormat, &formatProperties);
            if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depthImageFormat = idealFormat;
                break;
            }
        }

        VkImageCreateInfo depthImageCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthImageFormat,
            .extent{.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y), .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VmaAllocationCreateInfo allocCI{.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        vkChk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));
        VkImageViewCreateInfo depthViewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = depthImage, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depthImageFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
        vkChk(vkCreateImageView(device, &depthViewCI, nullptr, &depthImageView));

        spdlog::debug("Selected depth attachment format {}", string_VkFormat(depthImageFormat));

        // Geometry buffers
        VmaAllocationCreateInfo geometryBuffersAllocCI{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };
        VmaVirtualBlockCreateInfo geometryVirtualBlockCI{.size = GeometryBuffersSize};

        VkBufferCreateInfo vertexBufferCI{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = GeometryBuffersSize,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        };
        vkChk(vmaCreateBuffer(allocator, &vertexBufferCI, &geometryBuffersAllocCI, &vertexBuffer, &vertexBufferAllocation, &vertexBufferAllocationInfo));
        vkChk(vmaCreateVirtualBlock(&geometryVirtualBlockCI, &vertexVirtualBlock));

        VkBufferCreateInfo indexBufferCI{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = GeometryBuffersSize,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        };
        vkChk(vmaCreateBuffer(allocator, &indexBufferCI, &geometryBuffersAllocCI, &indexBuffer, &indexBufferAllocation, &indexBufferAllocationInfo));
        vkChk(vmaCreateVirtualBlock(&geometryVirtualBlockCI, &indexVirtualBlock));

        // Shader data buffers
        // todo abstract this to helper
        for (auto i = 0; i < MaxFramesInFlight; i++) {
            VmaAllocationCreateInfo bufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};

            // Skybox globals
            VkBufferCreateInfo skyboxGlobalsBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(SkyboxGlobals), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
            vkChk(vmaCreateBuffer(allocator, &skyboxGlobalsBufferCI, &bufferAllocCI, &skyboxGlobalsBuffers[i].buffer, &skyboxGlobalsBuffers[i].allocation, &skyboxGlobalsBuffers[i].allocationInfo));
            VkBufferDeviceAddressInfo skyboxGlobalsBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = skyboxGlobalsBuffers[i].buffer};
            skyboxGlobalsBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &skyboxGlobalsBufferBdaInfo);

            // Frame globals
            VkBufferCreateInfo frameGlobalsBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(FrameGlobals), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
            vkChk(vmaCreateBuffer(allocator, &frameGlobalsBufferCI, &bufferAllocCI, &frameGlobalsBuffers[i].buffer, &frameGlobalsBuffers[i].allocation, &frameGlobalsBuffers[i].allocationInfo));
            VkBufferDeviceAddressInfo frameGlobalsBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = frameGlobalsBuffers[i].buffer};
            frameGlobalsBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &frameGlobalsBufferBdaInfo);

            // Object datas
            VkBufferCreateInfo objectsDataBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(ObjectData) * MaxObjects, .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
            vkChk(vmaCreateBuffer(allocator, &objectsDataBufferCI, &bufferAllocCI, &objectDataBuffers[i].buffer, &objectDataBuffers[i].allocation, &objectDataBuffers[i].allocationInfo));
            VkBufferDeviceAddressInfo objectsDataBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = objectDataBuffers[i].buffer};
            objectDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &objectsDataBufferBdaInfo);

            // Draw commands
            VkBufferCreateInfo drawCommandsBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(VkDrawIndexedIndirectCommand) * MaxObjects, .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT};
            vkChk(vmaCreateBuffer(allocator, &drawCommandsBufferCI, &bufferAllocCI, &indirectBuffers[i].buffer, &indirectBuffers[i].allocation, &indirectBuffers[i].allocationInfo));
            VkBufferDeviceAddressInfo drawCommandsBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indirectBuffers[i].buffer};
            indirectBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &drawCommandsBufferBdaInfo);
        }

        // Sync objects
        VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        for (auto i = 0; i < MaxFramesInFlight; i++) {
            vkChk(vkCreateFence(device, &fenceCI, nullptr, &fences[i]));
            vkChk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAcquiredSemaphores[i]));
        }
        renderCompleteSemaphores.resize(swapchainImages.size());
        for (auto& semaphore: renderCompleteSemaphores) {
            vkChk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
        }

        // Command pool
        VkCommandPoolCreateInfo commandPoolCI{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = queueFamilyIndex};
        vkChk(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool));
        VkCommandBufferAllocateInfo cbAllocCI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .commandBufferCount = MaxFramesInFlight};
        vkChk(vkAllocateCommandBuffers(device, &cbAllocCI, commandBuffers.data()));

        // Descriptor sets
        VkDescriptorBindingFlags descBindingFlags{VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
        VkDescriptorSetLayoutBindingFlagsCreateInfo textureDescSetBindingFlagsCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &descBindingFlags};
        VkDescriptorSetLayoutBinding textureDescSetLayoutBinding{.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MaxTextureDescriptors, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
        VkDescriptorSetLayoutCreateInfo textureDescSetLayoutCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &textureDescSetBindingFlagsCI, .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT, .bindingCount = 1, .pBindings = &textureDescSetLayoutBinding};
        vkChk(vkCreateDescriptorSetLayout(device, &textureDescSetLayoutCI, nullptr, &textureDescriptorSetLayout));

        VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MaxTextureDescriptors};
        VkDescriptorPoolCreateInfo descPoolCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize};
        vkChk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO, .descriptorSetCount = 1, .pDescriptorCounts = &MaxTextureDescriptors};
        VkDescriptorSetAllocateInfo textureDescSetAI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = &variableDescCountAI, .descriptorPool = descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &textureDescriptorSetLayout};
        vkChk(vkAllocateDescriptorSets(device, &textureDescSetAI, &textureDescriptorSet));

        // Build default samplers
        VkSamplerCreateInfo samplerLinearRepeatCI{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = VK_LOD_CLAMP_NONE
        };
        vkChk(vkCreateSampler(device, &samplerLinearRepeatCI, nullptr, &samplers.linearRepeat));

        VkSamplerCreateInfo samplerLinearClampCI{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = VK_LOD_CLAMP_NONE
        };
        vkChk(vkCreateSampler(device, &samplerLinearClampCI, nullptr, &samplers.linearClamp));

        // Geometry pipeline
        VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .size = sizeof(VkDeviceAddress) * 2};
        VkPipelineLayoutCreateInfo pipelineLayoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &textureDescriptorSetLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConstantRange};
        vkChk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &geometryPipelineLayout));

        std::vector<std::uint32_t> litShaderCode{loadShader(PathUtils::getEnginePath() / "resources/Shaders/lit.slang.spv")};
        VkShaderModuleCreateInfo litShaderCI{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = litShaderCode.size() * sizeof(std::uint32_t),
            .pCode = litShaderCode.data()
        };
        vkChk(vkCreateShaderModule(device, &litShaderCI, nullptr, &litShaderModule));

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = litShaderModule, .pName = "vertexMain"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = litShaderModule, .pName = "fragmentMain"}
        };
        VkVertexInputBindingDescription vertexBinding{.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> vertexAttributes{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, baseColour)},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
            {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, tangent)},
            {.location = 4, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, texCoord)},
        };
        VkPipelineVertexInputStateCreateInfo vertexInputState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexBinding,
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexAttributes.size()),
            .pVertexAttributeDescriptions = vertexAttributes.data(),
        };
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamicStates.data()};
        VkPipelineViewportStateCreateInfo viewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
        VkPipelineRasterizationStateCreateInfo rasterizationState{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f};
        VkPipelineMultisampleStateCreateInfo multisampleState{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
        VkPipelineDepthStencilStateCreateInfo depthStencilState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
        VkPipelineColorBlendAttachmentState blendAttachment{.colorWriteMask = 0xF};
        VkPipelineColorBlendStateCreateInfo colorBlendState{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &blendAttachment};
        VkPipelineRenderingCreateInfo renderingCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &FrameImageFormat, .depthAttachmentFormat = depthImageFormat};
        VkGraphicsPipelineCreateInfo pipelineCI{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &renderingCI,
            .stageCount = 2,
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputState,
            .pInputAssemblyState = &inputAssemblyState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizationState,
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &depthStencilState,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = &dynamicState,
            .layout = geometryPipelineLayout
        };
        vkChk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &geometryPipeline));

        // Skybox pipeline
        VkPushConstantRange skyboxPushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .size = sizeof(VkDeviceAddress) * 1};
        VkPipelineLayoutCreateInfo skyboxPipelineLayoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &textureDescriptorSetLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &skyboxPushConstantRange};
        vkChk(vkCreatePipelineLayout(device, &skyboxPipelineLayoutCI, nullptr, &skyboxPipelineLayout));

        std::vector<std::uint32_t> skyboxShaderCode{loadShader(PathUtils::getEnginePath() / "resources/Shaders/skybox.slang.spv")};
        VkShaderModuleCreateInfo skyboxShaderCI{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = skyboxShaderCode.size() * sizeof(std::uint32_t),
            .pCode = skyboxShaderCode.data()
        };
        vkChk(vkCreateShaderModule(device, &skyboxShaderCI, nullptr, &skyboxShaderModule));

        std::vector<VkPipelineShaderStageCreateInfo> skyboxShaderStages{
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = skyboxShaderModule, .pName = "vertexMain"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = skyboxShaderModule, .pName = "fragmentMain"}
        };
        VkPipelineVertexInputStateCreateInfo skyboxVertexInputState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .vertexAttributeDescriptionCount = 0,
        };

        VkPipelineDepthStencilStateCreateInfo skyboxDepthStencilState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_FALSE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
        VkGraphicsPipelineCreateInfo skyboxPipelineCI{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &renderingCI,
            .stageCount = 2,
            .pStages = skyboxShaderStages.data(),
            .pVertexInputState = &skyboxVertexInputState,
            .pInputAssemblyState = &inputAssemblyState,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizationState,
            .pMultisampleState = &multisampleState,
            .pDepthStencilState = &skyboxDepthStencilState,
            .pColorBlendState = &colorBlendState,
            .pDynamicState = &dynamicState,
            .layout = skyboxPipelineLayout
        };
        vkChk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyboxPipelineCI, nullptr, &skyboxPipeline));

        spdlog::debug("Graphics pipelines created");

        // ImGui general setup
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.Fonts->AddFontFromFileTTF((PathUtils::getEnginePath() / "resources/Fonts/Inter_18pt-Regular.ttf").string().c_str());

        constexpr auto bgLight = ImColor(42, 42, 42);
        constexpr auto bgDark = ImColor(26, 26, 26);
        constexpr auto border = ImColor(64, 64, 64);

        ImGuiStyle& style = ImGui::GetStyle();
        style.FontSizeBase = 16.0f;

        style.WindowRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.FrameRounding = 2.0f;

        style.PopupBorderSize = 0.0f;

        style.Colors[ImGuiCol_WindowBg] = bgLight;
        style.Colors[ImGuiCol_TitleBg] = bgDark;
        style.Colors[ImGuiCol_TitleBgActive] = bgDark;
        style.Colors[ImGuiCol_MenuBarBg] = bgDark;
        style.Colors[ImGuiCol_Border] = border;
        style.Colors[ImGuiCol_PopupBg] = bgDark;

        float contentScale = SDL_GetWindowDisplayScale(window);
        style.FontScaleDpi = contentScale;
        style.ScaleAllSizes(contentScale);

        // ImGui Vulkan/SDL impls setup
        ImGui_ImplSDL3_InitForVulkan(window);
        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = instance,
            .PhysicalDevice = physicalDevice,
            .Device = device,
            .QueueFamily = queueFamilyIndex,
            .Queue = queue,
            .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
            .MinImageCount = 2,
            .ImageCount = 2,
            .PipelineCache = VK_NULL_HANDLE,
            .PipelineInfoMain = {
                .RenderPass = VK_NULL_HANDLE,
                .Subpass = 0,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &FrameImageFormat,
                    .depthAttachmentFormat = depthImageFormat
                }
            },
            .UseDynamicRendering = true,
            .Allocator = VK_NULL_HANDLE,
            .CheckVkResultFn = vkChk,
        };
        ImGui_ImplVulkan_Init(&initInfo);

        spdlog::debug("ImGui initialized");

        // set window icon
        // todo port to Wuffs
        SDL_Surface* iconSurface = SDL_LoadPNG((PathUtils::getEnginePath() / "resources/Textures/fate-icon.png").string().c_str());
        if (iconSurface) {
            SDL_SetWindowIcon(window, iconSurface);
            SDL_DestroySurface(iconSurface);
        }

        spdlog::info("Renderer initialised");
    }

    Renderer::~Renderer() {
        spdlog::info("Renderer shutting down");

        vkChk(vkDeviceWaitIdle(device));
        for (auto i = 0; i < MaxFramesInFlight; i++) {
            vkDestroyFence(device, fences[i], nullptr);
            vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
            vmaDestroyBuffer(allocator, skyboxGlobalsBuffers[i].buffer, skyboxGlobalsBuffers[i].allocation);
            vmaDestroyBuffer(allocator, frameGlobalsBuffers[i].buffer, frameGlobalsBuffers[i].allocation);
            vmaDestroyBuffer(allocator, objectDataBuffers[i].buffer, objectDataBuffers[i].allocation);
            vmaDestroyBuffer(allocator, indirectBuffers[i].buffer, indirectBuffers[i].allocation);
        }

        for (const auto& renderCompleteSemaphore: renderCompleteSemaphores) {
            vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);
        }

        vmaDestroyImage(allocator, depthImage, depthImageAllocation);
        vkDestroyImageView(device, depthImageView, nullptr);
        for (const auto& swapchainImageView: swapchainImageViews) {
            vkDestroyImageView(device, swapchainImageView, nullptr);
        }

        for (const auto& allocatedTexture: allocatedTextures) {
            vkDestroyImageView(device, allocatedTexture->view, nullptr);
            vmaDestroyImage(allocator, allocatedTexture->image, allocatedTexture->allocation);
        }

        vkDestroySampler(device, samplers.linearRepeat, nullptr);
        vkDestroySampler(device, samplers.linearClamp, nullptr);

        vmaDestroyBuffer(allocator, vertexBuffer, vertexBufferAllocation);
        vertexVirtualBlock->Clear();
        vmaDestroyVirtualBlock(vertexVirtualBlock);

        vmaDestroyBuffer(allocator, indexBuffer, indexBufferAllocation);
        indexVirtualBlock->Clear();
        vmaDestroyVirtualBlock(indexVirtualBlock);

        vkDestroyPipelineLayout(device, geometryPipelineLayout, nullptr);
        vkDestroyPipeline(device, geometryPipeline, nullptr);
        vkDestroyShaderModule(device, litShaderModule, nullptr);

        vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr);
        vkDestroyPipeline(device, skyboxPipeline, nullptr);
        vkDestroyShaderModule(device, skyboxShaderModule, nullptr);

        vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);
        vmaDestroyAllocator(allocator);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit();

        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void DrawSceneHierarchyNode(SceneTransform& transform) {
        if (ImGui::TreeNode(transform.getObject().getName().c_str())) {
            auto position = transform.getPosition();
            auto eulerAngles = transform.getEulerAngles();
            auto scale = transform.getLocalScale();

            if (ImGui::DragScalarN("Position", ImGuiDataType_Double, &position.x, 3, 0.01f)) {
                transform.setPosition(position);
            }
            if (ImGui::DragFloat3("Rotation", &eulerAngles.x, 0.1f)) {
                transform.setEulerAngles(eulerAngles);
            }
            if (ImGui::DragFloat3("Scale", &scale.x, 0.01f)) {
                transform.setLocalScale(scale);
            }

            for (std::size_t i = 0; i < transform.getObject().getMeshes().size(); ++i) {
                const auto mesh = transform.getObject().getMeshes()[i];
                const auto material = mesh->getMaterial();

                ImGui::SeparatorText(("Mesh " + std::to_string(i)).c_str());
                ImGui::Text("%zu vertices, %zu indices", mesh->getVertices().size(), mesh->getIndices().size());

                ImGui::SliderFloat("Metallic", &material->metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &material->roughness, 0.0f, 1.0f);
                ImGui::ColorEdit4("Base colour", &material->baseColour.x);

                ImGui::Text("Albedo map:");
                ImGui::SameLine();
                ImGui::TextColored(material->albedoMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->albedoMap ? "yes" : "no");
                ImGui::Text("Normal map:");
                ImGui::SameLine();
                ImGui::TextColored(material->normalMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->normalMap ? "yes" : "no");
                ImGui::Text("Ambient map:");
                ImGui::SameLine();
                ImGui::TextColored(material->ambientMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->ambientMap ? "yes" : "no");
                ImGui::Text("Roughness map:");
                ImGui::SameLine();
                ImGui::TextColored(material->roughnessMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->roughnessMap ? "yes" : "no");
                ImGui::Text("Metallic map:");
                ImGui::SameLine();
                ImGui::TextColored(material->metallicMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->metallicMap ? "yes" : "no");
                ImGui::Text("Emissive map:");
                ImGui::SameLine();
                ImGui::TextColored(material->emissiveMap ? ImColor(0, 255, 0) : ImColor(255, 0, 0), "%s", material->emissiveMap ? "yes" : "no");
            }

            for (SceneTransform* childTransform: transform.getChildren()) {
                DrawSceneHierarchyNode(*childTransform);
            }

            ImGui::TreePop();
        }
    }

    // todo this should probably be in Engine and call in to Renderer separately
    void Renderer::buildEditorUI(const Scene& scene, const double deltaTime) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) {
                    SDL_Event quitEvent;
                    quitEvent.type = SDL_EVENT_QUIT;
                    SDL_PushEvent(&quitEvent);
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("View on GitHub")) {
                    PathUtils::openBrowser("https://github.com/jowsey/fate");
                }

                ImGui::Separator();

                ImGui::MenuItem("the Fate game engine", nullptr, nullptr, false);
                ImGui::MenuItem("v" FATE_VERSION, nullptr, nullptr, false);

                ImGui::EndMenu();
            }

            constexpr std::uint32_t frameBacklog{5};
            static std::deque<double> deltaTimeBuffer(frameBacklog);
            if (deltaTimeBuffer.size() >= frameBacklog) {
                deltaTimeBuffer.pop_front();
            }
            deltaTimeBuffer.push_back(deltaTime);

            const double averageDeltaTime = deltaTimeBuffer.empty()
                                                ? 0.0
                                                : std::accumulate(deltaTimeBuffer.begin(), deltaTimeBuffer.end(), 0.0) / deltaTimeBuffer.size();

            const std::string fpsString = std::format("{:.3} fps", 1.0 / averageDeltaTime);
            const float fpsSize = ImGui::CalcTextSize(fpsString.c_str()).x;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - fpsSize - 8.0f);
            ImGui::TextUnformatted(fpsString.c_str());

            ImGui::EndMainMenuBar();
        }

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::Begin("Debug");

        ImGui::DragScalarN("Camera position", ImGuiDataType_Double, &cameraPosition, 3, 0.01f);
        ImGui::DragFloat3("Camera rotation", &cameraRotation.x, 0.1f);
        ImGui::DragFloat("Camera FOV", &cameraHorFovDegs, 0.1f);

        ImGui::DragFloat3("Light direction", &lightDir.x, 0.01f);
        ImGui::ColorEdit3("Light colour", &lightColor.x);
        ImGui::DragFloat("Light intensity", &lightIntensity, 0.01f);

        if (ImGui::CollapsingHeader("Hierarchy", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (SceneObject* object: scene.getObjects()) {
                if (object->getTransform().getParent() != nullptr) continue;
                DrawSceneHierarchyNode(object->getTransform());
            }
        }

        if (ImGui::CollapsingHeader("Resource usage", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
            // todo profile, maybe cache
            VmaStatistics vertexBufferStats;
            vmaGetVirtualBlockStatistics(vertexVirtualBlock, &vertexBufferStats);
            VmaStatistics indexBufferStats;
            vmaGetVirtualBlockStatistics(indexVirtualBlock, &indexBufferStats);

            const float vertexUsage = static_cast<float>(vertexBufferStats.allocationBytes) / static_cast<float>(vertexBufferStats.blockBytes);
            const float indexUsage = static_cast<float>(indexBufferStats.allocationBytes) / static_cast<float>(indexBufferStats.blockBytes);

            ImGui::ProgressBar(
                vertexUsage,
                ImVec2(-1.0f, 0.0f),
                std::format("Vertex buffer: {} ({:.3f}%)", FileUtils::prettyBytes(vertexBufferStats.allocationBytes), vertexUsage * 100.0f).c_str()
            );

            ImGui::ProgressBar(
                indexUsage,
                ImVec2(-1.0f, 0.0f),
                std::format("Index buffer: {} ({:.3f}%)", FileUtils::prettyBytes(indexBufferStats.allocationBytes), indexUsage * 100.0f).c_str()
            );

            // todo can we pull from descriptor set? or store manually again
            // ImGui::ProgressBar(
            //     0.0f,
            //     ImVec2(-1.0f, 0.0f),
            //     std::format("Texture usage: {}", FileUtils::prettyBytes(textureUploadedBytes)).c_str()
            // );
        }

        ImGui::End();
    }

    void Renderer::render(const Scene& scene) {
        // Wait to acquire next frame image
        vkChk(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
        vkChk(vkResetFences(device, 1, &fences[frameIndex]));
        vkChkSwapchain(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex));

        // Build globals buffers
        glm::mat4 viewMatrix = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(cameraPosition)) * glm::mat4_cast(glm::quat(glm::radians(cameraRotation))));

        const float winAspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
        const float fovYRads = 2.0f * glm::atan(glm::tan(glm::radians(cameraHorFovDegs) * 0.5f) / winAspect);

        glm::mat4 projectionMatrix = glm::perspective(fovYRads, winAspect, 0.01f, 100.0f);
        projectionMatrix[1][1] *= -1.0f; // flip Y for Vulkan

        if (scene.getSkybox()) {
            skyboxGlobals.view = viewMatrix;
            skyboxGlobals.projection = projectionMatrix;
            skyboxGlobals.cubemapIndex = scene.getSkybox()->descriptorIndex;

            std::memcpy(skyboxGlobalsBuffers[frameIndex].allocationInfo.pMappedData, &skyboxGlobals, sizeof(SkyboxGlobals));
        }

        frameGlobals.view = viewMatrix;
        frameGlobals.projection = projectionMatrix;
        frameGlobals.cameraPosition = glm::vec3(cameraPosition);

        frameGlobals.light = {
            .direction = lightDir,
            .range = 0.0f, // unused
            .colour = lightColor,
            .intensity = lightIntensity
        };

        std::memcpy(frameGlobalsBuffers[frameIndex].allocationInfo.pMappedData, &frameGlobals, sizeof(FrameGlobals));

        // Generate object data & draw commands from meshes
        std::vector<VkDrawIndexedIndirectCommand> drawCommands; // todo cache
        objectDatas.clear();

        std::uint32_t objectIndex = 0;

        for (const auto& object: scene.getObjects()) {
            for (const auto& mesh: object->getMeshes()) {
                VkDrawIndexedIndirectCommand command{
                    .indexCount = static_cast<std::uint32_t>(mesh->getIndices().size()),
                    .instanceCount = 1,
                    .firstIndex = mesh->getGPUHandle()->indicesOffset,
                    .vertexOffset = static_cast<std::int32_t>(mesh->getGPUHandle()->verticesOffset),
                    .firstInstance = objectIndex++
                };
                drawCommands.push_back(command);

                ObjectData objectData{
                    .model = object->getTransform().getWorldMatrix(),
                    .inverseTransposeModel = glm::transpose(glm::inverse(object->getTransform().getWorldMatrix())),
                    .material = {
                        .baseColour = mesh->getMaterial()->baseColour,
                        .albedoMapIndex = mesh->getMaterial()->albedoMap ? mesh->getMaterial()->albedoMap->descriptorIndex : 0,
                        .normalMapIndex = mesh->getMaterial()->normalMap ? mesh->getMaterial()->normalMap->descriptorIndex : 0,
                        .ambientMapIndex = mesh->getMaterial()->ambientMap ? mesh->getMaterial()->ambientMap->descriptorIndex : 0,
                        .roughnessMapIndex = mesh->getMaterial()->roughnessMap ? mesh->getMaterial()->roughnessMap->descriptorIndex : 0,
                        .metallicMapIndex = mesh->getMaterial()->metallicMap ? mesh->getMaterial()->metallicMap->descriptorIndex : 0,
                        .emissiveMapIndex = mesh->getMaterial()->emissiveMap ? mesh->getMaterial()->emissiveMap->descriptorIndex : 0,
                        .mapFlags = mesh->getMaterial()->mapFlags,
                        .roughness = mesh->getMaterial()->roughness,
                        .metallic = mesh->getMaterial()->metallic,
                    }
                };
                objectDatas.push_back(objectData);
            }
        }

        std::memcpy(indirectBuffers[frameIndex].allocationInfo.pMappedData, drawCommands.data(), drawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
        std::memcpy(objectDataBuffers[frameIndex].allocationInfo.pMappedData, objectDatas.data(), objectDatas.size() * sizeof(ObjectData));

        // Build command buffer
        auto commandBuffer = commandBuffers[frameIndex];
        vkChk(vkResetCommandBuffer(commandBuffer, 0));

        VkCommandBufferBeginInfo cbBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkChk(vkBeginCommandBuffer(commandBuffer, &cbBI));

        std::array<VkImageMemoryBarrier2, 2> outputBarriers{
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = swapchainImages[imageIndex],
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
            },
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = depthImage,
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1}
            }
        };
        VkDependencyInfo barrierDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 2, .pImageMemoryBarriers = outputBarriers.data()};
        vkCmdPipelineBarrier2(commandBuffer, &barrierDependencyInfo);

        VkRenderingAttachmentInfo colorAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainImageViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{.color{0.12f, 0.12f, 0.12f, 1.0f}}
        };
        VkRenderingAttachmentInfo depthAttachmentInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthImageView,
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {.depthStencil = {1.0f, 0}}
        };
        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea{.extent{.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y)}},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo
        };
        vkCmdBeginRendering(commandBuffer, &renderingInfo);

        VkViewport vp{.width = static_cast<float>(windowSize.x), .height = static_cast<float>(windowSize.y), .minDepth = 0.0f, .maxDepth = 1.0f};
        vkCmdSetViewport(commandBuffer, 0, 1, &vp);
        VkRect2D scissor{.extent{.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y)}};
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Geometry
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, geometryPipelineLayout, 0, 1, &textureDescriptorSet, 0, nullptr);

        VkDeviceSize vertexOffset{0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdPushConstants(commandBuffer, geometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkDeviceAddress), &frameGlobalsBuffers[frameIndex].deviceAddress);
        vkCmdPushConstants(commandBuffer, geometryPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &objectDataBuffers[frameIndex].deviceAddress);

        vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffers[frameIndex].buffer, 0, static_cast<std::uint32_t>(drawCommands.size()), sizeof(VkDrawIndexedIndirectCommand));

        // Skybox
        if (scene.getSkybox()) {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1, &textureDescriptorSet, 0, nullptr);

            vkCmdPushConstants(commandBuffer, skyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkDeviceAddress), &skyboxGlobalsBuffers[frameIndex].deviceAddress);
            vkCmdDraw(commandBuffer, 36, 1, 0, 0);
        }

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[frameIndex]);

        vkCmdEndRendering(commandBuffer);

        VkImageMemoryBarrier2 barrierPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchainImages[imageIndex],
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
        };
        VkDependencyInfo barrierPresentDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrierPresent};
        vkCmdPipelineBarrier2(commandBuffer, &barrierPresentDependencyInfo);
        vkChk(vkEndCommandBuffer(commandBuffer));

        // Submit to graphics queue
        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAcquiredSemaphores[frameIndex],
            .pWaitDstStageMask = &waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderCompleteSemaphores[imageIndex],
        };
        vkChk(vkQueueSubmit(queue, 1, &submitInfo, fences[frameIndex]));

        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        };
        vkChkSwapchain(vkQueuePresentKHR(queue, &presentInfo));

        frameIndex = (frameIndex + 1) % MaxFramesInFlight;

        // Update swapchain if necessary
        if (updateSwapchain) {
            updateSwapchain = false;

            VkSurfaceCapabilitiesKHR surfaceCapabilities;

            vkChk(vkDeviceWaitIdle(device));
            vkChk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

            SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

            // todo abstract into method + DRY constructor, maybe see if we can run this whole method in constructor too?
            VkSwapchainCreateInfoKHR swapchainCI{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = surface,
                .minImageCount = surfaceCapabilities.minImageCount,
                .imageFormat = FrameImageFormat,
                .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                .imageExtent{.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y)},
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                .oldSwapchain = swapchain,
            };

            vkChk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));
            for (auto i = 0; i < swapchainImageViews.size(); i++) {
                vkDestroyImageView(device, swapchainImageViews[i], nullptr);
            }

            std::uint32_t swapchainImageCount{0};
            vkChk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr));
            swapchainImages.resize(swapchainImageCount);
            vkChk(vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()));
            swapchainImageViews.resize(swapchainImageCount);

            for (auto i = 0; i < swapchainImageCount; i++) {
                VkImageViewCreateInfo viewCI{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = swapchainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = FrameImageFormat,
                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
                };
                vkChk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
            }

            for (auto& semaphore: renderCompleteSemaphores) {
                vkDestroySemaphore(device, semaphore, nullptr);
            }
            renderCompleteSemaphores.resize(swapchainImageCount);
            VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            for (auto& semaphore: renderCompleteSemaphores) {
                vkChk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
            }

            vkDestroySwapchainKHR(device, swapchainCI.oldSwapchain, nullptr);
            vmaDestroyImage(allocator, depthImage, depthImageAllocation);
            vkDestroyImageView(device, depthImageView, nullptr);

            VkImageCreateInfo depthImageCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depthImageFormat,
                .extent{.width = static_cast<uint32_t>(windowSize.x), .height = static_cast<uint32_t>(windowSize.y), .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };

            depthImageCI.extent = {.width = static_cast<uint32_t>(windowSize.x), .height = static_cast<uint32_t>(windowSize.y), .depth = 1};
            constexpr VmaAllocationCreateInfo allocCI{.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
            vkChk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));
            VkImageViewCreateInfo viewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = depthImage, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depthImageFormat, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
            vkChk(vkCreateImageView(device, &viewCI, nullptr, &depthImageView));
        }
    }

    GPUMeshHandle Renderer::uploadMesh(const Mesh& mesh) {
        const auto vertexBytes = mesh.getVertices().size() * sizeof(Vertex);
        const auto indexBytes = mesh.getIndices().size() * sizeof(std::uint32_t);

        const VmaVirtualAllocationCreateInfo vertexVirtualAllocCI{
            .size = vertexBytes,
            .alignment = 16
        };
        VmaVirtualAllocation vertexVirtualAllocation{};
        VkDeviceSize vertexOffset;
        vkChk(vmaVirtualAllocate(vertexVirtualBlock, &vertexVirtualAllocCI, &vertexVirtualAllocation, &vertexOffset));

        const VmaVirtualAllocationCreateInfo indexVirtualAllocCI{
            .size = indexBytes,
            .alignment = 16
        };
        VmaVirtualAllocation indexVirtualAllocation{};
        VkDeviceSize indexOffset;
        vkChk(vmaVirtualAllocate(indexVirtualBlock, &indexVirtualAllocCI, &indexVirtualAllocation, &indexOffset));

        std::memcpy(static_cast<char *>(vertexBufferAllocationInfo.pMappedData) + vertexOffset, mesh.getVertices().data(), vertexBytes);
        std::memcpy(static_cast<char *>(indexBufferAllocationInfo.pMappedData) + indexOffset, mesh.getIndices().data(), indexBytes);

        return GPUMeshHandle(vertexOffset / sizeof(Vertex), indexOffset / sizeof(std::uint32_t), vertexVirtualAllocation, indexVirtualAllocation);
    }

    AllocatedTexture* Renderer::uploadTexture(const TextureData& texture) {
        const auto pixelsSize = texture.width * texture.height * 4;
        spdlog::debug("Uploading texture of size {}x{} ({})", texture.width, texture.height, FileUtils::prettyBytes(pixelsSize));

        AllocatedTexture allocatedTexture{};

        VkImageCreateInfo imageCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = texture.colourSpace == TextureColourSpace::SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {.width = texture.width, .height = texture.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VmaAllocationCreateInfo imageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
        vkChk(vmaCreateImage(allocator, &imageCI, &imageAllocCI, &allocatedTexture.image, &allocatedTexture.allocation, nullptr));
        VkImageViewCreateInfo imageViewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = allocatedTexture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageCI.format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
        };
        vkChk(vkCreateImageView(device, &imageViewCI, nullptr, &allocatedTexture.view));

        // Upload
        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VkBufferCreateInfo imgSrcBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = pixelsSize, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo imgSrcAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        VmaAllocationInfo imgSrcAllocInfo{};
        vkChk(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo));
        std::memcpy(imgSrcAllocInfo.pMappedData, texture.pixels.get(), pixelsSize);

        VkCommandBuffer commandBuffer{};
        VkCommandBufferAllocateInfo commandBufferAI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .commandBufferCount = 1};
        vkChk(vkAllocateCommandBuffers(device, &commandBufferAI, &commandBuffer));

        VkCommandBufferBeginInfo commandBufferBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkChk(vkBeginCommandBuffer(commandBuffer, &commandBufferBI));
        VkImageMemoryBarrier2 barrierImageTransferDst{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = allocatedTexture.image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
        };
        VkDependencyInfo barrierImageInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrierImageTransferDst};
        vkCmdPipelineBarrier2(commandBuffer, &barrierImageInfo);

        // todo mipmaps
        // std::vector<VkBufferImageCopy> copyRegions{};
        // for (auto j = 0; j < kt->numLevels; j++) {
        //     ktx_size_t mipOffset{0};
        //     KTX_error_code ret = ktxTexture2_GetImageOffset(kt, j, 0, 0, &mipOffset);
        //     copyRegions.push_back({
        //         .bufferOffset = mipOffset,
        //         .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<std::uint32_t>(j), .layerCount = 1},
        //         .imageExtent{.width = kt->baseWidth >> j, .height = kt->baseHeight >> j, .depth = 1},
        //     });
        // }
        // vkCmdCopyBufferToImage(commandBuffer, imgSrcBuffer, allocatedTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copyRegions.size()), copyRegions.data());

        VkBufferImageCopy copyRegion{
            .bufferOffset = 0,
            .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
            .imageExtent{.width = texture.width, .height = texture.height, .depth = 1}
        };
        vkCmdCopyBufferToImage(commandBuffer, imgSrcBuffer, allocatedTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier2 barrierImageShaderRead{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = allocatedTexture.image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
        };
        barrierImageInfo.pImageMemoryBarriers = &barrierImageShaderRead;
        vkCmdPipelineBarrier2(commandBuffer, &barrierImageInfo);
        vkChk(vkEndCommandBuffer(commandBuffer));

        VkFence fence{};
        VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkChk(vkCreateFence(device, &fenceCI, nullptr, &fence));

        VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        vkChk(vkQueueSubmit(queue, 1, &submitInfo, fence));
        vkChk(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);

        std::uint32_t bindingIndex = textureDescriptorAllocator.getNext(); // todo free when unloading texture
        allocatedTexture.descriptorIndex = bindingIndex;

        VkDescriptorImageInfo textureImageInfo{.sampler = samplers.linearRepeat, .imageView = allocatedTexture.view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = textureDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = allocatedTexture.descriptorIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &textureImageInfo
        };
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        // don't need to resize if using a recycled/freed index
        if (bindingIndex >= allocatedTextures.size()) {
            allocatedTextures.resize(bindingIndex + 1);
        }

        auto uptr = std::make_unique<AllocatedTexture>(allocatedTexture);
        allocatedTextures[bindingIndex] = std::move(uptr);

        return allocatedTextures[bindingIndex].get();
    }

    AllocatedTexture* Renderer::uploadCubemap(const CubemapData& cubemap) {
        const auto faceBytes = cubemap.faceWidth * cubemap.faceHeight * 4;
        spdlog::debug("Uploading cubemap of size {}x{}[x6] ({})", cubemap.faceWidth, cubemap.faceHeight, FileUtils::prettyBytes(faceBytes * 6));

        // todo this can be *heavily* deduplicated, should just run through uploadTexture with some flags

        AllocatedTexture allocatedTexture{};

        VkImageCreateInfo imageCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .extent = {.width = cubemap.faceWidth, .height = cubemap.faceHeight, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 6,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VmaAllocationCreateInfo imageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
        vkChk(vmaCreateImage(allocator, &imageCI, &imageAllocCI, &allocatedTexture.image, &allocatedTexture.allocation, nullptr));

        VkImageViewCreateInfo imageViewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = allocatedTexture.image,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = imageCI.format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 6}
        };
        vkChk(vkCreateImageView(device, &imageViewCI, nullptr, &allocatedTexture.view));

        // Upload to staging buffer
        VkBuffer stagingBuffer{};
        VmaAllocation stagingBufferAlloc{};
        VkBufferCreateInfo stagingBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = faceBytes * 6, .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo stagingBufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        VmaAllocationInfo stagingBufferAllocInfo{};
        vkChk(vmaCreateBuffer(allocator, &stagingBufferCI, &stagingBufferAllocCI, &stagingBuffer, &stagingBufferAlloc, &stagingBufferAllocInfo));
        for (std::size_t i = 0; i < 6; ++i) {
            std::memcpy(static_cast<char *>(stagingBufferAllocInfo.pMappedData) + faceBytes * i, cubemap.faces[i].get(), faceBytes);
        }

        VkCommandBuffer commandBuffer{};
        VkCommandBufferAllocateInfo commandBufferAI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .commandBufferCount = 1};
        vkChk(vkAllocateCommandBuffers(device, &commandBufferAI, &commandBuffer));

        VkCommandBufferBeginInfo commandBufferBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkChk(vkBeginCommandBuffer(commandBuffer, &commandBufferBI));
        VkImageMemoryBarrier2 barrierImageTransferDst{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = allocatedTexture.image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 6}
        };
        VkDependencyInfo barrierImageInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrierImageTransferDst};
        vkCmdPipelineBarrier2(commandBuffer, &barrierImageInfo);

        std::vector<VkBufferImageCopy> regions;
        regions.reserve(6);
        for (std::uint32_t face = 0; face < 6; face++) {
            // todo could possibly be one region of layerCount = 6 since they're all the same?
            VkBufferImageCopy region{
                .bufferOffset = faceBytes * face,
                .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = face, .layerCount = 1},
                .imageExtent = {.width = cubemap.faceWidth, .height = cubemap.faceHeight, .depth = 1}
            };
            regions.push_back(region);
        }
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, allocatedTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions.data());

        VkImageMemoryBarrier2 barrierImageShaderRead{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = allocatedTexture.image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 6}
        };
        barrierImageInfo.pImageMemoryBarriers = &barrierImageShaderRead;
        vkCmdPipelineBarrier2(commandBuffer, &barrierImageInfo);
        vkChk(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        VkFence submitFence{};
        VkFenceCreateInfo submitFenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkChk(vkCreateFence(device, &submitFenceCI, nullptr, &submitFence));

        vkChk(vkQueueSubmit(queue, 1, &submitInfo, submitFence));
        vkChk(vkWaitForFences(device, 1, &submitFence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(device, submitFence, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAlloc);

        std::uint32_t bindingIndex = textureDescriptorAllocator.getNext(); // todo free when unloading texture
        allocatedTexture.descriptorIndex = bindingIndex;

        VkDescriptorImageInfo textureImageInfo{.sampler = samplers.linearClamp, .imageView = allocatedTexture.view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = textureDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = allocatedTexture.descriptorIndex,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &textureImageInfo
        };
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        // don't need to resize if using a recycled/freed index
        if (bindingIndex >= allocatedTextures.size()) {
            allocatedTextures.resize(bindingIndex + 1);
        }

        auto uptr = std::make_unique<AllocatedTexture>(allocatedTexture);
        allocatedTextures[bindingIndex] = std::move(uptr);

        return allocatedTextures[bindingIndex].get();
    }
}
