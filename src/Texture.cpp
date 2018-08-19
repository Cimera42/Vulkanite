//
// Created by Tim on 2/11/2017.
//

#include "Texture.h"
#include "logger.h"
#include "vulkanInterface.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture::Texture(VulkanInterface *inVulkanInterface, std::vector<std::string> inFilenames, bool cube):
	vki(inVulkanInterface),
	filenames(std::move(inFilenames)),
	isCubemap(cube)
{
	loadImage();
	createTextureImageView(static_cast<uint32_t>(filenames.size()));
	createTextureSampler();
}

Texture::~Texture()
{
	vkDestroySampler(vki->logicalDevice, textureSampler, nullptr);
	Logger() << "Texture sampler destroyed";
	texture.destroy(vki->logicalDevice);
}

void Texture::loadImage()
{
	auto layerCount = static_cast<uint32_t>(filenames.size());
	std::vector<stbi_uc*> imageData(layerCount);
	std::vector<int> widths(layerCount);
	std::vector<int> heights(layerCount);
	std::vector<int> comps(layerCount);
	VkDeviceSize textureSize = 0;
	int maxWidth = 0, maxHeight = 0;
	for(int i = 0; i < layerCount; ++i)
	{
		imageData[i] = stbi_load(filenames[i].c_str(), &widths[i], &heights[i], &comps[i], STBI_rgb_alpha);

		if(!imageData[i])
		{
			Logger() << "Texture image failed to load";
			throw std::runtime_error("Failed to load texture image");
		}

		textureSize += widths[i] * heights[i] * 4;
		if(widths[i] > maxWidth) maxWidth = widths[i];
		if(heights[i] > maxHeight) maxHeight = heights[i];
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	vki->createBuffer(textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				 stagingBuffer, stagingBufferMemory);

	void* allImageData = malloc(textureSize);
	size_t memCopyOffset = 0;
	for(int i = 0; i < imageData.size(); ++i)
	{
		memcpy(reinterpret_cast<char*>(allImageData)+memCopyOffset, imageData[i], static_cast<size_t>(widths[i] * heights[i] * 4));
		memCopyOffset += widths[i] * heights[i] * 4;
	}
	void* data;
	vkMapMemory(vki->logicalDevice, stagingBufferMemory, 0, textureSize, 0, &data);
	memcpy(data, allImageData, static_cast<size_t>(textureSize));
	vkUnmapMemory(vki->logicalDevice, stagingBufferMemory);

	free(allImageData);

	for(auto &&item : imageData)
	{
		stbi_image_free(item);
	}

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	bufferCopyRegions.reserve(layerCount);
	size_t offset = 0;
	for(uint32_t i = 0; i < layerCount; ++i)
	{
		VkBufferImageCopy region = {};
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = i;
		region.imageSubresource.layerCount = 1;

		region.bufferOffset = offset;
		region.imageExtent = {
				static_cast<uint32_t>(widths[i]),
				static_cast<uint32_t>(heights[i]),
				1
		};
		bufferCopyRegions.emplace_back(region);

		offset += widths[i] * heights[i] * 4;
	}

	createImage(static_cast<uint32_t>(maxWidth), static_cast<uint32_t>(maxHeight), layerCount);

	vki->transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerCount);

	VkCommandBuffer copyCommandBuffer = vki->beginSingleTimeCommands();

	vkCmdCopyBufferToImage(copyCommandBuffer, stagingBuffer, texture.image,
	                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()),
	                       bufferCopyRegions.data());

	vki->endSingleTimeCommands(copyCommandBuffer);

	vki->transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerCount);

	vkDestroyBuffer(vki->logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(vki->logicalDevice, stagingBufferMemory, nullptr);
}

void Texture::createImage(uint32_t width, uint32_t height, uint32_t layers)
{
	VkImageCreateFlags flags = VK_NULL_HANDLE;
	if(isCubemap)
	{
		flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}
	vki->createImage(width, height, layers,
	                 VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
	                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.image, texture.imageMemory, flags);
}

void Texture::createTextureImageView(uint32_t layers)
{
	VkImageViewType viewType;
	if(layers == 1)
	{
		viewType = VK_IMAGE_VIEW_TYPE_2D;
	}
	else if(isCubemap)
	{
		viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	}
	else
	{
		viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}
	texture.imageView = createImageView(vki->logicalDevice, viewType, texture.image,
	                                    VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
	                                    layers);
}

void Texture::createTextureSampler()
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

	VK_RESULT_CHECK(vkCreateSampler(vki->logicalDevice, &samplerInfo, nullptr, &textureSampler))
	Logger() << "Texture sampler created";
}