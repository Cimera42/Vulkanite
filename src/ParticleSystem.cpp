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
	initParticles();
	loadModel(std::move(particleModelFilename));
}

ParticleSystem::~ParticleSystem()
{
	vkDestroyBuffer(vki->logicalDevice, instanceBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, instanceBufferMemory, nullptr);

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
	update();
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

void ParticleSystem::draw(VkCommandBuffer commandBuffer)
{
	update();

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 1,1, &instanceBuffer, offsets);
	particleModel->draw(commandBuffer, static_cast<uint32_t>(particleInstanceData.size()));
}