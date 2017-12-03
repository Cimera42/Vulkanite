//
// Created by Tim on 21/10/2017.
//

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <set>
#include <fstream>
#include <array>
#include "vulkanInterface.h"
#include "logger.h"
#include "Camera.h"
#include "Terrain.h"
#include "Skybox.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>

#define LOAD_IFUNCTION(instance, funcName) auto (funcName) = (PFN_ ## funcName) vkGetInstanceProcAddr(instance, #funcName)

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanInterfaceDebugCallback(
		VkDebugReportFlagsEXT       flags,
		VkDebugReportObjectTypeEXT  objectType,
		uint64_t                    object,
		size_t                      location,
		int32_t                     messageCode,
		const char*                 pLayerPrefix,
		const char*                 pMessage,
		void*                       pUserData)
{
	if((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) == VK_DEBUG_REPORT_WARNING_BIT_EXT)
		return VK_TRUE;

	Logger(false, true) << pLayerPrefix << ": VULKAN ";

	if((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) == VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		Logger(false, false) << "INFORMATION ";
	if((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) == VK_DEBUG_REPORT_WARNING_BIT_EXT)
		Logger(false, false) << "WARNING ";
	if((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) == VK_DEBUG_REPORT_ERROR_BIT_EXT)
		Logger(false, false) << "ERROR ";

	Logger(true, false) << "\"" << pMessage << "\" " << objectType << " : " << object;
	return VK_FALSE;
}

VulkanInterface::~VulkanInterface()
{
	cleanupSwapchain(true);

	offscreenColorImage.destroy(logicalDevice);
	offscreenDepthImage.destroy(logicalDevice);

	vkDestroySemaphore(logicalDevice, offscreenRenderedSemaphore, nullptr);
	vkDestroySampler(logicalDevice, offscreenSampler, nullptr);
	vkDestroyRenderPass(logicalDevice, offscreenRenderPass, nullptr);
	vkDestroyFramebuffer(logicalDevice, offscreenFramebuffer, nullptr);
	vkDestroyPipeline(logicalDevice, offscreenPipeline, nullptr);

	vkDestroyPipeline(logicalDevice, pipelines.screen, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayouts.screen, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayouts.screen, nullptr);

	threadPool.destroy();
	for(auto &i : threadData)
	{
		vkFreeCommandBuffers(logicalDevice, i.commandPool, numPerThread, i.commandBuffers.data());
		vkDestroyCommandPool(logicalDevice, i.commandPool, nullptr);
	}

	depthImage.destroy(logicalDevice);
	Logger() << "Depth image destroyed";

//	vkFreeDescriptorSets(logicalDevice, descriptorSet, 0, nullptr);
//	Logger() << "Descriptor pool destroyed";
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	Logger() << "Descriptor pool destroyed";

	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayouts.standard, nullptr);
	Logger() << "Descriptor set layout destroyed";
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayouts.particle, nullptr);
	Logger() << "Particle descriptor set layout destroyed";
	vkDestroyBuffer(logicalDevice, uniformBuffer, nullptr);
	Logger() << "Uniform buffer destroyed";
	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	Logger() << "Uniform buffer memory freed";

	delete particles;
	delete model;
	delete screenQuad;
	delete terrain;
	delete skybox;
	for(auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
	}
	Logger() << "Shader modules destroyed";

	vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
	Logger() << "Render semaphore destroyed";
	vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
	Logger() << "Image semaphore destroyed";

	vkDestroyPipelineCache(logicalDevice, pipelineCache, nullptr);
	Logger() << "Pipeline cache destroyed";

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
	Logger() << "Command pool destroyed";
	vkDestroyDevice(logicalDevice, nullptr);
	Logger() << "Logical device destroyed";
	vkDestroySurfaceKHR(vulkanInstance, surface, nullptr);
	Logger() << "Surface destroyed";
	destroyDebug();
	vkDestroyInstance(vulkanInstance, nullptr);
	Logger() << "Vulkan instance destroyed";
}

void VulkanInterface::cleanupSwapchain(bool delSwapchain)
{
	int i = 0;
	for(const auto& framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
		Logger() << "Framebuffer [" << i << "] destroyed";
		i++;
	}

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &primaryCommandBuffer);
	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &particleCommandBuffer);

	vkDestroyPipeline(logicalDevice, pipelines.standard, nullptr);
	Logger() << "Standard pipeline destroyed";
	vkDestroyPipeline(logicalDevice, pipelines.particle, nullptr);
	Logger() << "Particle pipeline destroyed";
	vkDestroyPipelineLayout(logicalDevice, pipelineLayouts.standard, nullptr);
	Logger() << "Pipeline layout destroyed";
	vkDestroyPipelineLayout(logicalDevice, pipelineLayouts.particle, nullptr);
	Logger() << "Particle pipeline layout destroyed";
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	Logger() << "Render pass destroyed";
	i = 0;
	for(auto& imageView : swapchainImageViews)
	{
		vkDestroyImageView(logicalDevice, imageView, nullptr);
		Logger() << "Image view [" << i << "] destroyed";
		i++;
	}
	if(delSwapchain)
	{
		vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
		Logger() << "Swapchain destroyed";
	}
}

