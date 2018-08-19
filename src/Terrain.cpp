//
// Created by Tim on 27/11/2017.
//

#include "Terrain.h"
#include "vulkanInterface.h"
#include "logger.h"
#include <stb_image.h>

Terrain::Terrain(VulkanInterface *inVulkan, std::string filename):
		vki(inVulkan)
{
	loadData(filename);
}

Terrain::~Terrain()
{
	delete texture;

	vkDestroyDescriptorSetLayout(vki->logicalDevice, descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(vki->logicalDevice, pipelineLayout, nullptr);
	vkDestroyPipeline(vki->logicalDevice, pipeline, nullptr);
	vkDestroyBuffer(vki->logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, indexBufferMemory, nullptr);
	vkDestroyBuffer(vki->logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, vertexBufferMemory, nullptr);
}

glm::vec3 constructNormal(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3)
{
	glm::vec3 u = p2 - p1;
	glm::vec3 v = p3 - p1;

	glm::vec3 normal = glm::vec3(
			(u.y * v.z) - (u.z * v.y),
			(u.z * v.x) - (u.x * v.z),
			(u.x * v.y) - (u.y * v.x)
	);

	return normal;
}

void Terrain::loadData(std::string filename)
{
	int comp;
	int vertWidth;
	int vertHeight;
	stbi_uc* heightData = stbi_load(filename.c_str(), &vertWidth, &vertHeight, &comp, STBI_rgb_alpha);
	if(!heightData)
		throw std::runtime_error("Heightmap could not be loaded");

	int tileWidth = vertWidth-1;
	int tileHeight = vertHeight-1;
	float desiredWidth = 50;
	float desiredHeight = 10;
	float desiredDepth = 50;

	vertices.reserve(static_cast<uint32_t>(vertWidth * vertHeight));
	for(int i = 0; i < vertHeight; i++)
	{
		for(int j = 0; j < vertWidth; j++)
		{
			TerrainVertex v{};
			v.position = glm::vec3(j*(desiredWidth/tileWidth),0,i*(desiredDepth/tileHeight));
			v.position.y = heightData[(i*vertWidth + j)*4] / 256.0f * desiredHeight ;
			vertices.emplace_back(v);
		}
	}

	stbi_image_free(heightData);

	indices.reserve(static_cast<uint32_t>(tileWidth*tileHeight * 6));
	for(int i = 0; i < tileHeight; i++)
	{
		for(int j = 0; j < tileWidth; j++)
		{
			indices.emplace_back((i+1)*vertWidth + j+1);
			indices.emplace_back(i*vertWidth + j+1);
			indices.emplace_back(i*vertWidth + j);

			indices.emplace_back(i*vertWidth + j);
			indices.emplace_back((i+1)*vertWidth + j);
			indices.emplace_back((i+1)*vertWidth + j+1);
		}
	}

	for(int i = 0; i < indices.size(); i += 3)
	{
		TerrainVertex* v1 = &vertices[indices[i]];
		TerrainVertex* v2 = &vertices[indices[i+1]];
		TerrainVertex* v3 = &vertices[indices[i+2]];

		glm::vec3 normal = constructNormal(v1->position, v2->position, v3->position);
		v1->normal += normal;
		v2->normal += normal;
		v3->normal += normal;
	}

	for(auto &&vertex : vertices)
	{
		vertex.normal = glm::normalize(vertex.normal);
//		vertex.normal = (vertex.normal + glm::vec3(1))/2.0f;
	}

	createVertexBuffer();
	createIndexBuffer();

	createTexture();
	createDescriptor();
	createPipeline();
	allocateCommandBuffers();
}

void Terrain::createTexture()
{
	texture = new Texture(vki, {"images/rock.jpg", "images/grass.jpg"}, false);
}

void Terrain::createDescriptor()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr; // Optional

	std::vector<VkDescriptorSetLayoutBinding> bindings = {samplerLayoutBinding};
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(vki->logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout))
	Logger() << "Descriptor set layout created";

	VkDescriptorSetAllocateInfo allocInfo = {};
	std::vector<VkDescriptorSetLayout> layouts = {
			descriptorSetLayout
	};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vki->descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	VK_RESULT_CHECK(vkAllocateDescriptorSets(vki->logicalDevice, &allocInfo, &descriptorSet))
	Logger() << "Descriptor sets allocated";

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture->texture.imageView;
	imageInfo.sampler = texture->textureSampler;

	std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = descriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(vki->logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

std::vector<VkVertexInputBindingDescription> Terrain::getBindingDescription()
{
	return {
			bindingDescription(0, sizeof(TerrainVertex), VK_VERTEX_INPUT_RATE_VERTEX)
	};
}

std::vector<VkVertexInputAttributeDescription> Terrain::getAttributeDescription()
{
	return {
			attributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, position)),
			attributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TerrainVertex, normal))
	};
}

