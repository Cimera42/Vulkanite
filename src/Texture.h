//
// Created by Tim on 2/11/2017.
//

#ifndef VULKANITE_TEXTURE_H
#define VULKANITE_TEXTURE_H

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "ImageAttachment.h"

class VulkanInterface;

class Texture
{
	VulkanInterface* vki;

	bool isCubemap = false;
	std::vector<std::string> filenames;

	void loadImage();

	void createImage(uint32_t width, uint32_t height, uint32_t layers);
	void createTextureImageView(uint32_t layers);
	void createTextureSampler();

public:
	Texture(VulkanInterface* inVulkanInterface, std::vector<std::string> inFilenames, bool cube);
	~Texture();

	VkSampler textureSampler;

	ImageAttachment texture;
};

#endif //VULKANITE_TEXTURE_H
