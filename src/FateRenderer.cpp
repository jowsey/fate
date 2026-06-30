#include "FateRenderer.h"

#include <print>
#include <cwchar>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.inl"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"
#include "SDL3/SDL_timer.h"

#include "ktx.h"
#include "ktxvulkan.h"

#include "GPUMeshHandle.h"
#include "TextureData.h"
#include "Scene.h"
#include "utils/Files.h"
#include "utils/Paths.h"

void FateRenderer::vkChk(const VkResult result) {
    if (result != VK_SUCCESS) {
        std::println(stderr, "Vulkan call returned an error ({})", static_cast<int>(result));
        std::exit(result);
    }
}

void FateRenderer::vkChkSwapchain(const VkResult result) {
    if (result < VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            updateSwapchain = true;
            return;
        }

        std::println(stderr, "Vulkan call returned an error ({}),", static_cast<int>(result));
        std::exit(result);
    }
}

std::vector<std::uint32_t> loadSPIRV(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    const std::size_t fileSize = file.tellg();

    // SPIR-V alignment requires data to be in 32-bit (4-byte) chunks
    std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

FateRenderer::FateRenderer() {
    if (!(SDL_Init(SDL_INIT_VIDEO) && SDL_Vulkan_LoadLibrary(nullptr))) {
        std::println(stderr, "Failed to initialize SDL: {}", SDL_GetError());
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
    std::uint32_t instanceExtensionsCount{0};
    char const* const* instanceExtensions{SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount)};
    VkInstanceCreateInfo instanceCI{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = instanceExtensionsCount,
        .ppEnabledExtensionNames = instanceExtensions,
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
    std::cout << "Selected physical device: " << physDeviceProperties.properties.deviceName << "\n";

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
        std::println(stderr, "Selected device does not support presentation to the window surface");
        std::exit(-1);
    }

    // Logical device
    constexpr float queueFamilyPriorities{1.0f};
    VkDeviceQueueCreateInfo queueCI{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = queueFamilyIndex, .queueCount = 1, .pQueuePriorities = &queueFamilyPriorities};
    VkPhysicalDeviceVulkan12Features enabledVk12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .descriptorIndexing = true, .shaderSampledImageArrayNonUniformIndexing = true, .descriptorBindingVariableDescriptorCount = true, .runtimeDescriptorArray = true, .bufferDeviceAddress = true};
    VkPhysicalDeviceVulkan13Features enabledVk13Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &enabledVk12Features, .synchronization2 = true, .dynamicRendering = true};
    VkPhysicalDeviceFeatures enabledVk10Features{.samplerAnisotropy = VK_TRUE};
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
        std::println(stderr, "Failed to create Vulkan surface: {}", SDL_GetError());
        std::exit(-1);
    }
    SDL_GetWindowSize(window, &windowSize.x, &windowSize.y);

    VkSurfaceCapabilitiesKHR surfaceCaps{};
    vkChk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));
    VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
    if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
        swapchainExtent = {.width = static_cast<std::uint32_t>(windowSize.x), .height = static_cast<std::uint32_t>(windowSize.y)};
    }

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
    std::uint32_t imageCount{0};
    vkChk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    swapchainImages.resize(imageCount);
    vkChk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));
    swapchainImageViews.resize(imageCount);
    for (auto i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = swapchainImages[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = FrameImageFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        vkChk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
    }

    // Depth attachment
    std::vector<VkFormat> depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    for (VkFormat& format: depthFormatList) {
        VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFormat = format;
            break;
        }
    }
    assert(depthFormat != VK_FORMAT_UNDEFINED);
    VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
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
    VkImageViewCreateInfo depthViewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = depthImage, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = depthFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
    vkChk(vkCreateImageView(device, &depthViewCI, nullptr, &depthImageView));

    // Mesh data
    // todo migrate from explicit resources to scene tree generated from assimp GLTF loader
    // tinyobj::attrib_t attrib;
    // std::vector<tinyobj::shape_t> shapes;
    // std::vector<tinyobj::material_t> materials;
    // vkChk(tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, "../resources/o_box.obj"));
    // const VkDeviceSize indexCount{shapes[0].mesh.indices.size()};
    // std::vector<Vertex> vertices{};
    // std::vector<std::uint16_t> indices{};
    //
    // // Load vertex and index data
    // for (auto& index: shapes[0].mesh.indices) {
    //     Vertex v{
    //         .position = {attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2]},
    //         .normal = {attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2]},
    //         .texCoord = {attrib.texcoords[index.texcoord_index * 2], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1]}
    //     };
    //     vertices.push_back(v);
    //     indices.push_back(indices.size());
    // }
    //
    // // Allocate geometry buffer
    // VkDeviceSize vBufferSize{sizeof(Vertex) * vertices.size()};
    // VkDeviceSize iBufferSize{sizeof(std::uint16_t) * indices.size()};
    // VkBufferCreateInfo bufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = vBufferSize + iBufferSize, .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};
    // VmaAllocationCreateInfo vBufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
    // VmaAllocationInfo vBufferAllocInfo{};
    // vkChk(vmaCreateBuffer(allocator, &bufferCI, &vBufferAllocCI, &geometryBuffer, &geometryBufferAllocation, &vBufferAllocInfo));
    // memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufferSize);
    // memcpy(static_cast<char *>(vBufferAllocInfo.pMappedData) + vBufferSize, indices.data(), iBufferSize);

    // Geometry buffer
    VkBufferCreateInfo geometryBufferCI{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = GeometryBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };
    VmaAllocationCreateInfo geometryBufferAllocationCI{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    vkChk(vmaCreateBuffer(allocator, &geometryBufferCI, &geometryBufferAllocationCI, &geometryBuffer, &geometryBufferAllocation, &geometryBufferAllocationInfo));

    VmaVirtualBlockCreateInfo virtualBlockCI{.size = GeometryBufferSize};
    vkChk(vmaCreateVirtualBlock(&virtualBlockCI, &geometryVirtualBlock));

    // Shader data buffers
    for (auto i = 0; i < MaxFramesInFlight; i++) {
        VkBufferCreateInfo uBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = sizeof(ShaderData), .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo uBufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        vkChk(vmaCreateBuffer(allocator, &uBufferCI, &uBufferAllocCI, &shaderDataBuffers[i].buffer, &shaderDataBuffers[i].allocation, &shaderDataBuffers[i].allocationInfo));
        VkBufferDeviceAddressInfo uBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = shaderDataBuffers[i].buffer};
        shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &uBufferBdaInfo);
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

    // Texture images
    // todo migrate from explicit resources to traversing scene tree objects
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (auto i = 0; i < textures.size(); i++) {
        ktxTexture* ktxTexture{nullptr};
        std::string filename = "../resources/ss010.ktx";
        ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
        VkImageCreateInfo texImgCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(ktxTexture),
            .extent = {.width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1},
            .mipLevels = ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VmaAllocationCreateInfo texImageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
        vkChk(vmaCreateImage(allocator, &texImgCI, &texImageAllocCI, &textures[i].image, &textures[i].allocation, nullptr));
        VkImageViewCreateInfo texVewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = textures[i].image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = texImgCI.format, .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1}};
        vkChk(vkCreateImageView(device, &texVewCI, nullptr, &textures[i].view));

        // Upload
        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VkBufferCreateInfo imgSrcBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = static_cast<std::uint32_t>(ktxTexture->dataSize), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo imgSrcAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
        VmaAllocationInfo imgSrcAllocInfo{};
        vkChk(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo));
        memcpy(imgSrcAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);
        VkFenceCreateInfo fenceOneTimeCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fenceOneTime{};
        vkChk(vkCreateFence(device, &fenceOneTimeCI, nullptr, &fenceOneTime));
        VkCommandBuffer cbOneTime{};
        VkCommandBufferAllocateInfo cbOneTimeAI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .commandBufferCount = 1};
        vkChk(vkAllocateCommandBuffers(device, &cbOneTimeAI, &cbOneTime));
        VkCommandBufferBeginInfo cbOneTimeBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        vkChk(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
        VkImageMemoryBarrier2 barrierTexImage{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = textures[i].image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1}
        };
        VkDependencyInfo barrierTexInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrierTexImage};
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        std::vector<VkBufferImageCopy> copyRegions{};
        for (auto j = 0; j < ktxTexture->numLevels; j++) {
            ktx_size_t mipOffset{0};
            KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, j, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<std::uint32_t>(j), .layerCount = 1},
                .imageExtent{.width = ktxTexture->baseWidth >> j, .height = ktxTexture->baseHeight >> j, .depth = 1},
            });
        }
        vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(copyRegions.size()), copyRegions.data());
        VkImageMemoryBarrier2 barrierTexRead{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = textures[i].image,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = ktxTexture->numLevels, .layerCount = 1}
        };
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        vkChk(vkEndCommandBuffer(cbOneTime));
        VkSubmitInfo oneTimeSI{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cbOneTime};
        vkChk(vkQueueSubmit(queue, 1, &oneTimeSI, fenceOneTime));
        vkChk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
        vkDestroyFence(device, fenceOneTime, nullptr);
        vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);

        // Sampler
        VkSamplerCreateInfo samplerCI{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f,
            .maxLod = static_cast<float>(ktxTexture->numLevels),
        };
        vkChk(vkCreateSampler(device, &samplerCI, nullptr, &textures[i].sampler));
        ktxTexture_Destroy(ktxTexture);
        textureDescriptors.push_back({.sampler = textures[i].sampler, .imageView = textures[i].view, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
    }

    // Descriptors
    VkDescriptorBindingFlags descVariableFlag{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &descVariableFlag};
    VkDescriptorSetLayoutBinding descLayoutBindingTex{.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<std::uint32_t>(textures.size()), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = &descBindingFlags, .bindingCount = 1, .pBindings = &descLayoutBindingTex};
    vkChk(vkCreateDescriptorSetLayout(device, &descLayoutTexCI, nullptr, &textureDescriptorSetLayout));
    VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<std::uint32_t>(textures.size())};
    VkDescriptorPoolCreateInfo descPoolCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize};
    vkChk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));
    std::uint32_t variableDescCount{static_cast<std::uint32_t>(textures.size())};
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT, .descriptorSetCount = 1, .pDescriptorCounts = &variableDescCount};
    VkDescriptorSetAllocateInfo texDescSetAlloc{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = &variableDescCountAI, .descriptorPool = descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &textureDescriptorSetLayout};
    vkChk(vkAllocateDescriptorSets(device, &texDescSetAlloc, &textureDescriptorSet));
    VkWriteDescriptorSet writeDescSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = textureDescriptorSet, .dstBinding = 0, .descriptorCount = static_cast<std::uint32_t>(textureDescriptors.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = textureDescriptors.data()};
    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

    // Load shader
    std::vector<std::uint32_t> vertCode{loadSPIRV("shader.vert.spv")};
    std::vector<std::uint32_t> fragCode{loadSPIRV("shader.frag.spv")};

    VkShaderModuleCreateInfo vertCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertCode.size() * sizeof(std::uint32_t),
        .pCode = vertCode.data()
    };
    VkShaderModule vertModule{};
    vkChk(vkCreateShaderModule(device, &vertCI, nullptr, &vertModule));

    VkShaderModuleCreateInfo fragCI{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragCode.size() * sizeof(std::uint32_t),
        .pCode = fragCode.data()
    };
    VkShaderModule fragModule{};
    vkChk(vkCreateShaderModule(device, &fragCI, nullptr, &fragModule));

    // Pipeline
    VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VkDeviceAddress)};
    VkPipelineLayoutCreateInfo pipelineLayoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &textureDescriptorSetLayout, .pushConstantRangeCount = 1, .pPushConstantRanges = &pushConstantRange};
    vkChk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertModule, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragModule, .pName = "main"}
    };
    VkVertexInputBindingDescription vertexBinding{.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    std::vector<VkVertexInputAttributeDescription> vertexAttributes{
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, texCoord)},
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
    VkPipelineRenderingCreateInfo renderingCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &FrameImageFormat, .depthAttachmentFormat = depthFormat};
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
        .layout = pipelineLayout
    };
    vkChk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    // imgui
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

    ImGui_ImplSDL3_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .QueueFamily = queueFamilyIndex,
        .Queue = queue,
        .DescriptorPool = descriptorPool,
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
            }
        },
        .UseDynamicRendering = true,
        .Allocator = VK_NULL_HANDLE,
        .CheckVkResultFn = vkChk,
    };

    ImGui_ImplVulkan_Init(&initInfo);

    // set window icon
    // todo port to Wuffs
    SDL_Surface* iconSurface = SDL_LoadPNG((PathUtils::getEnginePath() / "resources/Textures/fate-icon.png").string().c_str());
    if (iconSurface) {
        SDL_SetWindowIcon(window, iconSurface);
        SDL_DestroySurface(iconSurface);
    }
}

