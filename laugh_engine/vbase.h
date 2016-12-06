#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/hash.hpp"

#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "common_utils.h"
#include "vdeleter.h"
#include "vutils.h"
#include "camera.h"
#include "vtextoverlay.h"
#include "vmesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


enum DisplayMode
{
	DISPLAY_MODE_FULL = 0,
	DISPLAY_MODE_ALBEDO,
	DISPLAY_MODE_EYE_NORMAL,
	DISPLAY_MODE_EYE_POSITION,
	DISPLAY_MODE_DEPTH,
	DISPLAY_MODE_ROUGHNESS,
	DISPLAY_MODE_METALNESS,
	DISPLAY_MODE_COUNT
};

class VBaseGraphics
{
public:
	uint32_t m_verNumMajor;
	uint32_t m_verNumMinor;

	uint32_t m_width = 1280;
	uint32_t m_height = 720;
	std::string m_windowTitle = "VBaseGraphics";

	DisplayMode m_displayMode = DISPLAY_MODE_FULL;

	static bool leftMBDown, middleMBDown;
	static float lastX, lastY;

	Camera m_camera{ glm::vec3(0.f, 1.f, 3.f), glm::vec3(0.f, 0.f, 0.f), glm::radians(45.f), float(m_width) / m_height, .1f, 100.f };

#ifdef NDEBUG
	bool enableValidationLayers = false;
#else
	bool enableValidationLayers = true;
#endif
	const std::vector<const char*> m_validationLayers{ { "VK_LAYER_LUNARG_standard_validation" } };

	// Device specific extensions
	const std::vector<const char*> m_deviceExtensions{ { VK_KHR_SWAPCHAIN_EXTENSION_NAME } };


	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData)
	{
		std::cerr << "validation layer: " << msg << std::endl;
		return VK_FALSE;
	}

	static void onWindowResized(GLFWwindow* window, int width, int height)
	{
		if (width == 0 || height == 0) return; // window is minimized

		VBaseGraphics *app = reinterpret_cast<VBaseGraphics *>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
	}

	static void mouseButtonCB(GLFWwindow* window, int button, int action, int mods)
	{
		auto recordCurrentCursorPos = [window]()
		{
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			lastX = static_cast<float>(xpos);
			lastY = static_cast<float>(ypos);
		};

		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !middleMBDown)
		{
			leftMBDown = true;
			recordCurrentCursorPos();
		}
		else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
		{
			leftMBDown = false;
		}
		else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS && !leftMBDown)
		{
			middleMBDown = true;
			recordCurrentCursorPos();
		}
		else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
		{
			middleMBDown = false;
		}
	}

	static void cursorPositionCB(GLFWwindow* window, double xpos, double ypos)
	{
		if (leftMBDown || middleMBDown)
		{
			VBaseGraphics *app = reinterpret_cast<VBaseGraphics *>(glfwGetWindowUserPointer(window));

			const float rotScale = .01f;
			const float panScale = .002f;

			float dx = xpos - lastX;
			float dy = ypos - lastY;
			lastX = xpos;
			lastY = ypos;
			
			if (leftMBDown)
			{
				app->m_camera.addRotation(-dx * rotScale, -dy * rotScale);
			}
			else
			{
				app->m_camera.addPan(-dx * panScale, dy * panScale);
			}
		}
	}

	static void scrollCB(GLFWwindow *window, double xoffset, double yoffset)
	{
		VBaseGraphics *app = reinterpret_cast<VBaseGraphics *>(glfwGetWindowUserPointer(window));
		const float scale = .2f;
		app->m_camera.addZoom(scale * yoffset);
	}

	static void keyCB(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		VBaseGraphics *app = reinterpret_cast<VBaseGraphics *>(glfwGetWindowUserPointer(window));

		if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
		{
			app->m_displayMode = static_cast<DisplayMode>((app->m_displayMode + 1) % DISPLAY_MODE_COUNT);
		}
	}

	virtual void run();

