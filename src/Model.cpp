//
// Created by Tim on 2/11/2017.
//

#include "Model.h"
#include "logger.h"
#include "vulkanInterface.h"
#include <assimp/Importer.hpp>
#include <utility>
#include <assimp/postprocess.h>

Model::Model(VulkanInterface *inVulkanInterface, Mesh* inMesh) :
		vki(inVulkanInterface)
{
	mesh = inMesh;
	texture = nullptr;
}

Model::Model(VulkanInterface *inVulkanInterface, std::string filename) :
	vki(inVulkanInterface)
{
	load(std::move(filename));
}

Model::~Model()
{
	delete mesh;
	if(texture)
		delete texture;
}

void Model::draw(VkCommandBuffer commandBuffer)
{
	draw(commandBuffer, 1);
}

void Model::draw(VkCommandBuffer commandBuffer, uint32_t instanceCount)
{
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0,1, &mesh->vertexBuffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer, 0,VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh->indices.size()),
	                 instanceCount, 0, 0, 0);
}

void Model::load(std::string filename)
{
	Assimp::Importer importer;
	Logger(1) << "Loading model: " << filename;
	const aiScene* scene = importer.ReadFile(filename,
											 aiProcess_CalcTangentSpace
											 | aiProcess_Triangulate
											 | aiProcess_JoinIdenticalVertices
											 | aiProcess_SortByPType
											 | aiProcess_LimitBoneWeights
											 | aiProcess_ValidateDataStructure
											 | aiProcess_PreTransformVertices
	);

	if(!scene)
	{
		Logger(1) << importer.GetErrorString();
		Logger(1) << "Could not load Mesh. Error importing";
		throw std::runtime_error("Failed to load model file");
	}

//	Logger(1) << "Animations: " << scene->mNumAnimations;
//	for(unsigned int i = 0; i < scene->mNumAnimations; i++)
//	{
//		aiAnimation* assimpAnimation = scene->mAnimations[i];
//		Logger(1) << "Animation name: " << assimpAnimation->mName.C_Str();
//		Logger(1) << "Animation channels: " << assimpAnimation->mNumChannels;
//		Logger(1) << "TPS: " << assimpAnimation->mTicksPerSecond;
//
//		//Convert into own class layout
//		//Actually using vectors instead of array+size
//		Animation* anim = new Animation();
//		anim->name = assimpAnimation->mName.C_Str();
//		//One channel per node
//		anim->channels = assimpAnimation->mNumChannels;
//		//Framerate of animation basically
//		anim->tickRate = assimpAnimation->mTicksPerSecond;
//		//Duration of animation
//		anim->duration = assimpAnimation->mDuration;
//		for(unsigned int j = 0; j < assimpAnimation->mNumChannels; j++)
//		{
//			aiNodeAnim* an = assimpAnimation->mChannels[j];
//			anim->animNodes[an->mNodeName.C_Str()] = an;
//		}
//		//Add to map of animations
//		animations[anim->name] = anim;
//	}

	for(unsigned int i = 0; i < scene->mNumMaterials; i++)
	{
		aiMaterial* assimpMaterial = scene->mMaterials[i];

		aiString nnn;
		assimpMaterial->Get(AI_MATKEY_NAME, nnn);
		//Don't care about default material
		if(strcmp(nnn.C_Str(), AI_DEFAULT_MATERIAL_NAME) == 0)
			continue;

		aiString texPath;
		//Retrieve diffuse texture and load it
		if(assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			Logger() << texPath.C_Str();
			std::string backslashFixed = texPath.C_Str();
			std::replace(backslashFixed.begin(), backslashFixed.end(), '\\', '/');

			//Convert to correct folder
			std::string baseFolder = std::string(filename);
			baseFolder = baseFolder.substr(0, baseFolder.find_last_of("/"));
			//Load image
			texture = new Texture(vki, {std::string(baseFolder + "/" + texPath.C_Str())}, false);
		}
	}

	//Collate images for opengl texture array
//	std::vector<std::string> texPaths;
//	for(unsigned int i = 0; i < materials.size(); i++)
//	{
//		texPaths.push_back(materials[i].image->getName());
//	}

	//Load textures and place into singular OpenGL texture Array
//	if(texPaths.size() > 0)
//	{
//		ImageLoader* imgLoader = static_cast<ImageLoader*>(AssetManager::i()->getLoader("image"));
//		texture = imgLoader->loadTexture(texPaths);
//	}

	//Save root node
//	rootNodeName = scene->mRootNode->mName.C_Str();
	//Loop over all nodes recursively starting from root
//	nodeLoop(scene->mRootNode, 0, nullptr);

	//Load as specific mesh type depending on if boned or not
	for(unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* assimpMesh = scene->mMeshes[i];
		std::string meshName = assimpMesh->mName.C_Str();
//		if(assimpMesh->mNumBones > 0)
//		{
//			boneMeshes[i] = new BoneMesh(meshName, assimpMesh, assimpNodes);
//		}
//		else
		{
			mesh = new Mesh(vki);
			mesh->load(assimpMesh);
		}
	}
}