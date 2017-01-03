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

		operator VkImage() const { return m_image; }

		void setLayout(VkImageLayout layout)
		{
			m_curLayout = layout;
		}

		// --- Geters ---
		bool isCubeImage() const { return m_isCubeImage; }

		VkExtent3D extent(uint32_t level = 0) const
		{
			assert(level < m_mipLevelCount);
			assert(m_extent.depth == 1 || level == 0);

			return { (m_extent.width >> level), (m_extent.height >> level), m_extent.depth };
		}
		
		uint32_t levels() const { return m_mipLevelCount; }
		uint32_t layers() const { return m_arrayLayerCount; }
		VkSampleCountFlagBits sampleCount() const { return m_sampleCount; }
		VkFormat format() const { return m_format; }
		VkImageType type() const { return m_type; }
		VkImageTiling tiling() const { return m_tiling; }
		VkImageUsageFlags usage() const { return m_usage; }
		VkMemoryPropertyFlags memoryProperties() const { return m_memoryProperties; }
		VkImageLayout layout() const { return m_curLayout; }
		// --- Geters ---

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

	class VImageView
	{
	public:
		VImageView(const VDevice &device, const VImage &image)
			:
			m_device(device),
			m_image(image),
			m_imageView{ m_device, vkDestroyImageView }
		{}

		void init(VkImageViewType viewType, VkImageAspectFlags aspectFlags,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1,
			VkComponentMapping componentMapping = {}, VkImageViewCreateFlags flags = 0)
		{
			init(viewType, m_image.format(), aspectFlags, baseMipLevel, levelCount, baseArrayLayer, layerCount, componentMapping, flags);
		}

		// Mutable format feature need to be enabled if the view has different format from the image
		// and view's format needs to be compatible
		void init(VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1,
			VkComponentMapping componentMapping = {}, VkImageViewCreateFlags flags = 0)
		{
			VkImageViewCreateInfo viewInfo = {};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_image;
			viewInfo.viewType = viewType;
			viewInfo.format = format;
			viewInfo.components = componentMapping;
			viewInfo.subresourceRange.aspectMask = aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
			viewInfo.subresourceRange.levelCount = levelCount;
			viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
			viewInfo.subresourceRange.layerCount = layerCount;
			viewInfo.flags = flags;

			if (vkCreateImageView(m_device, &viewInfo, nullptr, m_imageView.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create texture image view!");
			}

			m_type = viewType;
			m_format = format;
			m_componentMapping = componentMapping;
			m_aspectMask = aspectFlags;
			m_baseMipLevel = baseMipLevel;
			m_levelCount = levelCount;
			m_baseArrayLayer = baseArrayLayer;
			m_layerCount = layerCount;
		}

		operator VkImageView() const { return m_imageView; }

		// --- Geters ---
		VkImageViewType type() const { return m_type; }
		VkFormat format() const { return m_format; }
		VkComponentMapping componentMapping() const { return m_componentMapping; }
		VkImageAspectFlags aspectMask() const { return m_aspectMask; }
		uint32_t baseLevel() const { return m_baseMipLevel; }
		uint32_t levels() const { return m_levelCount; }
		uint32_t baseLayer() const { return m_baseArrayLayer; }
		uint32_t layers() const { return m_layerCount; }
		const VImage *image() const { return &m_image; }
		// --- Geters ---

	protected:
		const VDevice &m_device;
		const VImage &m_image;

		VDeleter<VkImageView> m_imageView;

		VkImageViewType m_type;
		VkFormat m_format;
		VkComponentMapping m_componentMapping;
		VkImageAspectFlags m_aspectMask;
		uint32_t m_baseMipLevel;
		uint32_t m_levelCount;
		uint32_t m_baseArrayLayer;
		uint32_t m_layerCount;
	};
}