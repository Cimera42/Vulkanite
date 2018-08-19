//
// Created by Tim on 2/11/2017.
//

#ifndef VULKANITE_MESH_H
#define VULKANITE_MESH_H

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <vector>
#include <assimp/scene.h>
#include <vulkan/vulkan.h>

class VulkanInterface;

struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

class Mesh
{
	void createVertexBuffer();
	void createIndexBuffer();

	VulkanInterface* vki;
	VkDeviceMemory vertexBufferMemory;
	VkDeviceMemory indexBufferMemory;

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	std::vector<Vertex> collatedVertices;
public:
	explicit Mesh(VulkanInterface* inVulkanInterface);
	~Mesh();
	void load(aiMesh* assimpMesh);
	void load(std::vector<glm::vec3> inVertices,
	          std::vector<glm::vec2> inUVs,
	          std::vector<glm::vec3> inNormals,
	          std::vector<uint32_t> inIndices);

	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	std::vector<uint32_t> indices;
};

#endif //VULKANITE_MESH_H