void VulkanInterface::initVulkan(Window * inWindow)
{
	window = inWindow;

	createInstance();
	initDebug();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createImageViews();
	createOffscreenImages();
	createRenderPass();
	createOffscreenRenderPass();
	createOffscreenFramebuffer();
	createStandardDescriptorSetLayout();
	createParticleDescriptorSetLayout();
	createScreenDescriptorSetLayout();
	createPipelineCache();
	createGraphicsPipeline();
	createCommandPool();
	createDepthResources();
	createFramebuffers();
	particles = new ParticleSystem(this, "models/Particles/particle1.fbx");
	model = new Model(this, "models/Mushroom/mushroom.fbx");
	Mesh * quadMesh = createScreenQuad(this);
	screenQuad = new Model(this, quadMesh);
	createUniformBuffer();
	createDescriptorPool();
	terrain = new Terrain(this, "images/island.png");
	skybox = new Skybox(this);
	createDescriptorSets();
	createScreenDescriptorSet();
	createCommandBuffers();
	createScreenCommandBuffer();
	createSemaphoresAndFences();
	createOffscreenSemaphore();
}

Mesh *createScreenQuad(VulkanInterface* vki)
{
	auto mesh = new Mesh(vki);

	mesh->load(
			{
				{-1,-1,0},
				{1,-1,0},
				{1,1,0},
				{-1,1,0}
			},
			{
				{0,0},
				{1,0},
				{1,1},
				{0,1}
			},
			{
				{0,0,1},
				{0,0,1},
				{0,0,1},
				{0,0,1}
			},
			{
				0,1,2,
				2,3,0
			}
	);

	return mesh;
}

void VulkanInterface::recreateSwapchain()
{
	vkDeviceWaitIdle(logicalDevice);

	cleanupSwapchain(false);

	createSwapchain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandBuffers();
}

void VulkanInterface::createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkanite";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto requiredExtensions = getRequiredExtensions();
	createInfo.enabledExtensionCount =  static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	if(enableValidationLayers)
	{
		if(checkValidationLayers())
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
			throw std::runtime_error("Validation layers missing");
	}
	else
		createInfo.enabledLayerCount = 0;

	VK_RESULT_CHECK(vkCreateInstance(&createInfo, nullptr, &vulkanInstance))
	Logger() << "Vulkan instance create success";

	//Retrieve extension
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	Logger() << "Available Instance Extensions:";
	for(const auto& extension : extensions)
	{
		Logger() << "\t" << extension.extensionName;
	}
}

void VulkanInterface::createSurface()
{
	VK_RESULT_CHECK(glfwCreateWindowSurface(vulkanInstance, window->glfwWindow, nullptr, &surface))
	Logger() << "GLFW Window Surface Created";
}

void VulkanInterface::pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, nullptr);
	if(deviceCount < 1)
		throw std::runtime_error("No GPU with Vulkan support found");
	else
		Logger() << "Physical Device count: " << deviceCount;

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, physicalDevices.data());

	Logger() << "Physical devices: ";
	for(const auto& device : physicalDevices)
	{
		Logger() << "---------------------";
		if(isDeviceSuitable(device, surface))
		{
			physicalDevice = device;
			break;
		}
	}
}

void VulkanInterface::createLogicalDevice()
{
	queues = findQueueFamilies(physicalDevice, surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = {queues.graphicsFamily, queues.presentFamily};

	float queuePriority = 1.0f;
	for(auto& queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(queues.graphicsFamily);
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if(enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
		createInfo.enabledLayerCount = 0;

	VK_RESULT_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice))
	Logger() << "Vulkan Logical Device created";

	vkGetDeviceQueue(logicalDevice, static_cast<uint32_t>(queues.graphicsFamily), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, static_cast<uint32_t>(queues.presentFamily), 0, &presentQueue);
}

void VulkanInterface::createSwapchain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

	surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	swapchainExtent = chooseSwapExtent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if(swapChainSupport.capabilities.maxImageCount > 0
	    && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = swapchainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = {static_cast<uint32_t>(queues.graphicsFamily),
	                                 static_cast<uint32_t>(queues.presentFamily)};

	if (queues.graphicsFamily != queues.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		//if present and graphics are the same, no need to share
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE; //Ignore pixels hidden by other drawings
	VkSwapchainKHR oldSwapchain = swapchain;
	if(hasSwapchain)
		createInfo.oldSwapchain = oldSwapchain;
	else
		createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_RESULT_CHECK(vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &swapchain))
	Logger() << "Swapchain created";

	if(hasSwapchain)
	{
		vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);
		Logger() << "Swapchain destroyed";
	}
	hasSwapchain = true;

	vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice, swapchain, &imageCount, swapchainImages.data());
}

