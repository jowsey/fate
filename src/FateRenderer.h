#pragma once

#include <vector>

#define VOLK_IMPLEMENTATION
#include "volk.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "glm/glm.hpp"
#include "SDL3/SDL_video.h"
#include "GPUMeshHandle.h"
#include "Mesh.h"
#include "Scene.h"

struct alignas(16) TransformBuffer {
    glm::mat4 modelMatrices[];
};

struct alignas(16) MaterialData {
    glm::vec4 baseColour;
    // GLuint64 albedoMapHandle;
    std::uint32_t mapFlags;
    float metallic;
    float roughness;
};

struct MaterialBuffer {
    MaterialData materials[];
};

struct Texture {
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
};

struct ShaderData {
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected{1};
};

struct ShaderDataBuffer {
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceAddress deviceAddress{};
};

class FateRenderer {
    static constexpr int GeometryBufferSize = 1024 * 1024 * 128; // 128MiB
    static constexpr std::uint32_t MaxFramesInFlight{2};
    static constexpr VkFormat FrameImageFormat{VK_FORMAT_B8G8R8A8_SRGB};

    // static GLuint loadShader(GLuint type, const std::filesystem::path& path);

    static void vkChk(VkResult result);

    void vkChkSwapchain(VkResult result);

    ShaderData shaderData{};
    std::array<ShaderDataBuffer, MaxFramesInFlight> shaderDataBuffers;

    SDL_Window* window;
    VkSurfaceKHR surface{VK_NULL_HANDLE};

    std::uint32_t imageIndex{0};
    std::uint32_t frameIndex{0};

    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    std::uint32_t queueFamilyIndex{0};
    VkQueue queue{VK_NULL_HANDLE};

    bool updateSwapchain{false};
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};

    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    VmaAllocator allocator{VK_NULL_HANDLE};

    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;

    VkImage depthImage;
    VmaAllocation depthImageAllocation;
    VkImageView depthImageView;
    std::array<VkCommandBuffer, MaxFramesInFlight> commandBuffers;
    std::array<VkFence, MaxFramesInFlight> fences;
    std::array<VkSemaphore, MaxFramesInFlight> imageAcquiredSemaphores;
    std::vector<VkSemaphore> renderCompleteSemaphores;

    VkBuffer geometryBuffer{VK_NULL_HANDLE};
    VmaAllocation geometryBufferAllocation{VK_NULL_HANDLE};
    VmaAllocationInfo geometryBufferAllocationInfo{};
    VmaVirtualBlock geometryVirtualBlock{VK_NULL_HANDLE};

    VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout textureDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet textureDescriptorSet{VK_NULL_HANDLE};

    glm::ivec2 windowSize{};

    // todo vulkan: segment back into objects
    std::array<Texture, 3> textures{};
    glm::vec3 objectRotations[3]{};

    glm::dvec3 cameraPosition{2.25f, 1.0f, -6.0f};
    glm::vec3 cameraRotation{0, 35.0f, 0};

public:
    FateRenderer();

    ~FateRenderer();

    void render(const Scene& scene);

    void drawEditorUI(const Scene& scene, double deltaTime);

    void endRender() const;

    [[nodiscard]] SDL_Window* getWindow() const { return window; }

    GPUMeshHandle uploadMesh(const Mesh& mesh);

    // GLuint64 uploadTexture(const TextureData& data, GLuint minFilter = GL_LINEAR, GLuint magFilter = GL_LINEAR);
};
