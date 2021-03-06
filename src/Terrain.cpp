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
	texture = new Texture(vki, {"images/rock.jpg", "images/sand.jpg"}, false);
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
	auto bindingDescription = getBindingDescription();
	auto attributeDescription = getAttributeDescription();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescription.size());
	vertexInputInfo.pVertexBindingDescriptions = bindingDescription.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) vki->window->width;
	viewport.height = (float) vki->window->height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = {static_cast<uint32_t>(vki->window->width),
	                  static_cast<uint32_t>(vki->window->height)};

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
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 1; // Optional
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange; // Optional

	VK_RESULT_CHECK(vkCreatePipelineLayout(vki->logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout))
	Logger() << "Pipeline layout created";

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
			vki->loadShaderModule("shaders/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			vki->loadShaderModule("shaders/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
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
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = vki->offscreenRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_RESULT_CHECK(vkCreateGraphicsPipelines(vki->logicalDevice, vki->pipelineCache, 1, &pipelineInfo, nullptr, &pipeline))
	Logger() << "Standard pipeline created";
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

void Terrain::updateCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
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
		updateCommandBuffer(inheritanceInfo);

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