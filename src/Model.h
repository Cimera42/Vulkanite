//
// Created by Tim on 2/11/2017.
//

#ifndef VULKANITE_MODEL_H
#define VULKANITE_MODEL_H

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include "Texture.h"
#include "Mesh.h"

class VulkanInterface;

struct InstanceData
{
	glm::vec3 pos;
};

class Model
{
	void load(std::string filename);

	Mesh * mesh;

	VulkanInterface * vki;

public:
	explicit Model(VulkanInterface *inVulkanInterface, std::string filename);
	~Model();
	void draw(VkCommandBuffer commandBuffer);
	void draw(VkCommandBuffer commandBuffer, uint32_t instanceCount);

	Texture * texture;
};


#endif //VULKANITE_MODEL_H
