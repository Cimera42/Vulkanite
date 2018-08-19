//
// Created by Tim on 2/11/2017.
//

#include "Mesh.h"

#include <utility>
#include "vulkanInterface.h"
#include "logger.h"

Mesh::Mesh(VulkanInterface* inVulkanInterface) :
		vki(inVulkanInterface)
{

}

Mesh::~Mesh()
{
	vkDestroyBuffer(vki->logicalDevice, indexBuffer, nullptr);
	Logger() << "Index buffer destroyed";
	vkFreeMemory(vki->logicalDevice, indexBufferMemory, nullptr);
	Logger() << "Index buffer memory freed";

	vkDestroyBuffer(vki->logicalDevice, vertexBuffer, nullptr);
	Logger() << "Vertex buffer destroyed";
	vkFreeMemory(vki->logicalDevice, vertexBufferMemory, nullptr);
	Logger() << "Vertex buffer memory freed";
}

void Mesh::load(aiMesh* assimpMesh)
{
	for(unsigned int j = 0; j < assimpMesh->mNumFaces; j++)
	{
		aiFace& assimpFace = assimpMesh->mFaces[j];
		for(unsigned int k = 0; k < assimpFace.mNumIndices; k++)
		{
			indices.push_back(assimpFace.mIndices[k]);
		}
	}
	for(unsigned int j = 0; j < assimpMesh->mNumVertices; j++)
	{
		aiVector3D vertex = assimpMesh->mVertices[j];
		glm::vec3 glmVert = glm::vec3(vertex.x,vertex.y,vertex.z);
		vertices.push_back(glmVert);

		aiVector3D uv = assimpMesh->mTextureCoords[0][j];
		glm::vec2 glmUv = glm::vec2(uv.x,1-uv.y);
		uvs.push_back(glmUv);

		aiVector3D normal = assimpMesh->mNormals[j];
		glm::vec3 glmNormal = glm::vec3(normal.x,normal.y,normal.z);
		normals.push_back(glmNormal);

		//materialIndices.push_back(assimpMesh->mMaterialIndex);

		Vertex collatedVertex;
		collatedVertex.position = glmVert;
		collatedVertex.uv = glmUv;
		collatedVertex.normal = glmNormal;
		//collatedVertex.materialIndex = assimpMesh->mMaterialIndex;
		collatedVertices.push_back(collatedVertex);
	}

	createVertexBuffer();
	createIndexBuffer();
}

void Mesh::load(std::vector<glm::vec3> inVertices,
                std::vector<glm::vec2> inUVs,
                std::vector<glm::vec3> inNormals,
                std::vector<uint32_t> inIndices)
{
	vertices = std::move(inVertices);
	uvs = std::move(inUVs);
	normals = std::move(inNormals);
	indices = std::move(inIndices);

	size_t size = vertices.size();
	for(auto j = 0; j < size; j++)
	{
		//materialIndices.push_back(assimpMesh->mMaterialIndex);

		Vertex collatedVertex;
		collatedVertex.position = vertices[j];
		collatedVertex.uv = uvs[j];
		collatedVertex.normal = normals[j];
		//collatedVertex.materialIndex = assimpMesh->mMaterialIndex;
		collatedVertices.push_back(collatedVertex);
	}

	createVertexBuffer();
	createIndexBuffer();
}

void Mesh::createVertexBuffer()
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkDeviceSize bufferSize = sizeof(collatedVertices[0]) * collatedVertices.size();
	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					  stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(vki->logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, collatedVertices.data(), bufferSize);
	vkUnmapMemory(vki->logicalDevice, stagingBufferMemory);

	vki->createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					  vertexBuffer, vertexBufferMemory);

	vki->copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(vki->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, stagingBufferMemory, nullptr);
}

void Mesh::createIndexBuffer()
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