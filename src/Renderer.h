#pragma once

#include <vector>

#include "CubemapData.h"
#include "glm/glm.hpp"
#include "SDL3/SDL.h"
#include "SDL3/SDL_video.h"
#include "GPUMeshHandle.h"
#include "IndexAllocator.h"
#include "Mesh.h"
#include "Scene.h"
#include "TextureData.h"

namespace Fate {
    struct MaterialData {
        glm::vec4 baseColour;
        std::uint32_t albedoMapIndex;
        std::uint32_t normalMapIndex;
        std::uint32_t ambientMapIndex;
        std::uint32_t roughnessMapIndex;
        std::uint32_t metallicMapIndex;
        std::uint32_t emissiveMapIndex;
        std::uint32_t mapFlags;
        float roughness;
        float metallic;
    };

    struct LightData {
        glm::vec3 direction;
        float range;
        glm::vec3 colour;
        float intensity; // todo pack tighter?
    };

    struct FrameGlobals {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 cameraPosition;
        float _padding;

        LightData light;
    };

    struct SkyboxGlobals {
        glm::mat4 view;
        glm::mat4 projection;
        std::uint32_t cubemapIndex;
    };

    struct ObjectData {
        glm::mat4 model;
        glm::mat4 inverseTransposeModel;
        MaterialData material;
    };

    struct AllocatedBuffer {
        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VmaAllocationInfo allocationInfo{};
        VkDeviceAddress deviceAddress{};
    };

    struct AllocatedTexture {
        VkImage image{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        std::uint32_t descriptorIndex{0};
    };

    struct SamplerCollection {
        VkSampler linearRepeat{VK_NULL_HANDLE};
        VkSampler linearClamp{VK_NULL_HANDLE};
    };

    class Renderer {
        static constexpr std::uint32_t MaxObjects = 65536;
        static constexpr std::uint32_t MaxTextureDescriptors = 65536;
        static constexpr std::uint32_t VertexBufferSize = 1024 * 1024 * 256; // 256MiB
        static constexpr std::uint32_t IndexBufferSize = 1024 * 1024 * 128; // 128MiB
        static constexpr std::uint32_t MaxFramesInFlight{2};
        static constexpr VkFormat FrameImageFormat{VK_FORMAT_B8G8R8A8_UNORM};

        static void vkChk(VkResult result);

        void vkChkSwapchain(VkResult result);

        IndexAllocator textureDescriptorAllocator{MaxTextureDescriptors};

        SDL_Window* window;
        VkSurfaceKHR surface;

        VkFormat depthImageFormat{VK_FORMAT_UNDEFINED};

        std::uint32_t imageIndex{0};
        std::uint32_t frameIndex{0};

        VkInstance instance{VK_NULL_HANDLE};
        VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
        VkDevice device{VK_NULL_HANDLE};
        std::uint32_t queueFamilyIndex{0};
        VkQueue queue{VK_NULL_HANDLE};

        VmaAllocator allocator{VK_NULL_HANDLE};

        VkSwapchainKHR swapchain{VK_NULL_HANDLE};

        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkImage depthImage;
        VmaAllocation depthImageAllocation;
        VkImageView depthImageView;

        SamplerCollection samplers{};

        VkPipeline geometryPipeline{VK_NULL_HANDLE};
        VkPipelineLayout geometryPipelineLayout{VK_NULL_HANDLE};
        VkShaderModule litShaderModule{VK_NULL_HANDLE};

        VkPipeline skyboxPipeline{VK_NULL_HANDLE};
        VkPipelineLayout skyboxPipelineLayout{VK_NULL_HANDLE};
        VkShaderModule skyboxShaderModule{VK_NULL_HANDLE};

        VkCommandPool commandPool{VK_NULL_HANDLE};
        std::array<VkCommandBuffer, MaxFramesInFlight> commandBuffers;
        std::array<VkFence, MaxFramesInFlight> fences;
        std::array<VkSemaphore, MaxFramesInFlight> imageAcquiredSemaphores;
        std::vector<VkSemaphore> renderCompleteSemaphores;

        VkBuffer vertexBuffer{VK_NULL_HANDLE};
        VmaAllocation vertexBufferAllocation{VK_NULL_HANDLE};
        VmaAllocationInfo vertexBufferAllocationInfo{};
        VmaVirtualBlock vertexVirtualBlock{VK_NULL_HANDLE};

        VkBuffer indexBuffer{VK_NULL_HANDLE};
        VmaAllocation indexBufferAllocation{VK_NULL_HANDLE};
        VmaAllocationInfo indexBufferAllocationInfo{};
        VmaVirtualBlock indexVirtualBlock{VK_NULL_HANDLE};

        VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
        VkDescriptorSetLayout textureDescriptorSetLayout{VK_NULL_HANDLE};
        VkDescriptorSet textureDescriptorSet{VK_NULL_HANDLE};

        SkyboxGlobals skyboxGlobals{};
        FrameGlobals frameGlobals{};
        std::vector<ObjectData> objectDatas{};
        std::array<AllocatedBuffer, MaxFramesInFlight> skyboxGlobalsBuffers{};
        std::array<AllocatedBuffer, MaxFramesInFlight> frameGlobalsBuffers{};
        std::array<AllocatedBuffer, MaxFramesInFlight> objectDataBuffers{};
        std::array<AllocatedBuffer, MaxFramesInFlight> indirectBuffers{};

        std::vector<std::unique_ptr<AllocatedTexture>> allocatedTextures{};

        glm::ivec2 windowSize{};

        glm::dvec3 cameraPosition{2.25f, 0.0f, 5.0f};
        glm::vec3 cameraRotation{0, 35.0f, 0};
        float cameraHorFovDegs{70.0f};

        glm::vec3 lightDir{-0.1f, -0.1f, -1.0f};
        glm::vec4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
        float lightIntensity{8.0f};

    public:
        Renderer();

        ~Renderer();

        void buildEditorUI(const Scene& scene, double deltaTime);

        void render(const Scene& scene);

        [[nodiscard]] SDL_Window* getWindow() const { return window; }

        GPUMeshHandle uploadMesh(const Mesh& mesh);

        AllocatedTexture* uploadTexture(const TextureData& texture);

        AllocatedTexture* uploadCubemap(const CubemapData& cubemap);

        // todo this should be way more explicit
        bool updateSwapchain{false};
    };
}
