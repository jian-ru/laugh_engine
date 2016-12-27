#pragma once

#include "VDevice.h"


namespace rj
{
	class VImage
	{
	public:
		VImage(const VDevice &device)
			:
			m_device(device),
			m_image{ m_device, vkDestroyImage },
			m_imageMemory{ m_device, vkFreeMemory }
		{}

		void initAs2DImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps,
			uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL)
		{
			createImage(m_image, m_imageMemory, m_device, m_device, format, VK_IMAGE_TYPE_2D, tiling, usage, memProps, width, height, 1,
				mipLevels, arrayLayers, 0, sampleCount, initialLayout);

			m_isCubeImage = false;
			m_extent = { width, height, 1 };
			m_mipLevelCount = mipLevels;
			m_arrayLayerCount = arrayLayers;
			m_sampleCount = sampleCount;
			m_format = format;
			m_type = VK_IMAGE_TYPE_2D;
			m_tiling = tiling;
			m_usage = usage;
			m_memoryProperties = memProps;
			m_curLayout = initialLayout;
		}

		void initAsCubeImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps,
			uint32_t mipLevels = 1, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL)
		{
			createImage(m_image, m_imageMemory, m_device, m_device, format, VK_IMAGE_TYPE_2D, tiling, usage, memProps, width, height, 1,
				mipLevels, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_SAMPLE_COUNT_1_BIT, initialLayout);

			m_isCubeImage = true;
			m_extent = { width, height, 1 };
			m_mipLevelCount = mipLevels;
			m_arrayLayerCount = 6;
			m_sampleCount = VK_SAMPLE_COUNT_1_BIT;
			m_format = format;
			m_type = VK_IMAGE_TYPE_2D;
			m_tiling = tiling;
			m_usage = usage;
			m_memoryProperties = memProps;
			m_curLayout = initialLayout;
		}

	protected:
		const VDevice &m_device;
		
		VDeleter<VkImage> m_image;
		VDeleter<VkDeviceMemory> m_imageMemory;

		bool m_isCubeImage;
		VkExtent3D m_extent;
		uint32_t m_mipLevelCount;
		uint32_t m_arrayLayerCount;
		VkSampleCountFlagBits m_sampleCount;
		VkFormat m_format;
		VkImageType m_type;
		VkImageTiling m_tiling;
		VkImageUsageFlags m_usage;
		VkMemoryPropertyFlags m_memoryProperties;
		VkImageLayout m_curLayout;
	};
}