void VulkanInterface::createOffscreenImages()
{
	createImage(swapchainExtent.width, swapchainExtent.height, 1, VK_FORMAT_R8G8B8A8_UNORM,
	            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenColorImage.image, offscreenColorImage.imageMemory, VK_NULL_HANDLE);
	offscreenColorImage.imageView = createImageView(logicalDevice, VK_IMAGE_VIEW_TYPE_2D, offscreenColorImage.image,
	                                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	VkFormat depthFormat = findDepthFormat(physicalDevice);
	createImage(swapchainExtent.width, swapchainExtent.height, 1,
	            depthFormat, VK_IMAGE_TILING_OPTIMAL,
	            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenDepthImage.image, offscreenDepthImage.imageMemory, VK_NULL_HANDLE);
	offscreenDepthImage.imageView = createImageView(logicalDevice, VK_IMAGE_VIEW_TYPE_2D, offscreenDepthImage.image,
	                                          depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	VK_RESULT_CHECK(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &offscreenSampler))
}

void VulkanInterface::createOffscreenSemaphore()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_RESULT_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &offscreenRenderedSemaphore))
}

void VulkanInterface::createOffscreenRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachmentDescriptions = {};
	// Color attachment
	attachmentDescriptions[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// Depth attachment
	VkFormat depthFormat = findDepthFormat(physicalDevice);
	attachmentDescriptions[1].format = depthFormat;
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.pAttachments = attachmentDescriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_RESULT_CHECK(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &offscreenRenderPass));
}

void VulkanInterface::createOffscreenFramebuffer()
{
	VkImageView attachments[2];
	attachments[0] = offscreenColorImage.imageView;
	attachments[1] = offscreenDepthImage.imageView;

	VkFramebufferCreateInfo fbufCreateInfo = {};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.renderPass = offscreenRenderPass;
	fbufCreateInfo.attachmentCount = 2;
	fbufCreateInfo.pAttachments = attachments;
	fbufCreateInfo.width = swapchainExtent.width;
	fbufCreateInfo.height = swapchainExtent.height;
	fbufCreateInfo.layers = 1;

	VK_RESULT_CHECK(vkCreateFramebuffer(logicalDevice, &fbufCreateInfo, nullptr, &offscreenFramebuffer));
}

void VulkanInterface::createScreenCommandBuffer()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &screenCommandBuffer));
}

void VulkanInterface::updateScreenCommandBuffer(VkFramebuffer framebuffer)
{
	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = framebuffer;
	renderPassBeginInfo.renderArea.extent.width = swapchainExtent.width;
	renderPassBeginInfo.renderArea.extent.height = swapchainExtent.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(screenCommandBuffer, &commandBufferBeginInfo);
	vkCmdBeginRenderPass(screenCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(screenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.screen);
	vkCmdBindDescriptorSets(screenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.screen, 0, 1,
	                        &descriptorSets.screen, 0, nullptr);

	screenQuad->draw(screenCommandBuffer);

	vkCmdEndRenderPass(screenCommandBuffer);
	VK_RESULT_CHECK(vkEndCommandBuffer(screenCommandBuffer));
}

void VulkanInterface::createScreenDescriptorSet()
{
	std::vector<VkDescriptorSetLayout> layouts = {
			descriptorSetLayouts.screen
	};
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	VK_RESULT_CHECK(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSets.screen))

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = offscreenColorImage.imageView;
	imageInfo.sampler = offscreenSampler;

	std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSets.screen;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanInterface::createImageViews()
{
	swapchainImageViews.resize(swapchainImages.size());

	int i = 0;
	for(auto& image : swapchainImages)
	{
		swapchainImageViews[i] = createImageView(logicalDevice, VK_IMAGE_VIEW_TYPE_2D, image, surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		Logger() << "Swapchain image view [" << i << "] created";
		i++;
	}
}

void VulkanInterface::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = surfaceFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //Clear to black each frame
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat(physicalDevice);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	//Frag shader output locations
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VK_RESULT_CHECK(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass))
	Logger() << "Render pass created";
}

void VulkanInterface::createStandardDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr; // Optional

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayouts.standard))
	Logger() << "Descriptor set layout created";
}

void VulkanInterface::createParticleDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr; // Optional

	std::array<VkDescriptorSetLayoutBinding, 1> bindings = {samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayouts.particle))
	Logger() << "Particle descriptor set layout created";
}

