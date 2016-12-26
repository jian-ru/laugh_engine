#pragma once

#include <vulkan/vulkan.h>
#include <exception>
#include <vector>


namespace rj
{
	/**
	* The queue that can present images onto a certain window surface may be different
	* from the queue that can run graphics commands. So we need multiple queue family
	* indices.
	*/
	class VQueueFamilyIndices
	{
	public:
		int graphicsFamily = -1;
		int presentFamily = -1;
		int computeFamily = -1;
		int transferFamily = -1;

		VQueueFamilyIndices(
			VkPhysicalDevice physicalDevice,
			const VDeleter<VkSurfaceKHR> &surface) :
			m_physicalDevice(physicalDevice),
			m_surface(surface)
		{}

		void findQueueFamilies(VkQueueFlags desiredFamilies = VK_QUEUE_GRAPHICS_BIT)
		{
			bool wantGraphics = desiredFamilies & VK_QUEUE_GRAPHICS_BIT;
			bool dedicatedCompute = desiredFamilies & VK_QUEUE_COMPUTE_BIT;
			bool dedicatedTransfer = desiredFamilies & VK_QUEUE_TRANSFER_BIT;

			VkPhysicalDevice device = m_physicalDevice;
			VkSurfaceKHR surface = m_surface;

			clear();

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

			if (wantGraphics)
			{
				for (uint32_t i = 0; i < queueFamilyCount; ++i)
				{
					const auto &queueFamily = queueFamilies[i];

					if (queueFamily.queueCount > 0 && graphicsFamily < 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
					{
						graphicsFamily = i;
						break;
					}
				}

				for (uint32_t i = 0; i < queueFamilyCount; ++i)
				{
					const auto &queueFamily = queueFamilies[i];

					VkBool32 presentSupport = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
					if (queueFamily.queueCount > 0 && presentFamily < 0 && presentSupport)
					{
						presentFamily = i;
						break;
					}
				}
			}

			if (dedicatedCompute)
			{
				for (int i = 0; i < queueFamilies.size(); ++i)
				{
					const auto &queueFamily = queueFamilies[i];

					if (queueFamily.queueCount > 0 &&
						computeFamily < 0 &&
						i != graphicsFamily && i != presentFamily &&
						(queueFamily.queueCount & VK_QUEUE_COMPUTE_BIT))
					{
						computeFamily = i;
						break;
					}
				}

				// fall back to using graphcis queue if no dedicated comptue queue is available
				if (computeFamily < 0)
				{
					computeFamily = graphicsFamily;
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

					if (queueFamily.queueCount > 0 &&
						transferFamily < 0 &&
						i != graphicsFamily && i != presentFamily && i != computeFamily &&
						(queueFamily.queueCount & VK_QUEUE_TRANSFER_BIT))
					{
						transferFamily = i;
						break;
					}
				}

				if (transferFamily < 0)
				{
					transferFamily = graphicsFamily < 0 ? computeFamily : graphicsFamily;
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

		void setPhysicalDevice(VkPhysicalDevice pd)
		{
			m_physicalDevice = pd;
		}

	protected:
		VkPhysicalDevice m_physicalDevice;
		const VDeleter<VkSurfaceKHR> &m_surface;
	};
}