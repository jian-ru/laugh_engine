#pragma once

#include <vector>
#include <set>

#include "VDeleter.h"
#include "VQueueFamilyIndices.h"
#include "VSwapChain.h"


namespace rj
{
	namespace helper_functions
	{
		/**
		* Check device specific extension support
		*/
		bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char *> &deviceExtensions)
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
	}

	using namespace helper_functions;

	class VDevice
	{
	public:
		VDevice(bool enableValidationLayers,
			const std::vector<const char *> &layerNames,
			const VDeleter<VkInstance> &instance,
			const VDeleter<VkSurfaceKHR> &surface,
			const std::vector<const char *> &deviceExtensions,
			const VkPhysicalDeviceFeatures &enabledFeatures = {})
			:
			m_enableValidationLayers(enableValidationLayers), m_validationLayers(layerNames),
			m_instance(instance), m_surface(surface),
			m_deviceExtensions(deviceExtensions), m_enabledDeviceFeatures(enabledFeatures)
		{
			pickPhysicalDevice();
			createLogicalDevice();
		}

		operator VkPhysicalDevice() const
		{
			return m_physicalDevice;
		}

		operator VkDevice() const
		{
			return static_cast<VkDevice>(m_device);
		}

		operator const VDeleter<VkDevice> &() const
		{
			return m_device;
		}

		const VQueueFamilyIndices &getQueueFamilyIndices() const
		{
			return m_queueFamilyIndices;
		}

	protected:
		void pickPhysicalDevice()
		{
			uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

			if (deviceCount == 0)
			{
				throw std::runtime_error("failed to find GPUs with Vulkan support!");
			}

			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

			for (auto device : devices)
			{
				if (isDeviceSuitable(device))
				{
					m_physicalDevice = device;
					return;
				}
			}

			throw std::runtime_error("failed to find a suitable GPU!");
		}

		bool isDeviceSuitable(VkPhysicalDevice physicalDevice)
		{
			m_queueFamilyIndices.clear();
			m_queueFamilyIndices.setPhysicalDevice(physicalDevice);
			m_queueFamilyIndices.findQueueFamilies(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

			bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice, m_deviceExtensions);

			bool swapChainAdequate = false;
			if (extensionsSupported)
			{
				SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, m_surface);
				swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
			}

			auto isQueueFamilyIndicesComplete = [this]()
			{
				return m_queueFamilyIndices.graphicsFamily >= 0 &&
					m_queueFamilyIndices.presentFamily >= 0 &&
					m_queueFamilyIndices.computeFamily >= 0;
			};

			return isQueueFamilyIndicesComplete() && extensionsSupported && swapChainAdequate;
		}
		
		void createLogicalDevice()
		{
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<int> uniqueQueueFamilies =
			{
				m_queueFamilyIndices.graphicsFamily,
				m_queueFamilyIndices.computeFamily,
				m_queueFamilyIndices.presentFamily
			};

			float queuePriority = 1.0f;
			for (int queueFamily : uniqueQueueFamilies)
			{
				VkDeviceQueueCreateInfo queueCreateInfo = {};
				queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;
				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
			createInfo.pEnabledFeatures = &m_enabledDeviceFeatures;

			createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
			createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

			if (m_enableValidationLayers)
			{
				createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
				createInfo.ppEnabledLayerNames = m_validationLayers.data();
			}
			else
			{
				createInfo.enabledLayerCount = 0;
			}

			if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, m_device.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create logical device!");
			}

			vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.computeFamily, 0, &m_computeQueue);
		}


		bool m_enableValidationLayers;
		std::vector<const char *> m_validationLayers;
		const VDeleter<VkInstance> &m_instance;
		const VDeleter<VkSurfaceKHR> &m_surface;
		std::vector<const char *> m_deviceExtensions;
		VkPhysicalDeviceFeatures m_enabledDeviceFeatures;

		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE; // implicitly destroyed when the instance is destroyed
		VDeleter<VkDevice> m_device{ vkDestroyDevice }; // support only one logical device right now
		
		VQueueFamilyIndices m_queueFamilyIndices{ VK_NULL_HANDLE, m_surface };

		VkQueue m_graphicsQueue = VK_NULL_HANDLE;
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		VkQueue m_computeQueue = VK_NULL_HANDLE;
	};
}