void VulkanInterface::createScreenDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr; // Optional

	std::array<VkDescriptorSetLayoutBinding, 1> bindings = {samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayouts.screen))
	Logger() << "Screen descriptor set layout created";
}

void VulkanInterface::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_RESULT_CHECK(vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanInterface::createGraphicsPipeline()
{
	auto bindingDescription = modelBindingDescription();
	auto attributeDescription = modelAttributeDescription();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescription.size());
	vertexInputInfo.pVertexBindingDescriptions = bindingDescription.data(); // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data(); // Optional

	auto particleBinding = particleBindingDescription();
	auto particleAttribute = particleAttributeDescription();

	VkPipelineVertexInputStateCreateInfo particleVertexInputInfo = {};
	particleVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	particleVertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(particleBinding.size());
	particleVertexInputInfo.pVertexBindingDescriptions = particleBinding.data(); // Optional
	particleVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(particleAttribute.size());
	particleVertexInputInfo.pVertexAttributeDescriptions = particleAttribute.data(); // Optional

	auto screenBinding = screenBindingDescription();
	auto screenAttribute = screenAttributeDescription();

	VkPipelineVertexInputStateCreateInfo screenVertexInputInfo = {};
	screenVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	screenVertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(screenBinding.size());
	screenVertexInputInfo.pVertexBindingDescriptions = screenBinding.data(); // Optional
	screenVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(screenAttribute.size());
	screenVertexInputInfo.pVertexAttributeDescriptions = screenAttribute.data(); // Optional

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchainExtent.width;
	viewport.height = (float) swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE; //Enable for shadowmaps to clamp to max depth, requires gpu feature enabled
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //Anything not fill requires gpu feature enable
	rasterizer.lineWidth = 1.0f; //>1 requires wideLines feature enable
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	/*colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional*/
	//Alpha blending
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	//Some state can be made dynamic
	/*VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;*/

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size = sizeof(PushConstantBufferObject);
	pushConstantRange.offset = 0;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//Used for shader uniforms
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts.standard; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 1; // Optional
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange; // Optional

	VK_RESULT_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts.standard))
	Logger() << "Pipeline layout created";

	pushConstantRange.size = sizeof(ParticlePushConstantBufferObject);
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts.particle; // Optional
	VK_RESULT_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts.particle))
	Logger() << "Particle pipeline layout created";

	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = VK_NULL_HANDLE; // Optional
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts.screen; // Optional
	VK_RESULT_CHECK(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayouts.screen))
	Logger() << "Screen pipeline layout created";

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
			loadShaderModule("shaders/standard.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShaderModule("shaders/standard.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = pipelineLayouts.standard;
	pipelineInfo.renderPass = offscreenRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_RESULT_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.standard))
	Logger() << "Standard pipeline created";

	pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	pipelineInfo.basePipelineHandle = pipelines.standard;
	pipelineInfo.basePipelineIndex = -1;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineInfo, nullptr, &offscreenPipeline))

	shaderStages = {
			loadShaderModule("shaders/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShaderModule("shaders/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	pipelineInfo.pVertexInputState = &particleVertexInputInfo;
	pipelineInfo.layout = pipelineLayouts.particle;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.particle))
	Logger() << "Particle pipeline created";

	shaderStages = {
			loadShaderModule("shaders/screen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShaderModule("shaders/screen.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	pipelineInfo.pVertexInputState = &screenVertexInputInfo;
	pipelineInfo.layout = pipelineLayouts.screen;
	pipelineInfo.renderPass = renderPass;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineInfo, nullptr, &pipelines.screen))
	Logger() << "Particle pipeline created";
}

void VulkanInterface::createFramebuffers()
{
	swapchainFramebuffers.resize(swapchainImageViews.size());

	int i = 0;
	for(const auto& imageView : swapchainImageViews)
	{
		std::array<VkImageView,2> attachments = {
				imageView,
				depthImage.imageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchainExtent.width;
		framebufferInfo.height = swapchainExtent.height;
		framebufferInfo.layers = 1;

		VkResult res = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapchainFramebuffers[i]);
		if(res != VK_SUCCESS)
		{
			Logger() << "Framebuffer [" << i << "] creation failed";
			std::string errorString = "Failed to create framebuffer [";
			errorString += std::to_string(i);
			errorString += "]";
			VK_RESULT_CHECK(res)
		}
		Logger() << "Framebuffer [" << i << "] created";

		i++;
	}
}

void VulkanInterface::createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = static_cast<uint32_t>(queues.graphicsFamily);
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

	VK_RESULT_CHECK(vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool))
	Logger() << "Command pool created";
}

