#include "vbase.h"


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
	createQueryPools();
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
	m_textOverlay.prepareResources();

	m_initialized = true;
}

void VBaseGraphics::mainLoop()
{
	while (!m_vulkanManager.windowShouldClose())
	{
		m_vulkanManager.windowPollEvents();
		
		updateUniformHostData();
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
		m_camera.setAspectRatio(extent.width / static_cast<float>(extent.height));
	};

	m_vulkanManager.deviceWaitIdle();

	m_vulkanManager.recreateSwapChain();
	updateCamera();
	createQueryPools();
	createRenderPasses();
	createGraphicsPipelines();
	createDepthResources();
	createColorAttachmentResources();
	createFramebuffers();
	createUniformBuffers();
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