FateRenderer::~FateRenderer() {
    vkChk(vkDeviceWaitIdle(device));
    for (auto i = 0; i < MaxFramesInFlight; i++) {
        vkDestroyFence(device, fences[i], nullptr);
        vkDestroySemaphore(device, imageAcquiredSemaphores[i], nullptr);
        vmaDestroyBuffer(allocator, shaderDataBuffers[i].buffer, shaderDataBuffers[i].allocation);
    }
    for (auto& renderCompleteSemaphore: renderCompleteSemaphores) {
        vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);
    }

    vmaDestroyImage(allocator, depthImage, depthImageAllocation);
    vkDestroyImageView(device, depthImageView, nullptr);
    for (auto& swapchainImageView: swapchainImageViews) {
        vkDestroyImageView(device, swapchainImageView, nullptr);
    }
    vmaDestroyBuffer(allocator, geometryBuffer, geometryBufferAllocation);

    for (auto& texture: textures) {
        vkDestroyImageView(device, texture.view, nullptr);
        vkDestroySampler(device, texture.sampler, nullptr);
        vmaDestroyImage(allocator, texture.image, texture.allocation);
    }

    vkDestroyDescriptorSetLayout(device, textureDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    // vkDestroyShaderModule(device, vertModule, nullptr);
    // vkDestroyShaderModule(device, fragModule, nullptr); // todo vulkan: hook up to idk
    vmaDestroyAllocator(allocator);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void DrawSceneHierarchyNode(SceneTransform& transform) {
    if (ImGui::TreeNode(transform.getObject().getName().c_str())) {
        auto position = transform.getPosition();

        if (ImGui::DragScalarN("Position", ImGuiDataType_Double, &position, 3, 0.01f)) {
            transform.setPosition(position);
        }

        for (std::size_t i = 0; i < transform.getObject().getMeshes().size(); ++i) {
            const auto mesh = transform.getObject().getMeshes()[i];
            const auto material = mesh->getMaterial();

            if (ImGui::CollapsingHeader(("Mesh " + std::to_string(i)).c_str())) {
                ImGui::Text("%zu vertices, %zu indices", mesh->getVertices().size(), mesh->getIndices().size());
                // ImGui::Text("Has albedo map: %s", material->albedoMapHandle.has_value() ? "true" : "false"); // todo vulkan
                ImGui::Text("Metallic: %.3f, Roughness: %.3f", material->metallic, material->roughness);
            }
        }

        for (SceneTransform* childTransform: transform.getChildren()) {
            DrawSceneHierarchyNode(*childTransform);
        }

        ImGui::TreePop();
    }
}

void FateRenderer::render(const Scene& scene) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

#pragma region Old OpenGL version
    // int winWidth;
    // int winHeight;
    // SDL_GetWindowSizeInPixels(window, &winWidth, &winHeight); // todo is this correct equivalent?
    // const float winAspect = static_cast<float>(winWidth) / winHeight;
    //
    // constexpr float camHorFovDegs = 60.0f;
    // const float fovYRads = 2.0f * glm::atan(glm::tan(glm::radians(camHorFovDegs) * 0.5f) / winAspect);
    // const glm::mat4 proj = glm::perspective(fovYRads, winAspect, 0.01f, 100.0f);
    //
    // const glm::mat4 view = glm::inverse(glm::translate(glm::mat4(1.0f), glm::vec3(cameraPosition)) * glm::mat4_cast(glm::quat(glm::radians(cameraRotation))));
    //
    // std::vector<RenderItem> renderItems; // todo cache + reserve, maybe store total meshes
    //
    // const auto& objects = scene.getObjects();
    //
    // // todo we need to migrate to camera-relative positions at some point
    // const auto camPos = glm::vec3(cameraPosition);
    //
    // for (const auto object: objects) {
    //     const auto& meshes = object->getMeshes();
    //
    //     for (const auto& mesh: meshes) {
    //         // todo almost certainly doesn't need to be rebuilt from scratch every frame
    //         const auto command = DrawElementsIndirectCommand{
    //             .count = static_cast<GLuint>(mesh->getIndices().size()),
    //             .instanceCount = 1,
    //             .firstIndex = mesh->getGPUHandle()->getEboOffset(),
    //             .baseVertex = mesh->getGPUHandle()->getVboOffset(),
    //             .baseInstance = 0
    //         };
    //
    //         const auto modelMatrix = object->getTransform().getWorldMatrix();
    //         const auto meshPosition = glm::vec3(modelMatrix[3]);
    //
    //         const auto meshMaterial = mesh->getMaterial();
    //         const auto materialData = MaterialData{
    //             .baseColour = meshMaterial->baseColour,
    //             .albedoMapHandle = meshMaterial->albedoMapHandle.value_or(missingTextureHandle),
    //             .mapFlags = meshMaterial->mapFlags,
    //             .metallic = meshMaterial->metallic,
    //             .roughness = meshMaterial->roughness
    //         };
    //
    //         renderItems.push_back(RenderItem{
    //             .command = command,
    //             .modelMatrix = modelMatrix,
    //             .material = materialData,
    //             .distance = glm::length(meshPosition - camPos),
    //             .isTransparent = meshMaterial->useAlpha
    //         });
    //     }
    // }
    //
    // std::ranges::sort(renderItems, [](const RenderItem& a, const RenderItem& b) {
    //     if (a.isTransparent != b.isTransparent) {
    //         return !a.isTransparent; // all opaque first
    //     }
    //
    //     // opaque is closest-first, transparent is farthest-first
    //     return !a.isTransparent
    //                ? a.distance < b.distance
    //                : a.distance > b.distance;
    // });
    //
    // indirectBuffer.clear();
    // modelMatrices.clear();
    // materials.clear();
    // indirectBuffer.reserve(renderItems.size());
    // modelMatrices.reserve(renderItems.size());
    // materials.reserve(renderItems.size());
    //
    // std::uint32_t ssboIndex = 0;
    // std::uint32_t opaqueCount = 0;
    // std::uint32_t transparentCount = 0;
    //
    // for (auto& item: renderItems) {
    //     item.command.baseInstance = ssboIndex++;
    //
    //     modelMatrices.push_back(item.modelMatrix);
    //     materials.push_back(item.material);
    //
    //     indirectBuffer.push_back(item.command);
    //
    //     if (item.isTransparent) {
    //         transparentCount++;
    //     }
    //     else {
    //         opaqueCount++;
    //     }
    // }
    //
    // // todo we also definitely don't need to be resending everything every frame
    // glNamedBufferSubData(transformBufferSSBO, 0, modelMatrices.size() * sizeof(glm::mat4), modelMatrices.data());
    // glNamedBufferSubData(materialBufferSSBO, 0, materials.size() * sizeof(MaterialData), materials.data());
    //
    // glm::mat4 vp = proj * view;
    // glProgramUniformMatrix4fv(shaderProgram, 0, 1, GL_FALSE, glm::value_ptr(vp));
    //
    // glUseProgram(shaderProgram);
    // glBindVertexArray(vao);
    //
    // glBindBuffer(GL_DRAW_INDIRECT_BUFFER, dib);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transformBufferSSBO);
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, materialBufferSSBO);
    //
    // glEnable(GL_DEPTH_TEST);
    //
    // glNamedBufferSubData(dib, 0, indirectBuffer.size() * sizeof(DrawElementsIndirectCommand), indirectBuffer.data());
    //
    // // Opaque pass
    // if (opaqueCount > 0) {
    //     glDisable(GL_BLEND);
    //     glDepthMask(GL_TRUE);
    //
    //     glMultiDrawElementsIndirect(
    //         GL_TRIANGLES,
    //         GL_UNSIGNED_INT,
    //         nullptr,
    //         opaqueCount,
    //         0
    //     );
    // }
    //
    // // Transparent pass
    // if (transparentCount > 0) {
    //     glEnable(GL_BLEND);
    //     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //     glDepthMask(GL_FALSE);
    //
    //     glMultiDrawElementsIndirect(
    //         GL_TRIANGLES,
    //         GL_UNSIGNED_INT,
    //         reinterpret_cast<const void *>(opaqueCount * sizeof(DrawElementsIndirectCommand)),
    //         transparentCount,
    //         0
    //     );
    // }