void VulkanInterface::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat(physicalDevice);
	createImage(swapchainExtent.width, swapchainExtent.height, 1,
	            depthFormat, VK_IMAGE_TILING_OPTIMAL,
	            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage.image, depthImage.imageMemory, VK_NULL_HANDLE);
	depthImage.imageView = createImageView(logicalDevice, VK_IMAGE_VIEW_TYPE_2D, depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	transitionImageLayout(depthImage.image, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
	                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

void VulkanInterface::createUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	             uniformBuffer, uniformBufferMemory);
}

void VulkanInterface::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 10;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 10;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 20;

	VK_RESULT_CHECK(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool))
	Logger() << "Descriptor pool created";
}

void VulkanInterface::createDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	std::vector<VkDescriptorSetLayout> layouts = {
			descriptorSetLayouts.standard,
			descriptorSetLayouts.particle,
			descriptorSetLayouts.screen
	};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	VK_RESULT_CHECK(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSets.standard))
	Logger() << "Descriptor sets allocated";

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(UniformBufferObject);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = model->texture->texture.imageView;
	imageInfo.sampler = model->texture->textureSampler;

	VkDescriptorImageInfo particleImageInfo = {};
	particleImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	particleImageInfo.imageView = particles->particleModel->texture->texture.imageView;
	particleImageInfo.sampler = particles->particleModel->texture->textureSampler;

	std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};
	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSets.standard;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = descriptorSets.standard;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = 1;
	descriptorWrites[1].pImageInfo = &imageInfo;

	descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[2].dstSet = descriptorSets.particle;
	descriptorWrites[2].dstBinding = 0;
	descriptorWrites[2].dstArrayElement = 0;
	descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[2].descriptorCount = 1;
	descriptorWrites[2].pImageInfo = &particleImageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanInterface::createCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &primaryCommandBuffer))
	Logger() << "Primary command buffer allocated";

	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	VK_RESULT_CHECK(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &particleCommandBuffer))
	Logger() << "Particle command buffer allocated";

//	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
//	VK_RESULT_CHECK(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &secondaryCommandBuffer))
//	Logger() << "Secondary command buffer allocated";

	threadPool.resize(numThread);
	threadData.resize(numThread);

	for(int i = 0; i < numThread; i++)
	{
		ThreadData* thread = &threadData[i];

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = static_cast<uint32_t>(queues.graphicsFamily);
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional

		VK_RESULT_CHECK(vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &thread->commandPool))
		Logger() << "Command pool created";

		thread->commandBuffers.resize(numPerThread);

		VkCommandBufferAllocateInfo subAllocInfo = {};
		subAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		subAllocInfo.commandPool = thread->commandPool;
		subAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		subAllocInfo.commandBufferCount = numPerThread;

		VK_RESULT_CHECK(vkAllocateCommandBuffers(logicalDevice, &subAllocInfo, thread->commandBuffers.data()))
		Logger() << "Sub command buffers allocated";

		thread->modelPositions.resize(numPerThread);
		thread->pushConstantBlock.resize(numPerThread);
		for(int j = 0; j < numPerThread; j++)
		{
			thread->modelPositions[j] = glm::vec3(i*2,j*2,i*j);
		}
	}
}

void VulkanInterface::threadedRender(int threadIndex, int objectIndex, VkCommandBufferInheritanceInfo inheritanceInfo)
{
	ThreadData* thread = &threadData[threadIndex];
	glm::vec3 modelPosition = thread->modelPositions[objectIndex];

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	VkCommandBuffer commandBuffer = thread->commandBuffers[objectIndex];

	VK_RESULT_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo))

	VkViewport viewport = {};
	viewport.width = window->width;
	viewport.height = window->height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.extent = swapchainExtent;
	scissor.offset = {0,0};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.standard);

	thread->pushConstantBlock[objectIndex].view = pushConstant.view;
	thread->pushConstantBlock[objectIndex].proj = pushConstant.proj;
	thread->pushConstantBlock[objectIndex].model = glm::translate(glm::mat4(1.0f), modelPosition);

	vkCmdPushConstants(
			commandBuffer,
			pipelineLayouts.standard,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(thread->pushConstantBlock[objectIndex]),
			&thread->pushConstantBlock[objectIndex]
	);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.standard,
	                        0, 1, &descriptorSets.standard, 0, nullptr);

	model->draw(commandBuffer);

	VK_RESULT_CHECK(vkEndCommandBuffer(commandBuffer));
}

void VulkanInterface::createSemaphoresAndFences()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_RESULT_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore))
	Logger() << "Image semaphore created";

	VK_RESULT_CHECK(vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore))
	Logger() << "Render semaphores created";
}

