#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <set>
#include <functional>
#include <string>

#include "gli/gli.hpp"
#include "gli/convert.hpp"
#include "gli/generate_mipmaps.hpp"

#include "vdeleter.h"


// A chunck of memory storing all uniforms
template<size_t maxSizeInBytes>
class AllUniformBlob
{
public:
	AllUniformBlob(const VkPhysicalDevice &pd) :
		memory(new char[maxSizeInBytes]), physicalDevice(pd)
	{}

	virtual ~AllUniformBlob()
	{
		delete[] memory;
	}

	void *alloc(size_t size)
	{
		if (minAlignment == std::numeric_limits<VkDeviceSize>::max())
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physicalDevice, &properties);
			minAlignment = properties.limits.minUniformBufferOffsetAlignment;
		}

		if (size == 0) throw std::runtime_error("AllUniformBlob::alloc - size can't be zero.");

		size_t actualSize = (size / minAlignment) * minAlignment;
		actualSize = actualSize < size ? (actualSize + minAlignment) : actualSize;

		if (nextStartingByte + actualSize > maxSizeInBytes) throw std::runtime_error("AllUniformBlob::alloc - out of memory.");

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

	VkDeviceSize minAlignment = std::numeric_limits<VkDeviceSize>::max();
	union
	{
		size_t nextStartingByte = 0;
		size_t currentSizeInBytes;
	};

	const VkPhysicalDevice &physicalDevice;
};

struct ShaderFileNames
{
	std::string vs;
	std::string fs;
	std::string cs;
	std::string gs;
	std::string hs;
	std::string ds;
};

struct BufferWrapper
{
	VDeleter<VkBuffer> buffer;
	VDeleter<VkDeviceMemory> bufferMemory;
	size_t numElements = 0;
	VkDeviceSize sizeInBytes = 0;

	BufferWrapper(const VDeleter<VkDevice> &device) :
		buffer{ device, vkDestroyBuffer },
		bufferMemory{ device, vkFreeMemory }
	{}

	VkDescriptorBufferInfo getDescriptorInfo(VkDeviceSize offset = 0, VkDeviceSize size = 0)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = size == 0? sizeInBytes : size;
		return bufferInfo;
	}
};

struct ImageWrapper
{
	VDeleter<VkImage> image;
	VDeleter<VkDeviceMemory> imageMemory;
	VDeleter<VkImageView> imageView;
	VDeleter<VkSampler> sampler;
	VkFormat format;

	ImageWrapper(const VDeleter<VkDevice> &device) :
		ImageWrapper(device, VK_FORMAT_UNDEFINED)
	{}

	ImageWrapper(const VDeleter<VkDevice> &device, VkFormat format) :
		image{ device, vkDestroyImage },
		imageMemory{ device, vkFreeMemory },
		imageView{ device, vkDestroyImageView },
		sampler{ device, vkDestroySampler },
		format{ format }
	{}

	VkDescriptorImageInfo getDescriptorInfo()
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageView = imageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.sampler = sampler;
		return imageInfo;
	}
};

typedef ImageWrapper BakedBRDF;

struct SwapChainWrapper
{
	VDeleter<VkSwapchainKHR> swapChain;
	std::vector<VkImage> swapChainImages; // swap chain images will be released when the swap chain is destroyed
	std::vector<VDeleter<VkImageView>> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	SwapChainWrapper(const VDeleter<VkDevice> &device)
		: swapChain{ device, vkDestroySwapchainKHR }
	{}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

/**
 * The queue that can present images onto a certain window surface may be different
 * from the queue that can run graphics commands. So we need multiply queue family
 * indices.
 */
struct QueueFamilyIndices
{
	int graphicsFamily = -1;
	int presentFamily = -1;
	int computeFamily = -1;
	int transferFamily = -1;