#pragma endregion

    // Sync
    vkChk(vkWaitForFences(device, 1, &fences[frameIndex], true, UINT64_MAX));
    vkChk(vkResetFences(device, 1, &fences[frameIndex]));
    vkChkSwapchain(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex));

    // Update shader data
    shaderData.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y), 0.1f, 32.0f);
    shaderData.view = glm::translate(glm::mat4(1.0f), glm::vec3(cameraPosition));
    for (auto i = 0; i < 3; i++) {
        auto instancePos = glm::vec3(static_cast<float>(i - 1) * 3.0f, 0.0f, 0.0f);
        shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
    }
    memcpy(shaderDataBuffers[frameIndex].allocationInfo.pMappedData, &shaderData, sizeof(ShaderData));

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
        .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}
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

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &textureDescriptorSet, 0, nullptr);

    VkDeviceSize vOffset{0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &geometryBuffer, &vOffset);
    vkCmdBindIndexBuffer(commandBuffer, geometryBuffer, geometryBufferAllocationInfo.size, VK_INDEX_TYPE_UINT16);
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &shaderDataBuffers[frameIndex].deviceAddress);

    vkCmdDrawIndexed(commandBuffer, indexCount, 3, 0, 0, 0);
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

    frameIndex = (frameIndex + 1) % MaxFramesInFlight;
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderCompleteSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex
    };
    vkChkSwapchain(vkQueuePresentKHR(queue, &presentInfo));
}

