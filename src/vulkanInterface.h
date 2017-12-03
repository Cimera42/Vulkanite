//
// Created by Tim on 21/10/2017.
//

#ifndef VULKANITE_VULKANINTERFACE_H
#define VULKANITE_VULKANINTERFACE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include "window.h"
#include "Texture.h"
#include "Model.h"
#include "SpecificThreadPool.h"
#include "ParticleSystem.h"
#include "ImageAttachment.h"

#define VALIDATION_LAYERS

#define S1(x) #x
#define S2(x) S1(x)
#define VK_RESULT_CHECK(res) \
{ \
	if((res) != VK_SUCCESS) \
	{ \
		Logger() << "Vulkan error in " << __FILE__ << " on line " << __LINE__; \
		throw std::runtime_error("Vulkan error in " __FILE__ " on line " S2(__LINE__)); \
	} \
}

class Transform;
class Camera;
class ParticleSystem;
class Terrain;
class Skybox;

struct UniformBufferObject {
	glm::mat4 model;
};

struct PushConstantBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct ParticlePushConstantBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
};

struct VulkanQueues
{
	int graphicsFamily = -1;
	int presentFamily = -1;

	bool isComplete()
	{
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct ThreadData
{
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<PushConstantBufferObject> pushConstantBlock;
	std::vector<glm::vec3> modelPositions;
};

class VulkanInterface
{
	VkInstance vulkanInstance;
	VkPhysicalDevice physicalDevice;
	VulkanQueues queues;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	bool hasSwapchain = false;
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR surfaceFormat;
	VkExtent2D swapchainExtent;
	VkRenderPass renderPass;
	struct {
		VkDescriptorSetLayout standard;
		VkDescriptorSetLayout particle;
		VkDescriptorSetLayout screen;
	} descriptorSetLayouts;
	struct {
		VkPipelineLayout standard;
		VkPipelineLayout particle;
		VkPipelineLayout screen;
	} pipelineLayouts;
	struct {
		VkPipeline standard;
		VkPipeline particle;
		VkPipeline screen;
	} pipelines;
	struct {
		VkDescriptorSet standard;
		VkDescriptorSet particle;
		VkDescriptorSet screen;
	} descriptorSets;
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	ImageAttachment depthImage;

	VkSampler offscreenSampler;
	ImageAttachment offscreenColorImage;
	ImageAttachment offscreenDepthImage;
	VkSemaphore offscreenRenderedSemaphore;
	VkFramebuffer offscreenFramebuffer;
	VkDescriptorSet offscreenDescriptorSet;
	VkPipeline offscreenPipeline;
	PushConstantBufferObject offscreenPushConstant;

	VkCommandBuffer primaryCommandBuffer;
	VkCommandBuffer particleCommandBuffer;
	VkCommandBuffer screenCommandBuffer;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> swapchainFramebuffers;

	std::vector<VkShaderModule> shaderModules;

	void createInstance();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapchain();
	void createImageViews();
	void createRenderPass();
	void createStandardDescriptorSetLayout();
	void createParticleDescriptorSetLayout();
	void createScreenDescriptorSetLayout();
	void createPipelineCache();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createDepthResources();
	void createUniformBuffer();
	void createDescriptorPool();
	void createDescriptorSets();
	void createCommandBuffers();
	void createSemaphoresAndFences();

	void createOffscreenRenderPass();
	void createOffscreenImages();
	void createOffscreenSemaphore();
	void createOffscreenFramebuffer();
	void createScreenCommandBuffer();
	void updateScreenCommandBuffer(VkFramebuffer framebuffer);
	void createScreenDescriptorSet();

	void threadedRender(int threadIndex, int objectIndex, VkCommandBufferInheritanceInfo inheritanceInfo);
	void updateParticleCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);
	void updateCommandBuffers();

	void cleanupSwapchain(bool delSwapchain);

	Model * model;
	Model * screenQuad;
	Terrain * terrain;
	Skybox * skybox;
	uint32_t numThread = 2;
	uint32_t numPerThread = 3;
	SpecificThreadPool threadPool;
	std::vector<ThreadData> threadData;
	ParticleSystem* particles;

#ifdef VALIDATION_LAYERS
	bool enableValidationLayers = true;
#else
	bool enableValidationLayers = false;
#endif
	const std::vector<const char*> validationLayers = {"VK_LAYER_LUNARG_standard_validation"};
	bool checkValidationLayers();

	std::vector<const char*> getRequiredExtensions();

	VkDebugReportCallbackEXT debugCallback;
	void initDebug();
	void destroyDebug();

public:
	void initVulkan(Window * window);
	~VulkanInterface();

	void update(Camera *camera);
	void draw();
	void waitForIdle();
	void recreateSwapchain();

	Window * window;
	VkDevice logicalDevice;

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
					  VkMemoryPropertyFlags propertyFlags, VkBuffer &buffer, VkDeviceMemory &bufferMemory);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void createImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkImageCreateFlags flags);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layers);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

	VkPipelineShaderStageCreateInfo loadShaderModule(const std::string &shaderFilename, VkShaderStageFlagBits stage);

	VkPipelineCache pipelineCache;
	VkRenderPass offscreenRenderPass;
	VkDescriptorPool descriptorPool;
	VkCommandPool commandPool;
	ParticlePushConstantBufferObject pushConstant;

	VkCommandBuffer beginSingleTimeCommands();

	void endSingleTimeCommands(VkCommandBuffer commandBuffer);
};

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
VulkanQueues findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
bool checkDeviceExtensionSupport(VkPhysicalDevice device);
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats);
VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> availablePresentModes);
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);
VkFormat findSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
VkFormat findDepthFormat(VkPhysicalDevice device);
bool hasStencilComponent(VkFormat format);

VkVertexInputBindingDescription bindingDescription(uint32_t binding, uint32_t stride,
                                                   VkVertexInputRate inputType);
VkVertexInputAttributeDescription attributeDescription(uint32_t binding, uint32_t location,
                                                       VkFormat format, uint32_t offset);
VkVertexInputAttributeDescription attributeDescription(uint32_t binding, uint32_t location,
                                                       VkFormat format, size_t offset);

std::vector<VkVertexInputBindingDescription> modelBindingDescription();
std::vector<VkVertexInputBindingDescription> particleBindingDescription();
std::vector<VkVertexInputBindingDescription> screenBindingDescription();
std::vector<VkVertexInputAttributeDescription> modelAttributeDescription();
std::vector<VkVertexInputAttributeDescription> screenAttributeDescription();
std::vector<VkVertexInputAttributeDescription> particleAttributeDescription();
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags);

VkImageView createImageView(VkDevice logicalDevice, VkImageViewType viewType, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t layers);

Mesh* createScreenQuad(VulkanInterface* vki);

#endif //VULKANITE_VULKANINTERFACE_H
