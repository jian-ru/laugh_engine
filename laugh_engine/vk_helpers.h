#pragma once

#include <fstream>
#include <unordered_map>
#include <chrono>

#include "gli/gli.hpp"
#include "gli/convert.hpp"
#include "gli/generate_mipmaps.hpp"

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
		
		extern std::unordered_map<VkFormat, FormatInfo> g_formatInfoTable;

		struct ImageWrapper
		{
			uint32_t image;
			std::vector<uint32_t> imageViews;
			std::vector<uint32_t> samplers;

			VkFormat format;
			uint32_t width, height, depth = 1;
			uint32_t mipLevelCount = 1;
			uint32_t layerCount = 1;
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
		};

		struct BufferWrapper
		{
			uint32_t buffer;
			VkDeviceSize offset;
			VkDeviceSize size;
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

		class Timer
		{
		public:
			Timer(uint32_t ac = 100) :
				tStart(std::chrono::high_resolution_clock::now()),
				tEnd(tStart),
				averageCount{ac},
				timeIntervals(ac, 0.f)
			{}
		
			void start()
			{
				tStart = std::chrono::high_resolution_clock::now();
			}
		
			void stop()
			{
				tEnd = std::chrono::high_resolution_clock::now();
				float te = timeElapsed();
				total -= timeIntervals[nextIdx];
				timeIntervals[nextIdx] = te;
				total += te;
				nextIdx = (nextIdx + 1) % averageCount;
			}
		
			float timeElapsed() const
			{
				return std::chrono::duration<float, std::milli>(tEnd - tStart).count();
			}
		
			float getAverageTime() const
			{
				return total / averageCount;
			}
		
		private:
			std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
			decltype(tStart) tEnd;
		
			uint32_t averageCount;
			uint32_t nextIdx = 0;
			float total = 0.f;
			std::vector<float> timeIntervals;
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

		inline std::string getBaseDir(const std::string &fn)
		{
			auto pos = fn.find_last_of('/');
			return pos == std::string::npos ? "" : fn.substr(0, pos);
		}

		std::vector<char> readFile(const std::string& filename);

		inline bool fileExist(const std::string &fn)
		{
			std::ifstream ifs(fn);
			return ifs.good();
		}

		uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

		VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

		inline bool hasStencilComponent(VkFormat format)
		{
			return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
		}

		size_t compute2DImageSizeInBytes(uint32_t width, uint32_t height, uint32_t pixelSizeInBytes, uint32_t mipLevelCount, uint32_t layerCount);

		// TODO: support compressed format
		// Address computation: layerIdx * layerSize + faceIdx * faceSize + levelSize(0) + ... + levelSize(levelIdx - 1)
		// layerCount is always 1 for 2D image (you need a texture2d_array to use layers)
		// faceCount is always 1 for 2D image
		void saveImage2D(
			const std::string &fileName,
			uint32_t width, uint32_t height, uint32_t bytesPerPixel,
			uint32_t mipLevels, gli::format format,
			const void *pixelData);

		void saveImageCube(
			const std::string &fileName,
			uint32_t width, uint32_t height, uint32_t bytesPerPixel,
			uint32_t mipLevels, gli::format format,
			const void *pixelData);

		// --- Shader module creation ---
		void createShaderModule(VDeleter<VkShaderModule> &shaderModule, VkDevice device, const std::vector<char>& code);
		// --- Shader module creation ---

		// --- Image view creation ---
		void createImageView(
			VDeleter<VkImageView>& imageView, VkDevice device,
			VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectFlags,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1,
			VkComponentMapping componentMapping = {}, // identity mapping
			VkImageViewCreateFlags flags = 0);
		// --- Image view creation ---

		// --- Image creation ---
		void createImage(
			VDeleter<VkImage>& image, VDeleter<VkDeviceMemory>& imageMemory,
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkFormat format, VkImageType imageType, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
			uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkImageCreateFlags flags = 0,
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
			VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const std::vector<uint32_t> &queueFamilyIndices = {});
		// --- Image creation ---

		// --- Image layout transition ---
		void recordImageLayoutTransitionCommands(VkCommandBuffer commandBuffer,
			VkImage image, VkFormat format,
			uint32_t baseLevel, uint32_t mipLevelCount, uint32_t baseLayer, uint32_t layerCount,
			VkImageLayout oldLayout, VkImageLayout newLayout);
		// --- Image layout transition ---

		// --- Buffer creation ---
		void createBuffer(VDeleter<VkBuffer>& buffer, VDeleter<VkDeviceMemory>& bufferMemory,
			VkPhysicalDevice physicalDevice, VkDevice device,
			VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBufferCreateFlags flags = 0,
			VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const std::vector<uint32_t> &queueFamilyIndices = {});
		// --- Buffer creation ---

		// --- Swap chain support ---
		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		// --- Swap chain support ---

		// --- Data transfer ---

		// If depth > 1, levelCount and layerCount must be 1
		// Copy layer by layer. Within each layer, copy level by level.
		void recordCopyBufferToImageCommands(VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer, VkImage dstImage, VkFormat format, VkImageAspectFlags aspectMask,
			uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t levelCount = 1, uint32_t layerCount = 1);

		void recordCopyBufferToBufferCommands(VkCommandBuffer commandBuffer,
			VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize sizeInBytes,
			VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
		// --- Data transfer ---
	}
}