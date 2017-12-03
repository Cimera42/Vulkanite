//
// Created by Tim on 2/12/2017.
//

#ifndef VULKANITE_SKYBOX_H
#define VULKANITE_SKYBOX_H

#include <glm/vec3.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <string>

class VulkanInterface;
class Texture;

struct SkyboxVertex
{
	glm::vec3 position;
};

class Skybox
{
	VulkanInterface* vki;

	std::vector<SkyboxVertex> vertices;
	std::vector<uint32_t> indices;

	Texture* texture;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	bool commandBufferFilled = false;
	VkCommandBuffer commandBuffer = nullptr;
	VkCommandBuffer pushConstantCommandBuffer = nullptr;

	void loadData();
	void createVertexBuffer();
	void createIndexBuffer();

	void createTexture();
	void createDescriptor();
	std::vector<VkVertexInputBindingDescription> getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> getAttributeDescription();
	void createPipeline();
	void allocateCommandBuffers();
	void updateCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);
	void updatePushConstantCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);

public:
	explicit Skybox(VulkanInterface* inVulkan);
	~Skybox();

	void draw(std::vector<VkCommandBuffer> * commandBuffers,
	          VkCommandBufferInheritanceInfo inheritanceInfo);
};

#endif //VULKANITE_SKYBOX_H