void VulkanInterface::update(Camera *camera)
{
	UniformBufferObject ubo = {};
	ubo.model = glm::mat4(1.0f);

	void* data;
	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(logicalDevice, uniformBufferMemory);

	pushConstant.view = camera->viewMatrix;
	pushConstant.proj = camera->projectionMatrix;
	//ubo.proj[1][1] *= -1; //Flip Y coordinate as its designed for OGL

//	beginSingleTimeCommands();
//	VkCommandBuffer pushCmdBuffer = beginSingleTimeCommands();
//	vkCmdPushConstants(pushCmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
//	                   0, sizeof(PushConstantBufferObject), &pushConstant);
//	endSingleTimeCommands(pushCmdBuffer);
}

void VulkanInterface::updateParticleCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	VK_RESULT_CHECK(vkBeginCommandBuffer(particleCommandBuffer, &commandBufferBeginInfo));

	vkCmdBindPipeline(particleCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.particle);
	vkCmdBindDescriptorSets(particleCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.particle,
	                        0, 1, &descriptorSets.particle, 0, nullptr);

	vkCmdPushConstants(
			particleCommandBuffer,
			pipelineLayouts.particle,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(pushConstant),
			&pushConstant
	);

	particles->draw(particleCommandBuffer);

	VK_RESULT_CHECK(vkEndCommandBuffer(particleCommandBuffer));
}

void VulkanInterface::updateCommandBuffers()
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	std::array<VkClearValue,2> clearValues = {};
	clearValues[0].color = {0.63f, 0.9f, 0.61f, 1.0f};
	clearValues[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = offscreenRenderPass;
	renderPassInfo.framebuffer = offscreenFramebuffer;
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapchainExtent;
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	VK_RESULT_CHECK(vkBeginCommandBuffer(primaryCommandBuffer, &beginInfo))

	vkCmdBeginRenderPass(primaryCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	VkCommandBufferInheritanceInfo inheritanceInfo = {};
	inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceInfo.renderPass = offscreenRenderPass;
	inheritanceInfo.framebuffer = offscreenFramebuffer;

	std::vector<VkCommandBuffer> commandBuffers;
	//Skybox must go first to render behind everything
	skybox->draw(&commandBuffers, inheritanceInfo);

//	updateParticleCommandBuffer(inheritanceInfo);
//	commandBuffers.emplace_back(particleCommandBuffer);

	terrain->draw(&commandBuffers, inheritanceInfo);

	for(int i = 0; i < numThread; i++)
	{
		for(int j = 0; j < numPerThread; j++)
		{
			//Create new thread and run>
			threadPool.addJob([=]{threadedRender(i,j, inheritanceInfo);}, i);
		}
	}
	//Wait for all threads to finish>
	threadPool.wait();

	for(int i = 0; i < numThread; i++)
	{
		for(int j = 0; j < numPerThread; j++)
		{
			commandBuffers.emplace_back(threadData[i].commandBuffers[j]);
		}
	}
	vkCmdExecuteCommands(primaryCommandBuffer, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

	vkCmdEndRenderPass(primaryCommandBuffer);

	VK_RESULT_CHECK(vkEndCommandBuffer(primaryCommandBuffer))
}

void VulkanInterface::draw()
{
	uint32_t imageIndex;
	VkResult result;
	do
	{
		result = vkAcquireNextImageKHR(logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(),
		                                        imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			recreateSwapchain();
		}
		else VK_RESULT_CHECK(result)
	}
	while(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR);

	updateCommandBuffers();
	updateScreenCommandBuffer(swapchainFramebuffers[imageIndex]);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &primaryCommandBuffer;
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &offscreenRenderedSemaphore;

	VK_RESULT_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr));

	submitInfo.pWaitSemaphores = &offscreenRenderedSemaphore;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
	submitInfo.pCommandBuffers = &screenCommandBuffer;
	VK_RESULT_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr));

	//Submit frame to swapchain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

	VkSwapchainKHR swapChains[] = {swapchain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	vkQueueWaitIdle(presentQueue);

	/*void drawFrame() {
		updateAppState();
		vkQueueWaitIdle(presentQueue);
		vkAcquireNextImageKHR(...)
		submitDrawCommands();
		vkQueuePresentKHR(presentQueue, &presentInfo);
	}*/
}

void VulkanInterface::waitForIdle()
{
	vkDeviceWaitIdle(logicalDevice);
}

void VulkanInterface::initDebug()
{
	if(!enableValidationLayers) return;

	LOAD_IFUNCTION(vulkanInstance, vkCreateDebugReportCallbackEXT);

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = vulkanInterfaceDebugCallback;

	vkCreateDebugReportCallbackEXT(vulkanInstance, &createInfo, nullptr, &debugCallback);
}

