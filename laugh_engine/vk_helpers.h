#pragma once

#include <fstream>
#include "VDeleter.h"


namespace rj
{
	namespace helper_functions
	{
		std::vector<char> readFile(const std::string& filename)
		{
			std::ifstream file(filename, std::ios::ate | std::ios::binary);

			if (!file.is_open())
			{
				throw std::runtime_error("failed to open file!");
			}

			size_t fileSize = (size_t)file.tellg();
			std::vector<char> buffer(fileSize);

			file.seekg(0);
			file.read(buffer.data(), fileSize);

			file.close();

			return buffer;
		}

		uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

			for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
			{
				if ((typeFilter & (1 << i)) &&
					(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}

			throw std::runtime_error("failed to find suitable memory type!");
		}

		void createShaderModule(VDeleter<VkShaderModule> &shaderModule, VkDevice device, const std::vector<char>& code)
		{
			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = code.size();
			createInfo.pCode = (uint32_t*)code.data();

			if (vkCreateShaderModule(device, &createInfo, nullptr, shaderModule.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create shader module!");
			}
		}

		// --- Image view creation ---
		void createImageView(
			VDeleter<VkImageView>& imageView, VkDevice device,
			VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1,
			VkComponentMapping componentMapping = {}, // identity mapping
			VkImageViewCreateFlags flags = 0)
		{
			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = image;
			viewInfo.viewType = viewType;
			viewInfo.format = format;
			viewInfo.components = componentMapping;
			viewInfo.subresourceRange.aspectMask = aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
			viewInfo.subresourceRange.levelCount = levelCount;
			viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
			viewInfo.subresourceRange.layerCount = layerCount;
			viewInfo.flags = flags;

			if (vkCreateImageView(device, &viewInfo, nullptr, imageView.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create texture image view!");
			}
		}
		// --- Image view creation ---

		// --- Image creation ---
		static void createImage(
			VDeleter<VkImage>& image, VDeleter<VkDeviceMemory>& imageMemory,
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkFormat format, VkImageType imageType, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkImageCreateFlags flags = 0,
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
			VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const std::vector<uint32_t> &queueFamilyIndices = {})
		{
			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = imageType;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = depth;
			imageInfo.mipLevels = mipLevels;
			imageInfo.arrayLayers = arrayLayers;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = initialLayout;
			imageInfo.usage = usage;
			imageInfo.samples = sampleCount;
			imageInfo.sharingMode = sharingMode;
			imageInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
			imageInfo.pQueueFamilyIndices = queueFamilyIndices.empty() ? nullptr : queueFamilyIndices.data();
			imageInfo.flags = flags;

			if (vkCreateImage(device, &imageInfo, nullptr, image.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image!");
			}

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, image, &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

			if (vkAllocateMemory(device, &allocInfo, nullptr, imageMemory.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate image memory!");
			}

			vkBindImageMemory(device, image, imageMemory, 0);
		}
		// --- Image creation ---
	}
}