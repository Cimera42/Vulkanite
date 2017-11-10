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

#define VALIDATION_LAYERS

class Transform;
class Camera;

struct UniformBufferObject {
	glm::mat4 model;
};

struct PushConstantBufferObject {
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
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkCommandPool commandPool;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> swapchainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	void createInstance();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapchain();
	void createImageViews();
	void createRenderPass();
	void createDescriptorSetLayout();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createDepthResources();
	void createUniformBuffer();
	void createDescriptorPool();
	void createDescriptorSet();
	void createCommandBuffers();
	void createSemaphores();

	void cleanupSwapchain(bool delSwapchain);

	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	Window * window;
	Model * model;
	PushConstantBufferObject pushConstant;

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

	VkDevice logicalDevice;

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
					  VkMemoryPropertyFlags propertyFlags, VkBuffer &buffer, VkDeviceMemory &bufferMemory);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
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

VkShaderModule loadShaderModule(VkDevice device, const std::string &shaderFilename);

VkVertexInputBindingDescription getBindingDescription();
std::array<VkVertexInputAttributeDescription, 3> getAttributeDescription();
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags);

VkImageView createImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

#endif //VULKANITE_VULKANINTERFACE_H
