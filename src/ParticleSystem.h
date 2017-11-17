//
// Created by Tim on 14/11/2017.
//

#ifndef VULKANITE_PARTICLESYSTEM_H
#define VULKANITE_PARTICLESYSTEM_H

#include "GenericThreadPool.h"
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>
#include "vulkanInterface.h"

struct Particle
{
	bool alive;
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 velocity;
};

struct ParticleInstanceData
{
	glm::mat4 transform;
};

class ParticleSystem
{
	VulkanInterface * vki;
	uint32_t maxParticles;
	std::vector<Particle*> particles;
	std::vector<ParticleInstanceData> particleInstanceData;

	VkDeviceSize instanceBufferSize;
	VkBuffer instanceBuffer;
	VkDeviceMemory instanceBufferMemory;

	std::mutex matricesMutex;
	GenericThreadPool threadPool;

	void initParticles();
	void particleUpdate(Particle *particle);
	void loadModel(std::string filename);
	void prepareInstanceBuffer();
	void copyMatrices();

public:
	explicit ParticleSystem(VulkanInterface *inVulkanInterface, std::string particleModelFilename);
	~ParticleSystem();

	Model* particleModel;

	void update();
	void draw(VkCommandBuffer commandBuffer);
};


#endif //VULKANITE_PARTICLESYSTEM_H
