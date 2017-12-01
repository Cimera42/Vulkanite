//
// Created by Tim on 24/11/2017.
//

#ifndef VULKANITE_IMAGEATTACHMENT_H
#define VULKANITE_IMAGEATTACHMENT_H

struct ImageAttachment
{
	VkImage image;
	VkDeviceMemory imageMemory;
	VkImageView imageView;

	void destroy(VkDevice logicalDevice) {
		vkFreeMemory(logicalDevice, imageMemory, nullptr);
		vkDestroyImage(logicalDevice, image, nullptr);
		vkDestroyImageView(logicalDevice, imageView, nullptr);
	}
};

#endif //VULKANITE_IMAGEATTACHMENT_H