void VulkanInterface::destroyDebug()
{
	if(debugCallback)
	{
		LOAD_IFUNCTION(vulkanInstance, vkDestroyDebugReportCallbackEXT);

		vkDestroyDebugReportCallbackEXT(vulkanInstance, debugCallback, nullptr);
		Logger() << "Vulkan Debug destroyed";
	}
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	std::string deviceTypes[] = {"VK_PHYSICAL_DEVICE_TYPE_OTHER",
								 "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU",
								 "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU",
								 "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU",
								 "VK_PHYSICAL_DEVICE_TYPE_CPU"};

	VkPhysicalDeviceProperties physicalProperties = {};
	VkPhysicalDeviceFeatures physicalFeatures = {};
	vkGetPhysicalDeviceProperties(device, &physicalProperties);
	vkGetPhysicalDeviceFeatures(device, &physicalFeatures);
	Logger() <<    "Device Name: " << physicalProperties.deviceName;
	Logger() <<    "Device Type: " << deviceTypes[physicalProperties.deviceType];
	Logger() << "Driver Version: " << physicalProperties.driverVersion;
	Logger() <<    "API Version: " << VK_VERSION_MAJOR(physicalProperties.apiVersion) << "."
			  << VK_VERSION_MINOR(physicalProperties.apiVersion) << "."
			  << VK_VERSION_PATCH(physicalProperties.apiVersion);

	if(physicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			physicalFeatures.geometryShader && physicalFeatures.samplerAnisotropy)
	{
		VulkanQueues queues = findQueueFamilies(device, surface);
		if(queues.isComplete())
		{
			bool extensionsSupported = checkDeviceExtensionSupport(device);
			if(extensionsSupported)
			{
				SwapChainSupportDetails swapChainDetails = querySwapChainSupport(device, surface);
				if(!swapChainDetails.formats.empty() && !swapChainDetails.presentModes.empty())
				{
					Logger() << "Using device \"" << physicalProperties.deviceName << "\"";
					return true;
				}
			}
		}
	}

	return false;
}

VulkanQueues findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	VulkanQueues queues;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for(const auto& queueFamily : queueFamilies)
	{
		if(queueFamily.queueCount)
		{
			if(queues.graphicsFamily == -1 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queues.graphicsFamily = i;
			}

			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), surface, &presentSupport);
			if(queues.presentFamily == -1 && presentSupport)
			{
				queues.presentFamily = i;
			}
		}
		i++;
	}
	return queues;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

bool VulkanInterface::checkValidationLayers()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for(auto layerName : validationLayers)
	{
		bool layerFound = false;
		for(auto layerProps : availableLayers)
		{
			if(strcmp(layerName, layerProps.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}
		if(!layerFound)
		{
			std::string errorString = "Validation layer ";
			errorString += layerName;
			errorString += " could not be found.";
			Logger() << errorString;
			throw std::runtime_error(errorString);
		}
	}

	std::set<std::string> requiredLayers(validationLayers.begin(), validationLayers.end());
	for(const auto& layer : availableLayers) {
		requiredLayers.erase(layer.layerName);
	}
	for(auto& leftoverLayer : requiredLayers)
	{
		Logger() << leftoverLayer << " validation layer missing";
	}

	return true;
}

std::vector<const char*> VulkanInterface::getRequiredExtensions()
{
	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	Logger() << "GLFW Required Extensions:";
	for(auto i = 0; i < glfwExtensionCount; ++i)
	{
		Logger() << "\t" << glfwExtensions[i];
		extensions.push_back(glfwExtensions[i]);
	}

	if(enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}
	return extensions;
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	if(formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
	if(presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats)
{
	if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	for(const auto& availableFormat : availableFormats)
	{
		if(availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
{
//	for(const auto& availablePresentMode : availablePresentModes)
//	{
//		if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
//		{
//			return availablePresentMode;
//		}
//	}

	//Vsync
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetWindowSize(window->glfwWindow, &width, &height);

		VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

VkFormat findSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                             VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props{};
		vkGetPhysicalDeviceFormatProperties(device, format, &props);

		if(tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if(tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	Logger() << "Supported format not found";
	throw std::runtime_error("Failed to find supported format");
}

VkFormat findDepthFormat(VkPhysicalDevice device)
{
	return findSupportedFormat(
			device,
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkPipelineShaderStageCreateInfo VulkanInterface::loadShaderModule(const std::string &shaderFilename,
                                                                  VkShaderStageFlagBits stage)
{
	std::ifstream shaderStream(shaderFilename.c_str(), std::ios::binary);
	std::string shaderCode((std::istreambuf_iterator<char>(shaderStream)),
	                       (std::istreambuf_iterator<char>()));

	VkShaderModuleCreateInfo shaderCreationInfo = {};
	shaderCreationInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderCreationInfo.codeSize = shaderCode.size();
	shaderCreationInfo.pCode = reinterpret_cast<const uint32_t *>(shaderCode.c_str());

	VkShaderModule shaderModule;
	VkResult res = vkCreateShaderModule(logicalDevice, &shaderCreationInfo, nullptr, &shaderModule);
	if(res != VK_SUCCESS)
	{
		Logger() << "Shader module creation failed [" << shaderFilename << "]";
		std::string errorString;
		VK_RESULT_CHECK(res)
	}
	Logger() << "Shader module created [" << shaderFilename << "]";

	VkPipelineShaderStageCreateInfo shaderStageInfo = {};
	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageInfo.stage = stage;
	shaderStageInfo.module = shaderModule;
	shaderStageInfo.pName = "main";

	shaderModules.emplace_back(shaderModule);

	return shaderStageInfo;
}

VkVertexInputBindingDescription bindingDescription(uint32_t binding, uint32_t stride,
                                                   VkVertexInputRate inputType)
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = binding;
	bindingDescription.stride = stride;
	bindingDescription.inputRate = inputType;
	return bindingDescription;
}

VkVertexInputAttributeDescription attributeDescription(uint32_t binding, uint32_t location,
                                                       VkFormat format, uint32_t offset)
{
	VkVertexInputAttributeDescription attributeDescription = {};
	attributeDescription.binding = binding;
	attributeDescription.location = location;
	attributeDescription.format = format;
	attributeDescription.offset = offset;
	return attributeDescription;
}

VkVertexInputAttributeDescription attributeDescription(uint32_t binding, uint32_t location,
                                                       VkFormat format, size_t offset)
{
	return attributeDescription(binding, location, format, static_cast<uint32_t>(offset));
}

std::vector<VkVertexInputBindingDescription> modelBindingDescription()
{
	return {bindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)};
}

std::vector<VkVertexInputBindingDescription> particleBindingDescription()
{
	return {
		bindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		bindingDescription(1, sizeof(ParticleInstanceData), VK_VERTEX_INPUT_RATE_INSTANCE)
	};
}

std::vector<VkVertexInputBindingDescription> screenBindingDescription()
{
	return {bindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)};
}

std::vector<VkVertexInputAttributeDescription> modelAttributeDescription()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
	uint32_t index = 0;

	attributeDescriptions[index] = attributeDescription(0, index, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position));
	index++;
	attributeDescriptions[index] = attributeDescription(0, index, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv));
	index++;
	attributeDescriptions[index] = attributeDescription(0, index, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal));

	return attributeDescriptions;
}

std::vector<VkVertexInputAttributeDescription> screenAttributeDescription()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
	uint32_t index = 0;

	attributeDescriptions[index] = attributeDescription(0, index, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position));
	index++;
	attributeDescriptions[index] = attributeDescription(0, index, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv));

	return attributeDescriptions;
}