void FateRenderer::drawEditorUI(const Scene& scene, const double deltaTime) {
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

        static std::deque<double> deltaTimeBuffer{};
        if (deltaTimeBuffer.size() >= 30) {
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
    ImGui::DragFloat3("Camera rotation", &cameraRotation.x, 0.01f);

    if (ImGui::CollapsingHeader("Hierarchy", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
        for (SceneObject* object: scene.getObjects()) {
            if (object->getTransform().getParent() != nullptr) continue;
            DrawSceneHierarchyNode(object->getTransform());
        }
    }

    // if (ImGui::CollapsingHeader("Resource usage", nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
    //     ImGui::ProgressBar(
    //         static_cast<float>(vboOffset) / DefaultBufferSize,
    //         ImVec2(-1.0f, 0.0f),
    //         std::format("VBO usage: {} ({:.5f}%)", FileUtils::prettyBytes(vboOffset), static_cast<float>(vboOffset) / DefaultBufferSize * 100.0f).c_str()
    //     );
    //
    //     ImGui::ProgressBar(
    //         static_cast<float>(eboOffset) / DefaultBufferSize,
    //         ImVec2(-1.0f, 0.0f),
    //         std::format("EBO usage: {} ({:.5f}%)", FileUtils::prettyBytes(eboOffset), static_cast<float>(eboOffset) / DefaultBufferSize * 100.0f).c_str()
    //     );
    //
    //     ImGui::ProgressBar(
    //         0.0f,
    //         ImVec2(-1.0f, 0.0f),
    //         std::format("Texture usage: {}", FileUtils::prettyBytes(textureUploadedBytes)).c_str()
    //     );
    // }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffers[frameIndex]);
}

void FateRenderer::endRender() const {
}

GPUMeshHandle FateRenderer::uploadMesh(const Mesh& mesh) {
    const auto vertexBytes = mesh.getVertices().size() * sizeof(Vertex);
    const auto indexBytes = mesh.getIndices().size() * sizeof(std::uint32_t);

    const auto alignedVertexSize = VmaAlignUp(vertexBytes, static_cast<VkDeviceSize>(16));
    const auto alignedIndexSize = VmaAlignUp(indexBytes, static_cast<VkDeviceSize>(16));

    const VmaVirtualAllocationCreateInfo virtualAllocCI{
        .size = alignedVertexSize + alignedIndexSize,
        .alignment = 16
    };
    VmaVirtualAllocation virtualAllocation{};
    VkDeviceSize bufferOffset;
    vkChk(vmaVirtualAllocate(geometryVirtualBlock, &virtualAllocCI, &virtualAllocation, &bufferOffset));

    std::memcpy(static_cast<char *>(geometryBufferAllocationInfo.pMappedData) + bufferOffset, mesh.getVertices().data(), vertexBytes);
    std::memcpy(static_cast<char *>(geometryBufferAllocationInfo.pMappedData) + bufferOffset + alignedVertexSize, mesh.getIndices().data(), indexBytes);

    return GPUMeshHandle(bufferOffset, bufferOffset + alignedVertexSize, virtualAllocation);
}

// GLuint64 FateRenderer::uploadTexture(const TextureData& data, const GLuint minFilter, const GLuint magFilter) {
//     GLuint texture;
//
//     std::println("Uploading texture of size {}x{} ({})", data.width, data.height, FileUtils::prettyBytes(data.width * data.height * 4));
//
//     glCreateTextures(GL_TEXTURE_2D, 1, &texture);
//     glTextureStorage2D(texture, 1, GL_RGBA8, data.width, data.height);
//     glTextureSubImage2D(texture, 0, 0, 0, data.width, data.height, GL_RGBA, GL_UNSIGNED_BYTE, data.pixels);
//     glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, minFilter);
//     glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, magFilter);
//     glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
//     glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
//
//     const GLuint64 textureHandle = glGetTextureHandleARB(texture);
//     glMakeTextureHandleResidentARB(textureHandle);
//
//     textureUploadedBytes += data.width * data.height * 4;
//
//     return textureHandle;
// }