	void findQueueFamilies(
		VkPhysicalDevice device,
		VkSurfaceKHR surface,
		VkQueueFlagBits desiredFamilies = VK_QUEUE_GRAPHICS_BIT)
	{
		bool wantGraphics = desiredFamilies & VK_QUEUE_GRAPHICS_BIT;
		bool dedicatedCompute = desiredFamilies & VK_QUEUE_COMPUTE_BIT;
		bool dedicatedTransfer = desiredFamilies & VK_QUEUE_TRANSFER_BIT;

		clear();

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		if (wantGraphics)
		{
			for (int i = 0; i < queueFamilies.size(); ++i)
			{
				const auto &queueFamily = queueFamilies[i];

				if (queueFamily.queueCount == 0) continue;

				if (graphicsFamily < 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
				{
					graphicsFamily = i;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
				if (presentFamily < 0 && presentSupport)
				{
					presentFamily = i;
				}
			}
		}

		if (dedicatedCompute)
		{
			for (int i = 0; i < queueFamilies.size(); ++i)
			{
				const auto &queueFamily = queueFamilies[i];

				if (queueFamily.queueCount == 0) continue;

				if (computeFamily < 0 &&
					i != graphicsFamily && i != presentFamily &&
					(queueFamily.queueCount & VK_QUEUE_COMPUTE_BIT))
				{
					computeFamily = i;
					break;
				}
			}
		}
		else
		{
			// graphics family implicitly support compute and transfer
			computeFamily = graphicsFamily;
		}

		if (dedicatedTransfer)
		{
			for (int i = 0; i < queueFamilies.size(); ++i)
			{
				const auto &queueFamily = queueFamilies[i];

				if (queueFamily.queueCount == 0) continue;

				if (transferFamily < 0 &&
					i != graphicsFamily && i != presentFamily && i != computeFamily &&
					(queueFamily.queueCount & VK_QUEUE_TRANSFER_BIT))
				{
					transferFamily = i;
					break;
				}
			}
		}
		else
		{
			// both compute and graphics family implicitly support transfer
			transferFamily = graphicsFamily < 0 ? computeFamily : graphicsFamily;
		}

		if (wantGraphics && (graphicsFamily < 0 || presentFamily < 0))
		{
			throw std::runtime_error("findQueueFamilies: no suitable graphics or present queue were found.");
		}

		if (dedicatedCompute && computeFamily < 0)
		{
			throw std::runtime_error("findQueueFamilies: cannot find compute queue.");
		}

		if (dedicatedTransfer && transferFamily < 0)
		{
			throw std::runtime_error("findQueueFamilies: cannot find transfer queue.");
		}
	}

	void clear()
	{
		graphicsFamily = -1;
		presentFamily = -1;
		computeFamily = -1;
		transferFamily = -1;
	}

	// TODO: remove this
	bool isComplete()
	{
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

static void defaultViewportAndScissor(VkExtent2D swapChainExtent, VkViewport &viewport, VkRect2D &scissor)
{
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapChainExtent.width);
	viewport.height = static_cast<float>(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;
}

static VkResult createDebugReportCallbackEXT(
	VkInstance instance,
	const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
	const VkAllocationCallbacks *pAllocator,
	VkDebugReportCallbackEXT *pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void destroyDebugReportCallbackEXT(
	VkInstance instance,
	VkDebugReportCallbackEXT callback,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
	{
		func(instance, callback, pAllocator);
	}
}

/**
 * Check if the validation layers in @validationLayers are supported
 */
static bool checkValidationLayerSupport(const std::vector<const char *> &validationLayers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

/**
 * Check device specific extension support
 */
static bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char *> &deviceExtensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto &extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

static VkPhysicalDevice pickPhysicalDevice(VkInstance instance, std::function<bool (VkPhysicalDevice)> isDeviceSuitable)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto &device : devices)
	{
		if (isDeviceSuitable(device))
		{
			return device;
		}
	}

	throw std::runtime_error("failed to find a suitable GPU!");
}

static void createCommandPool(VDeleter<VkCommandPool> &commandPool, VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = flags; // Optional

	if (vkCreateCommandPool(device, &poolInfo, nullptr, commandPool.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

static void createShaderModule(VkDevice device, const std::vector<char>& code, VDeleter<VkShaderModule> &shaderModule)
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

struct DefaultGraphicsPipelineCreateInfo
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
	VkPipelineViewportStateCreateInfo viewportStateInfo;
	VkPipelineRasterizationStateCreateInfo rasterizerInfo;
	VkPipelineMultisampleStateCreateInfo multisamplingInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
	VkPipelineColorBlendStateCreateInfo colorBlendInfo;
	VkPipelineDynamicStateCreateInfo dynamicStateInfo;
	std::vector<VkPushConstantRange> pushConstantRanges;

	VkGraphicsPipelineCreateInfo pipelineInfo;

	VDeleter<VkShaderModule> vertShaderModule;
	VDeleter<VkShaderModule> fragShaderModule;

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	DefaultGraphicsPipelineCreateInfo(
		const VDeleter<VkDevice> &device,
		const ShaderFileNames &shaderFiles)
		:
		vertShaderModule{ device, vkDestroyShaderModule },
		fragShaderModule{ device, vkDestroyShaderModule }
	{
		pipelineLayoutInfo = {};
		vertexInputInfo = {};
		inputAssemblyInfo = {};
		viewportStateInfo = {};
		rasterizerInfo = {};
		multisamplingInfo = {};
		depthStencilInfo = {};
		colorBlendAttachmentStates = {};
		colorBlendInfo = {};
		dynamicStateInfo = {};
		pipelineInfo = {};

		auto fillShaderStageInfo = [&device, this](
			const std::string &fn,
			VkShaderStageFlagBits stage,
			VDeleter<VkShaderModule> &module)
		{
			auto shaderCode = readFile(fn.c_str());

			createShaderModule(device, shaderCode, module);

			VkPipelineShaderStageCreateInfo shaderStageInfo = {};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = stage;
			shaderStageInfo.module = module;
			shaderStageInfo.pName = "main";

			shaderStages.emplace_back(shaderStageInfo);
		};

		if (!shaderFiles.vs.empty())
		{
			fillShaderStageInfo(shaderFiles.vs, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);
		}
		if (!shaderFiles.fs.empty())
		{
			fillShaderStageInfo(shaderFiles.fs, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);
		}

		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.scissorCount = 1;

		rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth = 1.0f;
		rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

		multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilInfo.depthTestEnable = VK_TRUE;
		depthStencilInfo.depthWriteEnable = VK_TRUE;
		depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		colorBlendAttachmentStates.resize(1);
		colorBlendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentStates[0].blendEnable = VK_FALSE;

		colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendInfo.attachmentCount = 1;
		colorBlendInfo.pAttachments = colorBlendAttachmentStates.data();

		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizerInfo;
		pipelineInfo.pMultisampleState = &multisamplingInfo;
		pipelineInfo.pDepthStencilState = &depthStencilInfo;
		pipelineInfo.pColorBlendState = &colorBlendInfo;
		pipelineInfo.pDynamicState = nullptr;
	}
};

static void createCommandPool(VkDevice device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags, VDeleter<VkCommandPool> &commandPool)
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndex;
	poolInfo.flags = flags;

	if (vkCreateCommandPool(device, &poolInfo, nullptr, commandPool.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create command pool!");
	}
}

static void createCommandPool(VkDevice device, uint32_t queueFamilyIndex, VDeleter<VkCommandPool> &commandPool)
{
	createCommandPool(device, queueFamilyIndex, 0, commandPool);
}

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

static void createImage(
	VkPhysicalDevice physicalDevice, VkDevice device,
	uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	VDeleter<VkImage>& image, VDeleter<VkDeviceMemory>& imageMemory)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

static void createImage(
	VkPhysicalDevice physicalDevice, VkDevice device,
	uint32_t width, uint32_t height, VkFormat format,
	VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	VDeleter<VkImage>& image, VDeleter<VkDeviceMemory>& imageMemory)
{
	createImage(physicalDevice, device, width, height, 1, format, tiling, usage, properties, image, imageMemory);
}

static void createImageView2D(VkDevice device, 
	VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevelCount,
	VDeleter<VkImageView>& imageView)
{
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevelCount;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &viewInfo, nullptr, imageView.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture image view!");
	}
}

static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endSingleTimeCommands(VkDevice device, VkCommandBuffer commandBuffer, VkQueue submitQueue, VkCommandPool commandPool)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(submitQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(submitQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue submitQueue,
	VkImage image, VkFormat format, uint32_t mipLevelCount, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

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
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

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

	endSingleTimeCommands(device, commandBuffer, submitQueue, commandPool);
}

static void createBuffer(
	VkPhysicalDevice physicalDevice, VkDevice device,
	VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VDeleter<VkBuffer>& buffer, VDeleter<VkDeviceMemory>& bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

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

/**
 * Copy a staging buffer @srcBuffer that contains the pixel data of all
 * mip levels of @srcTex to a precreated device-local image @dstImage
 */
static void copyBufferToImage(
	VkDevice device, VkCommandPool commandPool, VkQueue submitQueue,
	const gli::texture2d &srcTex, VkBuffer srcBuffer, VkImage dstImage)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	// Copy mip levels from staging buffer
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	uint32_t offset = 0;

	for (uint32_t i = 0; i < srcTex.levels(); i++)
	{
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = i;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(srcTex[i].extent().x);
		bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(srcTex[i].extent().y);
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = offset;

		bufferCopyRegions.push_back(bufferCopyRegion);

		offset += static_cast<uint32_t>(srcTex[i].size());
	}

	vkCmdCopyBufferToImage(
		commandBuffer,
		srcBuffer,
		dstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	endSingleTimeCommands(device, commandBuffer, submitQueue, commandPool);
}

static void copyBufferToImage(
	VkDevice device, VkCommandPool commandPool, VkQueue submitQueue,
	const std::vector<VkBufferImageCopy> &bufferCopyRegions, VkBuffer srcBuffer, VkImage dstImage)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	vkCmdCopyBufferToImage(
		commandBuffer,
		srcBuffer,
		dstImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	endSingleTimeCommands(device, commandBuffer, submitQueue, commandPool);
}

static void copyBufferToBuffer(
	VkDevice device, VkCommandPool commandPool, VkQueue submitQueue,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(device, commandBuffer, submitQueue, commandPool);
}

static void getDefaultSamplerCreateInfo(VkSamplerCreateInfo &samplerInfo)
{
	samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 8;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
}

static void loadTexture(
	VkPhysicalDevice physicalDevice, const VDeleter<VkDevice> &device,
	VkCommandPool commandPool, VkQueue submitQueue,
	const std::string &fn, ImageWrapper &texture)
{
	std::string ext = getFileExtension(fn);
	if (ext != "ktx" && ext != "dds")
	{
		throw std::runtime_error("texture type ." + ext + " is not supported.");
	}


	gli::texture2d textureSrc(gli::load(fn.c_str()));

	if (textureSrc.empty())
	{
		throw std::runtime_error("cannot load texture.");
	}

	gli::texture2d textureMipmapped;
	if (textureSrc.levels() == 1)
	{
		textureMipmapped = gli::generate_mipmaps(textureSrc, gli::FILTER_LINEAR);
	}
	else
	{
		textureMipmapped = textureSrc;
	}

	VkFormat format;
	switch (textureMipmapped.format())
	{
	case gli::FORMAT_RGBA8_UNORM_PACK8:
		format = VK_FORMAT_R8G8B8A8_UNORM;
		break;
	case gli::FORMAT_RGBA32_SFLOAT_PACK32:
		format = VK_FORMAT_R32G32B32A32_SFLOAT;
		break;
	case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
		format = VK_FORMAT_BC3_UNORM_BLOCK;
		break;
	default:
		throw std::runtime_error("texture format is not supported.");
	}

	int width = textureMipmapped.extent().x;
	int height = textureMipmapped.extent().y;
	int mipLevels = textureMipmapped.levels();
	int sizeInBytes = textureMipmapped.size();

	BufferWrapper stagingBuffer{ device };

	createBuffer(physicalDevice, device, sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer.buffer, stagingBuffer.bufferMemory);

	void* data;
	vkMapMemory(device, stagingBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
	memcpy(data, textureMipmapped.data(), (size_t)sizeInBytes);
	vkUnmapMemory(device, stagingBuffer.bufferMemory);

	texture.format = format;

	createImage(
		physicalDevice, device,
		width, height, mipLevels,
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture.image,
		texture.imageMemory);

	transitionImageLayout(
		device, commandPool, submitQueue,
		texture.image, format, mipLevels,
		VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyBufferToImage(device, commandPool, submitQueue, textureMipmapped, stagingBuffer.buffer, texture.image);
	
	transitionImageLayout(
		device, commandPool, submitQueue,
		texture.image, format, mipLevels,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	createImageView2D(device, texture.image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, texture.imageView);

	VkSamplerCreateInfo samplerInfo = {};
	getDefaultSamplerCreateInfo(samplerInfo);

	if (vkCreateSampler(device, &samplerInfo, nullptr, texture.sampler.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create texture sampler!");
	}
}

static void createBufferFromHostData(
	VkPhysicalDevice physicalDevice, const VDeleter<VkDevice> &device,
	VkCommandPool commandPool, VkQueue submitQueue,
	const void *hostData, uint32_t numElements, size_t sizeInBytes,
	VkBufferUsageFlags usage,
	BufferWrapper &retBuffer)
{
	BufferWrapper stagingBuffer{ device };

	retBuffer.sizeInBytes = sizeInBytes;
	retBuffer.numElements = numElements;

	createBuffer(
		physicalDevice, device,
		sizeInBytes,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer.buffer, stagingBuffer.bufferMemory);

	void* data;
	vkMapMemory(device, stagingBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
	memcpy(data, hostData, sizeInBytes);
	vkUnmapMemory(device, stagingBuffer.bufferMemory);

	createBuffer(
		physicalDevice, device,
		sizeInBytes,
		usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		retBuffer.buffer, retBuffer.bufferMemory);

	copyBufferToBuffer(
		device, commandPool, submitQueue,
		stagingBuffer.buffer, retBuffer.buffer, sizeInBytes);
}

static void createStagingBuffer(
	VkPhysicalDevice physicalDevice, const VDeleter<VkDevice> &device, 
	const void *hostData, uint32_t numElements, size_t sizeInBytes, 
	BufferWrapper &retBuffer)
{
	retBuffer.sizeInBytes = sizeInBytes;
	retBuffer.numElements = numElements;

	createBuffer(
		physicalDevice, device,
		sizeInBytes,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		retBuffer.buffer, retBuffer.bufferMemory);

	if (hostData)
	{
		void* data;
		vkMapMemory(device, retBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
		memcpy(data, hostData, sizeInBytes);
		vkUnmapMemory(device, retBuffer.bufferMemory);
	}
}