std::vector<VkVertexInputAttributeDescription> particleAttributeDescription()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions = modelAttributeDescription();
	auto index = static_cast<uint32_t>(attributeDescriptions.size());

	uint32_t matSize = 4;
	attributeDescriptions.resize(index + matSize);

	//Instance mat4
	for(uint32_t i = 0; i < matSize; i++)
	{
		attributeDescriptions[index+i] = attributeDescription(1, index+i, VK_FORMAT_R32G32B32A32_SFLOAT,
		                                                      offsetof(ParticleInstanceData, transform) + sizeof(glm::vec4)*i);
	}

	return attributeDescriptions;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags)
{
	VkPhysicalDeviceMemoryProperties memProperties = {};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if((typeFilter & (1 << i)) &&
		   (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanInterface::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
				  VkMemoryPropertyFlags propertyFlags, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //No need to share

	VK_RESULT_CHECK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer))
	//Logger() << "Buffer created";

	VkMemoryRequirements memoryRequirements = {};
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, propertyFlags);

	VK_RESULT_CHECK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory))
	//Logger() << "Buffer memory allocated";
	vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

void VulkanInterface::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer copyCommandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(copyCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(copyCommandBuffer);
}

VkCommandBuffer VulkanInterface::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//Begin recording
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanInterface::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void VulkanInterface::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layers)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layers;

	if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if(hasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		Logger() << "Unsupported layout transition";
		throw std::invalid_argument("Unsupported layout transition");
	}

	vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
	);

	endSingleTimeCommands(commandBuffer);
}

void VulkanInterface::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {
			width,
			height,
			1
	};

	vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
	);

	endSingleTimeCommands(commandBuffer);
}

void VulkanInterface::createImage(uint32_t width, uint32_t height, uint32_t layers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, VkImageCreateFlags flags)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = layers;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.flags = flags;

	VK_RESULT_CHECK(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image))
	//Logger() << "Image created";

	VkMemoryRequirements memRequirements = {};
	vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

	VK_RESULT_CHECK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory))
	//Logger() << "Image memory allocated";

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

VkImageView createImageView(VkDevice logicalDevice, VkImageViewType viewType, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t layers)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = viewType;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layers;

	VkImageView imageView;
	VK_RESULT_CHECK(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView))
	//Logger() << "Image view created";

	return imageView;
}