protected:
	VDeleter<VkInstance> m_instance{ vkDestroyInstance };
	VDeleter<VkDebugReportCallbackEXT> m_debugReportCB{ m_instance, destroyDebugReportCallbackEXT };
	
	GLFWwindow *m_window = nullptr;
	VDeleter<VkSurfaceKHR> m_surface{ m_instance, vkDestroySurfaceKHR };

	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE; // implicitly destroyed when the instance is destroyed
	QueueFamilyIndices m_queueFamilyIndices; // queue families used by this app
	VDeleter<VkDevice> m_device{ vkDestroyDevice };

	VkQueue m_graphicsQueue; // queues are implicitly released when the logical device is released
	VkQueue m_presentQueue;

	SwapChainWrapper m_swapChain{ m_device };

	VDeleter<VkDescriptorPool> m_descriptorPool{ m_device, vkDestroyDescriptorPool };

	VDeleter<VkCommandPool> m_graphicsCommandPool{ m_device, vkDestroyCommandPool };
	std::vector<VkCommandBuffer> m_presentCommandBuffers;
	std::vector<VDeleter<VkFramebuffer>> m_finalOutputFramebuffers; // present framebuffers

	VDeleter<VkPipelineCache> m_pipelineCache{ m_device, vkDestroyPipelineCache };

	std::vector<VMesh> m_models;
	std::vector<BakedBRDF> m_bakedBRDFs;

	VTextOverlay m_textOverlay{ m_physicalDevice, m_device, m_queueFamilyIndices, m_graphicsQueue, m_swapChain, m_finalOutputFramebuffers };
	Timer m_perfTimer;


	virtual void initVulkan();

	virtual void mainLoop();

	virtual void recreateSwapChain();

	// Let the app determine its own criteria of a suitable physical device
	virtual bool isDeviceSuitable(VkPhysicalDevice physicalDevice);

	// Let the app pick the queue families they need
	virtual std::set<int> getUniqueQueueFamilyIndices();
	virtual void getEnabledPhysicalDeviceFeatures(VkPhysicalDeviceFeatures &features);
	virtual void getRequiredQueues();

	virtual VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	virtual VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);
	virtual VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	virtual void createRenderPasses() = 0;
	virtual void createDescriptorSetLayouts() = 0;
	virtual void createGraphicsPipelines() = 0;
	virtual void createCommandPools() = 0;
	virtual void createDepthResources() = 0;
	virtual void createColorAttachmentResources() = 0;
	virtual void createFramebuffers() = 0;
	virtual void loadAndPrepareAssets() = 0;
	virtual void createUniformBuffers() = 0;
	virtual void createDescriptorPools() = 0;
	virtual void createDescriptorSets() = 0;
	virtual void createCommandBuffers() = 0;
	virtual void createSynchronizationObjects() = 0; // semaphores, fences, etc. go in here

	virtual void updateUniformBuffers() = 0;
	virtual void updateText(uint32_t imageIdx, Timer *timer = nullptr);
	virtual void drawFrame() = 0;


	void initWindow();

	std::vector<const char*> getRequiredExtensions();
	
	void createInstance();
	
	void setupDebugCallback();
	
	void createSurface();

	void createLogicalDevice();

	void createSwapChain();

	void createSwapChainImageViews();

	void createPipelineCache();
};


#ifdef VBASE_IMPLEMENTATION

bool VBaseGraphics::leftMBDown = false;
bool VBaseGraphics::middleMBDown = false;
float VBaseGraphics::lastX;
float VBaseGraphics::lastY;

void VBaseGraphics::initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // not to create OpenGL context
	m_window = glfwCreateWindow(m_width, m_height, m_windowTitle.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(m_window, this);
	glfwSetWindowSizeCallback(m_window, VBaseGraphics::onWindowResized);
	glfwSetMouseButtonCallback(m_window, VBaseGraphics::mouseButtonCB);
	glfwSetCursorPosCallback(m_window, VBaseGraphics::cursorPositionCB);
	glfwSetKeyCallback(m_window, VBaseGraphics::keyCB);
	glfwSetScrollCallback(m_window, VBaseGraphics::scrollCB);
}

std::vector<const char*> VBaseGraphics::getRequiredExtensions()
{
	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++)
	{
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

void VBaseGraphics::createInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport(m_validationLayers))
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = m_windowTitle.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
		createInfo.ppEnabledLayerNames = m_validationLayers.data();
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

void VBaseGraphics::setupDebugCallback()
{
	if (!enableValidationLayers) return;

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallback;

	if (createDebugReportCallbackEXT(m_instance, &createInfo, nullptr, m_debugReportCB.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug callback!");
	}
}

void VBaseGraphics::createSurface()
{
	if (glfwCreateWindowSurface(m_instance, m_window, nullptr, m_surface.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}
}

void VBaseGraphics::createLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = getUniqueQueueFamilyIndices();

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

	VkPhysicalDeviceFeatures deviceFeatures{};
	getEnabledPhysicalDeviceFeatures(deviceFeatures);

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

	if (enableValidationLayers)
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

	getRequiredQueues();
}

void VBaseGraphics::createSwapChain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice, m_surface);

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
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; // always 1 for non-stereoscopic 3D applications
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	const QueueFamilyIndices &indices = m_queueFamilyIndices;
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

	VkSwapchainKHR oldSwapChain = m_swapChain.swapChain;
	createInfo.oldSwapchain = oldSwapChain;

	VkSwapchainKHR newSwapChain;
	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &newSwapChain) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swap chain!");
	}

	m_swapChain.swapChain = newSwapChain;

	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, nullptr);
	m_swapChain.swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_device, m_swapChain.swapChain, &imageCount, m_swapChain.swapChainImages.data());

	m_swapChain.swapChainImageFormat = surfaceFormat.format;
	m_swapChain.swapChainExtent = extent;
}