void Terrain::createPipeline()
{
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size = sizeof(PushConstantBufferObject);
	pushConstantRange.offset = 0;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	//Used for shader uniforms
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1; // Optional
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 1; // Optional
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange; // Optional

	VK_RESULT_CHECK(vkCreatePipelineLayout(vki->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout))
	Logger() << "Pipeline layout created";


	PipelineData pipelineData = vki->defaultPipelineData;
	pipelineData.vertexBinding = getBindingDescription();
	pipelineData.vertexAttributes = getAttributeDescription();
	pipelineData.pipelineInfo.layout = pipelineLayout;
	pipelineData.pipelineInfo.renderPass = vki->offscreenRenderPass;
	//2 extra attachments since deferred rendering
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.shaderStages = {
			vki->loadShaderModule("shaders/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			vki->loadShaderModule("shaders/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	vki->createPipeline(pipelineData, &pipeline);
	Logger() << "Terrain pipeline created";
}

void Terrain::allocateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vki->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(vki->logicalDevice, &allocInfo, &commandBuffer));
	VK_RESULT_CHECK(vkAllocateCommandBuffers(vki->logicalDevice, &allocInfo, &pushConstantCommandBuffer));
}

void Terrain::fillCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
	                        &descriptorSet, 0, nullptr);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0,1, &vertexBuffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0,VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()),
	                 1, 0, 0, 0);

	VK_RESULT_CHECK(vkEndCommandBuffer(commandBuffer));

	commandBufferFilled = true;
}

void Terrain::updatePushConstantCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	vkBeginCommandBuffer(pushConstantCommandBuffer, &commandBufferBeginInfo);

	PushConstantBufferObject pushConstant = {};
	pushConstant.view = vki->pushConstant.view;
	pushConstant.proj = vki->pushConstant.proj;
	pushConstant.model = glm::mat4(1.0f);

	vkCmdPushConstants(pushConstantCommandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
	                   sizeof(pushConstant), &pushConstant);

	VK_RESULT_CHECK(vkEndCommandBuffer(pushConstantCommandBuffer));
}

void Terrain::draw(std::vector<VkCommandBuffer> * commandBuffers, VkCommandBufferInheritanceInfo inheritanceInfo)
{
	if(!commandBufferFilled)
		fillCommandBuffer(inheritanceInfo);

	updatePushConstantCommandBuffer(inheritanceInfo);

	commandBuffers->emplace_back(pushConstantCommandBuffer);
	commandBuffers->emplace_back(commandBuffer);
}

void Terrain::createVertexBuffer()
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                  stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(vki->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), bufferSize);
	vkUnmapMemory(vki->logicalDevice, stagingBufferMemory);

	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	                  vertexBuffer, vertexBufferMemory);

	vki->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(vki->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, stagingBufferMemory, nullptr);
}

void Terrain::createIndexBuffer()
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                  stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(vki->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), bufferSize);
	vkUnmapMemory(vki->logicalDevice, stagingBufferMemory);

	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	                  indexBuffer, indexBufferMemory);

	vki->copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(vki->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, stagingBufferMemory, nullptr);
}