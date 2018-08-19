//
// Created by Tim on 27/11/2017.
//

#include "Skybox.h"
#include "vulkanInterface.h"
#include "logger.h"

Skybox::Skybox(VulkanInterface *inVulkan):
		vki(inVulkan)
{
	loadData();
}

Skybox::~Skybox()
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

void Skybox::loadData()
{
	//Taken from https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_05
	//Cbf doing it myself
	vertices = {
			// front
			{{-1.0, -1.0, 1.0}},
			{{1.0, -1.0, 1.0}},
			{{1.0, 1.0, 1.0}},
			{{-1.0, 1.0, 1.0}},
			// back
			{{-1.0, -1.0, -1.0}},
			{{1.0, -1.0, -1.0}},
			{{1.0, 1.0, -1.0}},
			{{-1.0, 1.0, -1.0}},
	};

	indices = {
			// front
			0, 1, 2,
			2, 3, 0,
			// top
			1, 5, 6,
			6, 2, 1,
			// back
			7, 6, 5,
			5, 4, 7,
			// bottom
			4, 0, 3,
			3, 7, 4,
			// left
			4, 5, 1,
			1, 0, 4,
			// right
			3, 2, 6,
			6, 7, 3,
	};

	createVertexBuffer();
	createIndexBuffer();

	createTexture();
	createDescriptor();
	createPipeline();
	allocateCommandBuffers();
}

void Skybox::createTexture()
{
	/*
	 * For cube and cube array image views, the layers of the image
	 * view starting at baseArrayLayer correspond to faces in the order
	 * +X, -X, +Y, -Y, +Z, -Z.
	 */
	texture = new Texture(vki, {
			"images/envmap_interstellar/interstellar_lf.tga",
			"images/envmap_interstellar/interstellar_rt.tga",
			"images/envmap_interstellar/interstellar_up.tga",
			"images/envmap_interstellar/interstellar_dn.tga",
			"images/envmap_interstellar/interstellar_ft.tga",
			"images/envmap_interstellar/interstellar_bk.tga"
	}, true);
}

void Skybox::createDescriptor()
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

std::vector<VkVertexInputBindingDescription> Skybox::getBindingDescription()
{
	return {
			bindingDescription(0, sizeof(SkyboxVertex), VK_VERTEX_INPUT_RATE_VERTEX)
	};
}

std::vector<VkVertexInputAttributeDescription> Skybox::getAttributeDescription()
{
	return {
			attributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SkyboxVertex, position))
	};
}

void Skybox::createPipeline()
{
	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.size = sizeof(ViewPushConstantBufferObject);
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
	pipelineData.depthStencil.depthTestEnable = VK_FALSE; //Render behind everything
	pipelineData.pipelineInfo.layout = pipelineLayout;
	pipelineData.pipelineInfo.renderPass = vki->offscreenRenderPass;
	//2 extra attachments since deferred rendering
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.shaderStages = {
			vki->loadShaderModule("shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			vki->loadShaderModule("shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	vki->createPipeline(pipelineData, &pipeline);
	Logger() << "Skybox pipeline created";
}

void Skybox::allocateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vki->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(vki->logicalDevice, &allocInfo, &commandBuffer));
	VK_RESULT_CHECK(vkAllocateCommandBuffers(vki->logicalDevice, &allocInfo, &pushConstantCommandBuffer));
}

void Skybox::fillCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
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

void Skybox::updatePushConstantCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	vkBeginCommandBuffer(pushConstantCommandBuffer, &commandBufferBeginInfo);

	ViewPushConstantBufferObject pushConstant = {};
	pushConstant.view = vki->pushConstant.view;
	//Stop skybox from 'moving'
	pushConstant.view[3][0] = 0;
	pushConstant.view[3][1] = 0;
	pushConstant.view[3][2] = 0;
	pushConstant.proj = vki->pushConstant.proj;

	vkCmdPushConstants(pushConstantCommandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
	                   sizeof(pushConstant), &pushConstant);

	VK_RESULT_CHECK(vkEndCommandBuffer(pushConstantCommandBuffer));
}

void Skybox::draw(std::vector<VkCommandBuffer> * commandBuffers, VkCommandBufferInheritanceInfo inheritanceInfo)
{
	if(!commandBufferFilled)
		fillCommandBuffer(inheritanceInfo);

	updatePushConstantCommandBuffer(inheritanceInfo);

	commandBuffers->emplace_back(pushConstantCommandBuffer);
	commandBuffers->emplace_back(commandBuffer);
}

void Skybox::createVertexBuffer()
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

void Skybox::createIndexBuffer()
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