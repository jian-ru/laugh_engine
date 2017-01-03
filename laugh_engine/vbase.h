#pragma once

#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "camera.h"
#include "vmesh.h"


enum DisplayMode
{
	DISPLAY_MODE_FULL = 0,
	DISPLAY_MODE_ALBEDO,
	DISPLAY_MODE_EYE_NORMAL,
	DISPLAY_MODE_EYE_POSITION,
	DISPLAY_MODE_DEPTH,
	DISPLAY_MODE_ROUGHNESS,
	DISPLAY_MODE_METALNESS,
	DISPLAY_MODE_AO,
	DISPLAY_MODE_COUNT
};

class VBaseGraphics
{
public:
	uint32_t m_verNumMajor;
	uint32_t m_verNumMinor;

	uint32_t m_width = 1920;
	uint32_t m_height = 1080;
	std::string m_windowTitle = "VBaseGraphics";

	DisplayMode m_displayMode = DISPLAY_MODE_FULL;

	static bool leftMBDown, middleMBDown;
	static float lastX, lastY;

	Camera m_camera{
		glm::vec3(-1.74542487f, 1.01875722f, -2.32838178f),
		glm::vec3(0.326926917f, 0.0790613592f, -0.198676541f),
		glm::radians(45.f), float(m_width) / m_height, .1f, 100.f };


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
	VkPhysicalDeviceFeatures m_physicalDeviceFeatures;

	rj::VManager m_vulkanManager{ this, keyCB, mouseButtonCB, cursorPositionCB, scrollCB, onWindowResized, m_width, m_height, getWindowTitle(), getEnabledPhysicalDeviceFeatures() };

	uint32_t m_descriptorPool;

	uint32_t m_graphicsCommandPool;
	uint32_t m_computeCommandPool;
	std::vector<uint32_t> m_presentCommandBuffers;

	std::vector<uint32_t> m_finalOutputFramebuffers; // present framebuffer names

	Skybox m_skybox{ &m_vulkanManager };
	std::vector<VMesh> m_models;

	typedef ImageWrapper BakedBRDF;
	std::vector<BakedBRDF> m_bakedBRDFs;
	bool m_bakedBrdfReady = false;
	bool m_shouldSaveBakedBrdf = false;


	virtual void initVulkan();

	virtual void mainLoop();

	virtual void recreateSwapChain();

	// Let the app pick the queue families they need
	virtual const std::string &getWindowTitle();
	virtual const VkPhysicalDeviceFeatures &getEnabledPhysicalDeviceFeatures();

	virtual void createRenderPasses() = 0;
	virtual void createDescriptorSetLayouts() = 0;
	virtual void createComputePipelines() {}
	virtual void createGraphicsPipelines() = 0;
	virtual void createCommandPools() = 0;
	virtual void createComputeResources() {}
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
	//virtual void updateText(uint32_t imageIdx, Timer *timer = nullptr);
	virtual void drawFrame() = 0;
};


#ifdef VBASE_IMPLEMENTATION

std::shared_mutex rj::VManager::g_commandBufferMutex;

bool VBaseGraphics::leftMBDown = false;
bool VBaseGraphics::middleMBDown = false;
float VBaseGraphics::lastX;
float VBaseGraphics::lastY;


void VBaseGraphics::run()
{
	initVulkan();
	mainLoop();
}

void VBaseGraphics::initVulkan()
{
	loadAndPrepareAssets();
	createRenderPasses();
	createDescriptorSetLayouts();
	createComputePipelines();
	createGraphicsPipelines();
	createCommandPools();
	createComputeResources();
	createDepthResources();
	createColorAttachmentResources();
	createFramebuffers();
	createUniformBuffers();
	createDescriptorPools();
	createDescriptorSets();
	createCommandBuffers();
	createSynchronizationObjects();
}

void VBaseGraphics::mainLoop()
{
	while (!m_vulkanManager.windowShouldClose())
	{
		m_vulkanManager.windowPollEvents();

		updateUniformBuffers();
		drawFrame();
	}

	// When the main loop terminate, some commands may still being executed on
	// the GPU. So it is a bad idea to release resources right away. Instead,
	// we wait for the logical device to idle before releasing resources.
	m_vulkanManager.deviceWaitIdle();
}

void VBaseGraphics::recreateSwapChain()
{
	auto updateCamera = [this]()
	{
		auto extent = m_vulkanManager.getSwapChainExtent();
		m_camera.aspectRatio = extent.width / static_cast<float>(extent.height);
	};

	m_vulkanManager.deviceWaitIdle();

	m_vulkanManager.recreateSwapChain();
	updateCamera();
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

const std::string &VBaseGraphics::getWindowTitle()
{
	return m_windowTitle;
}

const VkPhysicalDeviceFeatures &VBaseGraphics::getEnabledPhysicalDeviceFeatures()
{
	m_physicalDeviceFeatures = {};
	m_physicalDeviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
	m_physicalDeviceFeatures.geometryShader = VK_TRUE;

	return m_physicalDeviceFeatures;
}

#endif // VBASE_IMPLEMENTATION