void VBaseGraphics::createSwapChainImageViews()
{
	m_swapChain.swapChainImageViews.resize(m_swapChain.swapChainImages.size(), VDeleter<VkImageView>{m_device, vkDestroyImageView});

	for (uint32_t i = 0; i < m_swapChain.swapChainImages.size(); i++)
	{
		createImageView2D(m_device, m_swapChain.swapChainImages[i], m_swapChain.swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, m_swapChain.swapChainImageViews[i]);
	}
}

void VBaseGraphics::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, m_pipelineCache.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline cache.");
	}
}

void VBaseGraphics::run()
{
	initWindow();
	initVulkan();
	mainLoop();
}

void VBaseGraphics::initVulkan()
{
	using namespace std::placeholders;

	createInstance();
	setupDebugCallback();
	createSurface();
	// I also initialize m_queueFamilyIndices in the next statement
	m_physicalDevice = pickPhysicalDevice(m_instance, std::bind(&VBaseGraphics::isDeviceSuitable, this, _1));
	createLogicalDevice();
	createSwapChain();
	createSwapChainImageViews();
	createPipelineCache();
	createRenderPasses();
	createDescriptorSetLayouts();
	createGraphicsPipelines();
	createCommandPools();
	createDepthResources();
	createColorAttachmentResources();
	createFramebuffers();
	loadAndPrepareAssets();
	createUniformBuffers();
	createDescriptorPools();
	createDescriptorSets();
	createCommandBuffers();
	createSynchronizationObjects();
	m_textOverlay.prepareResources();
}

void VBaseGraphics::updateText(uint32_t imageIdx, Timer *timer)
{
	m_textOverlay.waitForFence(imageIdx);
	if (timer) timer->stop();

	m_textOverlay.beginTextUpdate();

	std::stringstream ss;
	ss << m_windowTitle << " - ver" << m_verNumMajor << "." << m_verNumMinor;
	m_textOverlay.addText(ss.str(), 5.0f, 5.0f, VTextOverlay::alignLeft);

	ss = std::stringstream();
	float averageMsPerFrame = m_perfTimer.getAverageTime();
	ss << std::fixed << std::setprecision(2) << "Frame time: " << averageMsPerFrame << " ms (" << 1000.f / averageMsPerFrame << " FPS)";
	m_textOverlay.addText(ss.str(), 5.f, 25.f, VTextOverlay::alignLeft);

	m_textOverlay.endTextUpdate(imageIdx);
}

void VBaseGraphics::mainLoop()
{
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();

		updateUniformBuffers();
		drawFrame();
	}

	// When the main loop terminate, some commands may still being executed on
	// the GPU. So it is a bad idea to release resources right away. Instead,
	// we wait for the logical device to idle before releasing resources.
	vkDeviceWaitIdle(m_device);
}

void VBaseGraphics::recreateSwapChain()
{
	auto updateCamera = [this]()
	{
		m_camera.aspectRatio = m_swapChain.swapChainExtent.width / static_cast<float>(m_swapChain.swapChainExtent.height);
	};

	vkDeviceWaitIdle(m_device);

	createSwapChain();
	updateCamera();
	createSwapChainImageViews();
	createRenderPasses();
	createGraphicsPipelines();
	createDepthResources();
	createColorAttachmentResources();
	createFramebuffers();
	// Image views of G-buffers and lighting result image has changed so
	// recreation of descriptor sets is necessary
	createDescriptorSets();
	createCommandBuffers();
}

bool VBaseGraphics::isDeviceSuitable(VkPhysicalDevice physicalDevice)
{
	m_queueFamilyIndices.findQueueFamilies(physicalDevice, m_surface);

	bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice, m_deviceExtensions);

	bool swapChainAdequate = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, m_surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	auto isQueueFamilyIndicesComplete = [this]()
	{
		return m_queueFamilyIndices.graphicsFamily >= 0 && m_queueFamilyIndices.presentFamily >= 0;
	};

	return isQueueFamilyIndicesComplete() && extensionsSupported && swapChainAdequate;
}

std::set<int> VBaseGraphics::getUniqueQueueFamilyIndices()
{
	return{ m_queueFamilyIndices.graphicsFamily, m_queueFamilyIndices.presentFamily };
}

void VBaseGraphics::getEnabledPhysicalDeviceFeatures(VkPhysicalDeviceFeatures &features)
{
	features = VkPhysicalDeviceFeatures{};
}

void VBaseGraphics::getRequiredQueues()
{
	vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
}

/**
 * Choose a suitable format for swap chain images
 */
VkSurfaceFormatKHR VBaseGraphics::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
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

VkPresentModeKHR VBaseGraphics::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
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

VkExtent2D VBaseGraphics::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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
		VkExtent2D actualExtent = { m_width, m_height };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

#endif // VBASE_IMPLEMENTATION