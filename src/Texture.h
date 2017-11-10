//
// Created by Tim on 2/11/2017.
//

#ifndef VULKANITE_TEXTURE_H
#define VULKANITE_TEXTURE_H

#include <vulkan/vulkan.h>
#include <stb_image.h>
#include <string>

class VulkanInterface;

class Texture
{
	VulkanInterface* vki;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

	std::string filename;
	stbi_uc* imageData;
	int width, height, components;

	void loadImage();

	void createTextureImageView();

	void createTextureSampler();

public:
	explicit Texture(VulkanInterface* inVulkanInterface, const std::string &inFilename);
	~Texture();

	VkImageView textureImageView;
	VkSampler textureSampler;
};

#endif //VULKANITE_TEXTURE_H
