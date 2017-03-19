#pragma once

#include <vector>
#include "VDeleter.h"


namespace rj
{
	namespace helper_functions
	{
		/**
		* Check if the validation layers in @validationLayers are supported
		*/
		bool checkValidationLayerSupport(const std::vector<const char *> &layerNames);

		VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugReportFlagsEXT flags,
			VkDebugReportObjectTypeEXT objType,
			uint64_t obj,
			size_t location,
			int32_t code,
			const char* layerPrefix,
			const char* msg,
			void* userData);

		VkResult createDebugReportCallbackEXT(
			VkInstance instance,
			const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
			const VkAllocationCallbacks *pAllocator,
			VkDebugReportCallbackEXT *pCallback);

		void destroyDebugReportCallbackEXT(
			VkInstance instance,
			VkDebugReportCallbackEXT callback,
			const VkAllocationCallbacks* pAllocator);
	}

	using namespace helper_functions;

	class VInstance
	{
	public:
		VInstance(bool enableValidationLayers,
			const std::vector<const char *> &layerNames,
			const std::vector<const char *> &extensionNames)
			:
			m_enableValidationLayers(enableValidationLayers),
			m_layerNames(layerNames),
			m_requiredExtensions(extensionNames)
		{
			if (m_enableValidationLayers)
			{
				m_requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			}

			createInstance();
			setupDebugCallback();
		}

		operator const VDeleter<VkInstance> &() const
		{
			return m_instance;
		}

	protected:
		void createInstance()
		{
			if (m_enableValidationLayers && !checkValidationLayerSupport(m_layerNames))
			{
				throw std::runtime_error("validation layers requested, but not available!");
			}

			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Vulkan App";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;

			createInfo.enabledExtensionCount = static_cast<uint32_t>(m_requiredExtensions.size());
			createInfo.ppEnabledExtensionNames = m_requiredExtensions.data();

			if (m_enableValidationLayers)
			{
				createInfo.enabledLayerCount = static_cast<uint32_t>(m_layerNames.size());
				createInfo.ppEnabledLayerNames = m_layerNames.data();
			}
			else
			{
				createInfo.enabledLayerCount = 0;
			}

			if (vkCreateInstance(&createInfo, nullptr, m_instance.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create instance!");
			}
		}

		void setupDebugCallback()
		{
			if (!m_enableValidationLayers) return;

			VkDebugReportCallbackCreateInfoEXT createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			createInfo.pfnCallback = debugCallback;

			if (createDebugReportCallbackEXT(m_instance, &createInfo, nullptr, m_debugReportCB.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to set up debug callback!");
			}
		}


		bool m_enableValidationLayers;
		std::vector<const char *> m_layerNames;
		std::vector<const char *> m_requiredExtensions;

		VDeleter<VkInstance> m_instance{ vkDestroyInstance };
		VDeleter<VkDebugReportCallbackEXT> m_debugReportCB{ m_instance, destroyDebugReportCallbackEXT };
	};
}