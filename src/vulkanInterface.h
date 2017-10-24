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
#include "window.h"

#define VALIDATION_LAYERS

struct Vertex
{
	glm::vec2 position;
	glm::vec3 colour;
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
	VkDevice logicalDevice;
	VulkanQueues queues;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSurfaceKHR surface;
	bool hasSwapchain = false;
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR surfaceFormat;
	VkExtent2D swapchainExtent;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkCommandPool commandPool;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

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
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createVertexBuffer();
	void createIndexBuffer();
	void createCommandBuffers();
	void createSemaphores();

	void cleanupSwapchain(bool delSwapchain);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	Window * window;
	std::vector<Vertex> vertices;
	std::vector<uint16_t > indices;


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

	void draw();
	void waitForIdle();
	void recreateSwapchain();
};

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
VulkanQueues findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
bool checkDeviceExtensionSupport(VkPhysicalDevice device);
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> availableFormats);
VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> availablePresentModes);
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);

VkShaderModule loadShaderModule(VkDevice device, const std::string &shaderFilename);

VkVertexInputBindingDescription getBindingDescription();
std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription();
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags);

#endif //VULKANITE_VULKANINTERFACE_H
