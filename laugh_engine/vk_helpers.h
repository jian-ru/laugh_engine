#pragma once

#include <fstream>
#include "VDeleter.h"


namespace rj
{
	namespace helper_functions
	{
		struct FormatInfo
		{
			uint32_t blockSize;
			VkExtent3D blockExtent;
		};
		
		std::unordered_map<VkFormat, FormatInfo> g_formatInfoTable =
		{
			{ VK_FORMAT_R8G8B8A8_UNORM, { 4, { 1, 1, 1 } } },
			{ VK_FORMAT_R32G32_SFLOAT, { 8, { 1, 1, 1 } } },
			{ VK_FORMAT_R32G32B32A32_SFLOAT, { 16, { 1, 1, 1 } } },
			{ VK_FORMAT_BC3_UNORM_BLOCK, { 16, { 4, 4, 1 } } }
		};

		// A chunck of memory storing many uniforms
		template<size_t maxSizeInBytes>
		class UniformBlob
		{
		public:
			UniformBlob(VkDeviceSize alignmentInBytes = std::numeric_limits<VkDeviceSize>::max()) :
				memory(new char[maxSizeInBytes]),
				minAlignment(alignmentInBytes)
			{}

			virtual ~UniformBlob()
			{
				delete[] memory;
			}

			void setAlignment(VkDeviceSize alignmentInBytes)
			{
				minAlignment = alignmentInBytes;
			}

			void *alloc(size_t size)
			{
				assert(minAlignment != std::numeric_limits<VkDeviceSize>::max());

				if (size == 0) throw std::runtime_error("UniformBlob::alloc - size can't be zero.");

				size_t actualSize = (size / minAlignment) * minAlignment;
				actualSize = actualSize < size ? (actualSize + minAlignment) : actualSize;

				if (nextStartingByte + actualSize > maxSizeInBytes) throw std::runtime_error("UniformBlob::alloc - out of memory.");

				char *ret = &memory[nextStartingByte];
				nextStartingByte += actualSize;
				return ret;
			}

			size_t size() const
			{
				return currentSizeInBytes;
			}

			const char *operator&() const
			{
				return memory;
			}

			// @ptr should be a pointer to somewhere in @memory
			size_t offsetOf(const char *ptr) const
			{
				return ptr - memory;
			}

		private:
			char* memory;

			VkDeviceSize minAlignment;
			union
			{
				size_t nextStartingByte = 0;
				size_t currentSizeInBytes;
			};
		};

		template <class T>
		inline void hash_combine(std::size_t& seed, const T& v)
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}

		inline std::string getFileExtension(const std::string &fn)
		{
			size_t pos = fn.find_last_of('.');
			if (pos == std::string::npos) return ""; else return fn.substr(pos + 1);
		}

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

		bool hasStencilComponent(VkFormat format)
		{
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
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
		void createImage(
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

		// --- Image layout transition ---
		void recordImageLayoutTransitionCommands(VkCommandBuffer commandBuffer,
			VkImage image, VkFormat format,
			uint32_t baseLevel, uint32_t mipLevelCount, uint32_t baseLayer, uint32_t layerCount,
			VkImageLayout oldLayout, VkImageLayout newLayout)
		{
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = oldLayout;
			barrier.newLayout = newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				if (hasStencilComponent(format))
				{
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else
			{
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}
			barrier.subresourceRange.baseMipLevel = baseLevel;
			barrier.subresourceRange.levelCount = mipLevelCount;
			barrier.subresourceRange.baseArrayLayer = baseLayer;
			barrier.subresourceRange.layerCount = layerCount;

			// barrier.srcAccessMask - operations involving the resource that must happen before the barrier
			// barrier.dstAccessMask - operations involving the resource that must wait on the barrier
			if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			{
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
			{
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			}
			else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			{
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			}
			else
			{
				throw std::invalid_argument("unsupported layout transition!");
			}

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
		// --- Image layout transition ---

		// --- Buffer creation ---
		void createBuffer(VDeleter<VkBuffer>& buffer, VDeleter<VkDeviceMemory>& bufferMemory,
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBufferCreateFlags flags = 0,
			VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const std::vector<uint32_t> &queueFamilyIndices = {})
		{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = sharingMode;
			bufferInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
			bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();
			bufferInfo.flags = flags;

			if (vkCreateBuffer(device, &bufferInfo, nullptr, buffer.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create buffer!");
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

			if (vkAllocateMemory(device, &allocInfo, nullptr, bufferMemory.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate buffer memory!");
			}

			vkBindBufferMemory(device, buffer, bufferMemory, 0);
		}
		// --- Buffer creation ---

		// --- Data transfer ---

		// If depth > 1, levelCount and layerCount must be 1
		// Copy layer by layer. Within each layer, copy level by level.
		void recordCopyBufferToImageCommands(VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer, VkImage dstImage, VkFormat format, VkImageAspectFlags aspectMask,
			uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t levelCount = 1, uint32_t layerCount = 1)
		{
			assert(width > 0 && height > 0 && depth > 0);
			assert(depth == 1 || levelCount == 1 && layerCount == 1);

			const auto &formatInfo = g_formatInfoTable.at(format);
			const uint32_t blockSize = formatInfo.blockSize;
			const uint32_t blockWidth = formatInfo.blockExtent.width;
			const uint32_t blockHeight = formatInfo.blockExtent.height;
			const uint32_t blockDepth = formatInfo.blockExtent.depth;

			// Copy mip levels from staging buffer
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			uint32_t offset = 0;

			for (uint32_t layer = 0; layer < layerCount; layer++)
			{
				for (uint32_t level = 0; level < levelCount; level++)
				{
					uint32_t imgWidth = (width >> level);
					uint32_t imgHeight = (height >> level);
					uint32_t blockCountX = (imgWidth + (blockWidth - 1)) / blockWidth;
					uint32_t blockCountY = (imgHeight + (blockHeight - 1)) / blockHeight;
					uint32_t blockCountZ = (depth + (blockDepth - 1)) / blockDepth;

					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = aspectMask;
					bufferCopyRegion.imageSubresource.mipLevel = level;
					bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = imgWidth;
					bufferCopyRegion.imageExtent.height = imgHeight;
					bufferCopyRegion.imageExtent.depth = depth;
					bufferCopyRegion.bufferOffset = offset;

					bufferCopyRegions.push_back(bufferCopyRegion);

					offset += blockCountX * blockCountY * blockCountZ * blockSize;
				}
			}

			vkCmdCopyBufferToImage(
				commandBuffer,
				srcBuffer,
				dstImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()),
				bufferCopyRegions.data());
		}

		void recordCopyBufferToBufferCommands(VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize sizeInBytes,
			VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0)
		{
			VkBufferCopy copyRegion = {};
			copyRegion.srcOffset = srcOffset;
			copyRegion.dstOffset = dstOffset;
			copyRegion.size = sizeInBytes;
			vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		}
		// --- Data transfer ---
	}
}