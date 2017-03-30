#pragma once

#include <vector>
#include <algorithm>
#include "VDevice.h"
#include "VWindow.h"
#include "vk_helpers.h"


namespace rj
{
	class VSwapChain
	{
	public:
		VSwapChain(const VDevice &device, const VWindow &window)
			: m_device(device), m_window(window)
		{
			recreateSwapChain();
		}

		void recreateSwapChain()
		{
			createSwapChain();
			createSwapChainImageViews();
		}

		operator VkSwapchainKHR() const { return m_swapChain; }

		const std::vector<VDeleter<VkImageView>> &imageViews() const
		{
			return m_swapChainImageViews;
		}

		VkExtent2D extent() const
		{
			return m_swapChainExtent;
		}

		VkFormat format() const
		{
			return m_swapChainImageFormat;
		}

		// Return number of swap chain images
		uint32_t size() const
		{
			return static_cast<uint32_t>(m_swapChainImages.size());
		}

	protected:
		void createSwapChain()
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_device, m_window);

			VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
			VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
			VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

			uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
			if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
			{
				imageCount = swapChainSupport.capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = m_window;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1; // always 1 for non-stereoscopic 3D applications
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			const VQueueFamilyIndices &indices = m_device.getQueueFamilyIndices();
			uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

			if (indices.graphicsFamily != indices.presentFamily)
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndices;
			}
			else
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0; // Optional
				createInfo.pQueueFamilyIndices = nullptr; // Optional
			}

			createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_TRUE;

			VkSwapchainKHR oldSwapChain = m_swapChain;
			createInfo.oldSwapchain = oldSwapChain;

			VkSwapchainKHR newSwapChain;
			if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &newSwapChain) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create swap chain!");
			}

			m_swapChain = newSwapChain;

			vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
			m_swapChainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

			m_swapChainImageFormat = surfaceFormat.format;
			m_swapChainExtent = extent;
		}

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
		{
			if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
			{
				return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
			}

			for (const auto& availableFormat : availableFormats)
			{
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return availableFormat;
				}
			}

			return availableFormats[0];
		}

		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
		{
			for (const auto& availablePresentMode : availablePresentModes)
			{
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					return availablePresentMode;
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR; // this format is always supported
		}

		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
		{
			// swap chain extent should almost always equal to the currentExtent obtained from the window
			// surface capabilities (resolution of the window) but some window managers allow us to vary by
			// setting currentExtent to std::numeric_limits<uint32_t>::max()
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			{
				return capabilities.currentExtent;
			}
			else
			{
				uint32_t width, height;
				m_window.getExtent(&width, &height);
				VkExtent2D actualExtent = { width, height };

				actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
				actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

				return actualExtent;
			}
		}

		void createSwapChainImageViews()
		{
			m_swapChainImageViews.resize(m_swapChainImages.size(), VDeleter<VkImageView>{ m_device, vkDestroyImageView });

			for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
			{
				createImageView(m_swapChainImageViews[i], m_device, m_swapChainImages[i], VK_IMAGE_VIEW_TYPE_2D, m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}


		const VDevice &m_device;
		const VWindow &m_window;

		VDeleter<VkSwapchainKHR> m_swapChain{ m_device, vkDestroySwapchainKHR };
		std::vector<VkImage> m_swapChainImages; // swap chain images will be released when the swap chain is destroyed
		std::vector<VDeleter<VkImageView>> m_swapChainImageViews;
		VkFormat m_swapChainImageFormat;
		VkExtent2D m_swapChainExtent;
	};
}