//
// Created by Tim on 2/11/2017.
//

#include "Texture.h"
#include "logger.h"
#include "vulkanInterface.h"

Texture::Texture(VulkanInterface *inVulkanInterface, const std::string &inFilename):
	vki(inVulkanInterface),
	filename(inFilename)
{
	loadImage();
	createTextureImageView();
	createTextureSampler();
}

Texture::~Texture()
{
	vkDestroySampler(vki->logicalDevice, textureSampler, nullptr);
	Logger() << "Texture sampler destroyed";
	vkDestroyImageView(vki->logicalDevice, textureImageView, nullptr);
	Logger() << "Texture image view destroyed";
	vkDestroyImage(vki->logicalDevice, textureImage, nullptr);
	Logger() << "Texture image destroyed";
	vkFreeMemory(vki->logicalDevice, textureImageMemory, nullptr);
	Logger() << "Texture image memory freed";
}

void Texture::loadImage()
{
	imageData = stbi_load(filename.c_str(), &width, &height, &components, STBI_rgb_alpha);

	if(!imageData)
	{
		Logger() << "Texture image failed to load";
		throw std::runtime_error("Failed to load texture image");
	}
	Logger() << "Texture image loaded";

	auto textureSize = static_cast<VkDeviceSize>(width * height * 4);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	vki->createBuffer(textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(vki->logicalDevice, stagingBufferMemory, 0, textureSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(textureSize));
	vkUnmapMemory(vki->logicalDevice, stagingBufferMemory);

	stbi_image_free(imageData);

	vki->createImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
					 VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
					 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

	vki->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vki->copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	vki->transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(vki->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, stagingBufferMemory, nullptr);
}

void Texture::createTextureImageView()
{
	textureImageView = createImageView(vki->logicalDevice, textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Texture::createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if(vkCreateSampler(vki->logicalDevice, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		Logger() << "Texture sampler creation failed";
		throw std::runtime_error("Failed to create texture sampler");
	}
	Logger() << "Texture sampler created";
}