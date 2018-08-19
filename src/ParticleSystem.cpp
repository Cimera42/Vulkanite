//
// Created by Tim on 14/11/2017.
//

#include "ParticleSystem.h"
#include "logger.h"
#include <random>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

ParticleSystem::ParticleSystem(VulkanInterface *inVulkanInterface, std::string particleModelFilename) :
	vki(inVulkanInterface)
{
	maxParticles = 1000;
	threadPool.resize(8);
	loadModel(std::move(particleModelFilename));
	initParticles();
}

ParticleSystem::~ParticleSystem()
{
	vkDestroyBuffer(vki->logicalDevice, instanceBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, instanceBufferMemory, nullptr);

	vkDestroyDescriptorSetLayout(vki->logicalDevice, descriptorSetLayout, nullptr);
	vkDestroyPipelineLayout(vki->logicalDevice, pipelineLayout, nullptr);
	vkDestroyPipeline(vki->logicalDevice, pipeline, nullptr);

	delete particleModel;
	threadPool.destroy();
}

void ParticleSystem::initParticles()
{
	particles.resize(maxParticles);
	std::random_device seed;
	std::mt19937 randGen(seed());
	std::uniform_real_distribution<float> uniform_dist(0,10);
	std::uniform_real_distribution<float> vel_dist(-1,1);

	for(auto && particle : particles)
	{
		particle = new Particle();
		particle->alive = true;
		particle->position = glm::vec3(uniform_dist(randGen),
		                              uniform_dist(randGen),
		                              uniform_dist(randGen));
		particle->rotation = glm::quat(1,0,0,0);
		particle->velocity = glm::vec3(0);
	}

	prepareInstanceBuffer();
	createDescriptor();
	createPipeline();
	allocateCommandBuffers();
	update();
}

void ParticleSystem::createDescriptor()
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
	imageInfo.imageView = particleModel->texture->texture.imageView;
	imageInfo.sampler = particleModel->texture->textureSampler;

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

std::vector<VkVertexInputBindingDescription> ParticleSystem::getBindingDescription()
{
	return {
			bindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
			bindingDescription(1, sizeof(ParticleInstanceData), VK_VERTEX_INPUT_RATE_INSTANCE)
	};
}

std::vector<VkVertexInputAttributeDescription> ParticleSystem::getAttributeDescription()
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

void ParticleSystem::createPipeline()
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
	pipelineData.pipelineInfo.layout = pipelineLayout;
	//2 extra attachments since deferred rendering
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.colorBlendAttachments.emplace_back(pipelineData.colorBlendAttachments[0]);
	pipelineData.shaderStages = {
			vki->loadShaderModule("shaders/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			vki->loadShaderModule("shaders/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	vki->createPipeline(pipelineData, &pipeline);
	Logger() << "Particle pipeline created";
}

void ParticleSystem::allocateCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vki->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(vki->logicalDevice, &allocInfo, &commandBuffer));
}

void ParticleSystem::update()
{
	particleInstanceData.clear();
	for (auto &&particle : particles)
	{
		if (particle->alive)
		{
			threadPool.addJob(std::bind(particleUpdate, this, particle));
		}
	}
	threadPool.wait();

	copyMatrices();
}

void ParticleSystem::particleUpdate(Particle *particle)
{
	particle->velocity.y -= 9.8*0.0001f;
	particle->position += particle->velocity;
	if(particle->position.y < 0)
	{
		particle->position.y = 0;
		particle->velocity.y *= -1;
	}
	if(particle->position.x > 10) {particle->position.x = 10; particle->velocity.x *= -1;}
	if(particle->position.x < 0) {particle->position.x = 0; particle->velocity.x *= -1;}
	if(particle->position.z > 10) {particle->position.z = 10; particle->velocity.z *= -1;}
	if(particle->position.z < 0) {particle->position.z = 0; particle->velocity.z *= -1;}

	glm::mat4 particleMatrix = glm::translate(particle->position);
	//particleMatrix *= glm::toMat4(particle->rotation);

	ParticleInstanceData d = {particleMatrix};
	std::lock_guard<std::mutex> lock(matricesMutex);
	particleInstanceData.emplace_back(d);
}

void ParticleSystem::loadModel(std::string filename)
{
	particleModel = new Model(vki, std::move(filename));
}

void ParticleSystem::prepareInstanceBuffer()
{
	instanceBufferSize = sizeof(ParticleInstanceData) * maxParticles;
	vki->createBuffer(instanceBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	                  instanceBuffer, instanceBufferMemory);
}

void ParticleSystem::copyMatrices()
{
	void* data;
	vkMapMemory(vki->logicalDevice, instanceBufferMemory, 0, instanceBufferSize, 0, &data);
	memcpy(data, particleInstanceData.data(), instanceBufferSize);
	vkUnmapMemory(vki->logicalDevice, instanceBufferMemory);
}

void ParticleSystem::draw(std::vector<VkCommandBuffer> * commandBuffers, VkCommandBufferInheritanceInfo inheritanceInfo)
{
	update();

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

	VK_RESULT_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
	                        0, 1, &descriptorSet, 0, nullptr);

	ViewPushConstantBufferObject pushConstant = {};
	pushConstant.view = vki->pushConstant.view;
	pushConstant.proj = vki->pushConstant.proj;

	vkCmdPushConstants(
			commandBuffer,
			pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(pushConstant),
			&pushConstant
	);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 1,1, &instanceBuffer, offsets);
	particleModel->draw(commandBuffer, static_cast<uint32_t>(particleInstanceData.size()));

	VK_RESULT_CHECK(vkEndCommandBuffer(commandBuffer));

	commandBuffers->emplace_back(commandBuffer);
}