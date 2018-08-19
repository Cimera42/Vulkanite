//
// Created by Tim on 27/11/2017.
//

#ifndef VULKANITE_TERRAIN_H
#define VULKANITE_TERRAIN_H

#include <glm/vec3.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include <string>

class VulkanInterface;
class Texture;

struct TerrainVertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

class Terrain
{
	VulkanInterface* vki;

	std::vector<TerrainVertex> vertices;
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

	void loadData(std::string filename);
	void createVertexBuffer();
	void createIndexBuffer();

	void createTexture();
	void createDescriptor();
	std::vector<VkVertexInputBindingDescription> getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> getAttributeDescription();
	void createPipeline();
	void allocateCommandBuffers();
	void fillCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);
	void updatePushConstantCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);

public:
	explicit Terrain(VulkanInterface* inVulkan, std::string filename);
	~Terrain();

	void draw(std::vector<VkCommandBuffer> * commandBuffers,
	          VkCommandBufferInheritanceInfo inheritanceInfo);
};

#endif //VULKANITE_TERRAIN_H
