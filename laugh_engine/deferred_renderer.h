/**
 * Notes for myself
 * 1. How to add a new uniform buffer?
 *   - Add the uniform buffer in the shaders that use it
 *   - If it is a uniform buffer, add the new struct with the same layout as the uniform
 *     somewhere visible to this file
 *   - Add an instance in the DeferredRenderer class
 *   - Create a VkBuffer and allocate device memory to interface with the device
 *   - Increase descriptor pool size accordingly
 *   - Create a VkDescriptorSetLayoutBinding to put it into descriptor set layouts that use it
 *   - Create a VkDescriptorBufferInfo and VkWriteDescriptorSet to write the uniform descriptor
 *     into the sets that use it
 *   - Allocate host memory in DeferredRenderer::createUniformBuffers
 *   - Update it in DeferredRenderer::updateUniformBuffers and copy data to device memory
 *
 * 2. How to add a new texture sampled in shaders?
 *   - Load in the texture
 *   - Create a staging buffer and copy pixel data to it. Buffer usage need to have VK_BUFFER_USAGE_TRANSFER_SRC_BIT set.
 *     Memory properties are VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
 *   - Create a device local image with the same format and transfer data from staging buffer to it
 *   - Transfer image layout from VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 *   - Create a VkImageView and VkSampler
 *   - Increase descriptor pool size
 *   - Add new VkDescriptorSetLayoutBindings to the descriptor set layouts that use the texture
 *   - Add new VkDescritorImageInfos and VkWriteDescriptorSets to the descriptor sets that use it
 *   - Add uniform Sampler2D's to shaders and bind desired descriptor sets before draw calls
 *
 * 3. How to create an image and use it as an attachment?
 *   - Create a VkImage, VkDeviceMemory, and VkImageView
 *   - Image format is usually VK_FORMAT_R8G8B8A8_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, or VK_FORMAT_R32G32B32A32_SFLOAT.
 *     For depth image, you need to find a format that is supported by your device and prefer the one with higher precision
 *   - Image usage is VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, or VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
 *     depend on how you want to use it
 *   - Attach it to the framebuffers of the render passes that use this attachment
 *   - Add VkAttachmentDescriptions and VkAttachmentReferences to the render passes and subpasses that use it
 *     specify initial, final layouts, and the layouts the image should have during each subpass that uses it
 *   - Configure pipelines accordingly. For example, enable depth test or color blending
 *   - If the attachment will be used as an input attachment, you will also need to add VkDescriptorSetLayoutBindings and
 *     VkWriteDescriptorSets to the layouts and sets that use it. In the shaders, you will need to add a uniform subpassInput
 *     with the correct input_attachment_index specified in the layout qualifier. The index is the attachment's index in the
 *     pInputAttachments array of VkSubpassDescription. And instead of using texture(), use subpassLoad() to read the input
 *     attachment. Input attachment doesn't support magnification, minification, or sampling. So subpasses that use input
 *     attachments should render into attachments with the same resolution as input attachments
 *   - Provide a clear value when recording command buffers if the loadOp is VK_LOAD_OP_CLEAR
 */

#pragma once

#include <array>

#include "vbase.h"

#define BRDF_LUT_SIZE 256
#define ALL_UNIFORM_BLOB_SIZE (64 * 1024)
#define NUM_LIGHTS 2

#define BRDF_BASE_DIR "../textures/BRDF_LUTs/"
#define BRDF_NAME "FSchlick_DGGX_GSmith.dds"

#define PROBE_BASE_DIR "../textures/Environment/PaperMill/"
//#define PROBE_BASE_DIR "../textures/Environment/Factory/"
//#define PROBE_BASE_DIR "../textures/Environment/MonValley/"
//#define PROBE_BASE_DIR "../textures/Environment/Canyon/"

//#define MODEL_NAMES { "Cerberus" }
//#define MODEL_NAMES { "Jeep_Wagoneer" }
//#define MODEL_NAMES { "9mm_Pistol" }
//#define MODEL_NAMES { "Drone_Body", "Drone_Legs", "Floor" }
#define MODEL_NAMES { "Combat_Helmet" }
//#define MODEL_NAMES { "Bug_Ship" }
//#define MODEL_NAMES { "Knight_Base", "Knight_Helmet", "Knight_Chainmail", "Knight_Skirt", "Knight_Sword", "Knight_Armor" }


struct CubeMapCameraUniformBuffer
{
	glm::mat4 V[6];
	glm::mat4 P;
};

struct TransMatsUniformBuffer
{
	glm::mat4 VP;
};

struct PointLight
{
	glm::vec4 position;
	glm::vec3 color;
	float radius;
};

// due to std140 padding for uniform buffer object
// only use data types that are vec4 or multiple of vec4's
struct LightingPassUniformBuffer
{
	glm::vec4 eyePos;
	PointLight pointLights[NUM_LIGHTS];
};

struct DisplayInfoUniformBuffer
{
	typedef int DisplayMode_t;
	
	DisplayMode_t displayMode;
};


class DeferredRenderer : public VBaseGraphics
{
public:
	DeferredRenderer()
	{
		m_windowTitle = "Laugh Engine";
		m_verNumMajor = 0;
		m_verNumMinor = 1;
	}

	virtual void run();

protected:
	VDeleter<VkRenderPass> m_specEnvPrefilterRenderPass{ m_device, vkDestroyRenderPass };
	VDeleter<VkRenderPass> m_geomAndLightRenderPass{ m_device, vkDestroyRenderPass };
	std::vector<VDeleter<VkRenderPass>> m_bloomRenderPasses;
	VDeleter<VkRenderPass> m_finalOutputRenderPass{ m_device, vkDestroyRenderPass };

	VDeleter<VkDescriptorSetLayout> m_brdfLutDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_specEnvPrefilterDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_skyboxDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_geomDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_lightingDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_bloomDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_finalOutputDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };

	VDeleter<VkPipelineLayout> m_brdfLutPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_brdfLutPipeline{ m_device, vkDestroyPipeline };
	VDeleter<VkPipelineLayout> m_diffEnvPrefilterPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_diffEnvPrefilterPipeline{ m_device, vkDestroyPipeline };
	VDeleter<VkPipelineLayout> m_specEnvPrefilterPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_specEnvPrefilterPipeline{ m_device, vkDestroyPipeline };
	VDeleter<VkPipelineLayout> m_skyboxPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_skyboxPipeline{ m_device, vkDestroyPipeline };
	VDeleter<VkPipelineLayout> m_geomPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_geomPipeline{ m_device, vkDestroyPipeline };
	VDeleter<VkPipelineLayout> m_lightingPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_lightingPipeline{ m_device, vkDestroyPipeline };
	std::vector<VDeleter<VkPipelineLayout>> m_bloomPipelineLayout;
	std::vector<VDeleter<VkPipeline>> m_bloomPipelines;
	VDeleter<VkPipelineLayout> m_finalOutputPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_finalOutputPipeline{ m_device, vkDestroyPipeline };

	ImageWrapper m_depthImage{ m_device };
	ImageWrapper m_lightingResultImage{ m_device, VK_FORMAT_R16G16B16A16_SFLOAT };
	std::vector<ImageWrapper> m_gbufferImages = { { m_device, VK_FORMAT_R32G32B32A32_SFLOAT }, { m_device, VK_FORMAT_R32G32B32A32_SFLOAT } };
	std::vector<ImageWrapper> m_postEffectImages = { { m_device, VK_FORMAT_R16G16B16A16_SFLOAT }, { m_device, VK_FORMAT_R16G16B16A16_SFLOAT } };

	AllUniformBlob<ALL_UNIFORM_BLOB_SIZE> m_allUniformHostData{ m_physicalDevice };
	CubeMapCameraUniformBuffer *m_uCubeViews = nullptr;
	TransMatsUniformBuffer *m_uTransMats = nullptr;
	LightingPassUniformBuffer *m_uLightInfo = nullptr;
	DisplayInfoUniformBuffer *m_uDisplayInfo = nullptr;
	BufferWrapper m_allUniformBuffer{ m_device };

	VkDescriptorSet m_brdfLutDescriptorSet;
	VkDescriptorSet m_specEnvPrefilterDescriptorSet;
	VkDescriptorSet m_skyboxDescriptorSet;
	std::vector<VkDescriptorSet> m_geomDescriptorSets; // one set per model
	VkDescriptorSet m_lightingDescriptorSet;
	VkDescriptorSet m_finalOutputDescriptorSet;

	VDeleter<VkFramebuffer> m_diffEnvPrefilterFramebuffer{ m_device, vkDestroyFramebuffer };
	std::vector<VDeleter<VkFramebuffer>> m_specEnvPrefilterFramebuffers;
	VDeleter<VkFramebuffer> m_geomAndLightingFramebuffer{ m_device, vkDestroyFramebuffer };

	VDeleter<VkSemaphore> m_imageAvailableSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_geomAndLightingCompleteSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_finalOutputFinishedSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_renderFinishedSemaphore{ m_device, vkDestroySemaphore };

	VDeleter<VkFence> m_brdfLutFence{ m_device, vkDestroyFence };
	VDeleter<VkFence> m_envPrefilterFence{ m_device, vkDestroyFence };

	VkCommandBuffer m_brdfLutCommandBuffer;
	VkCommandBuffer m_envPrefilterCommandBuffer;
	VkCommandBuffer m_geomAndLightingCommandBuffer;


	virtual void createRenderPasses();
	virtual void createDescriptorSetLayouts();
	virtual void createComputePipelines();
	virtual void createGraphicsPipelines();
	virtual void createCommandPools();
	virtual void createComputeResources();
	virtual void createDepthResources();
	virtual void createColorAttachmentResources();
	virtual void loadAndPrepareAssets();
	virtual void createUniformBuffers();
	virtual void createDescriptorPools();
	virtual void createDescriptorSets();
	virtual void createFramebuffers();
	virtual void createCommandBuffers();
	virtual void createSynchronizationObjects(); // semaphores, fences, etc. go in here

	virtual void updateUniformBuffers();
	virtual void drawFrame();

	// Helpers
	virtual void createSpecEnvPrefilterRenderPass();
	virtual void createGeometryAndLightingRenderPass();
	virtual void createBloomRenderPasses();
	virtual void createFinalOutputRenderPass();

	virtual void createBrdfLutDescriptorSetLayout();
	virtual void createSpecEnvPrefilterDescriptorSetLayout();
	virtual void createSkyboxDescriptorSetLayout();
	virtual void createStaticMeshDescriptorSetLayout();
	virtual void createGeomPassDescriptorSetLayout();
	virtual void createLightingPassDescriptorSetLayout();
	virtual void createBloomDescriptorSetLayout();
	virtual void createFinalOutputDescriptorSetLayout();

	virtual void createBrdfLutPipeline();
	virtual void createDiffEnvPrefilterPipeline();
	virtual void createSpecEnvPrefilterPipeline();
	virtual void createSkyboxPipeline();
	virtual void createStaticMeshPipeline();
	virtual void createGeomPassPipeline();
	virtual void createLightingPassPipeline();
	virtual void createBloomPipelines();
	virtual void createFinalOutputPassPipeline();

	// Descriptor sets cannot be altered once they are bound until execution of all related
	// commands complete. So each model will need a different descriptor set because they use
	// different textures
	virtual void createBrdfLutDescriptorSet();
	virtual void createSpecEnvPrefilterDescriptorSet();
	virtual void createSkyboxDescriptorSet();
	virtual void createStaticMeshDescriptorSet();
	virtual void createGeomPassDescriptorSets();
	virtual void createLightingPassDescriptorSets();
	virtual void createFinalOutputPassDescriptorSets();

	virtual void createBrdfLutCommandBuffer();
	virtual void createEnvPrefilterCommandBuffer();
	virtual void createGeomAndLightingCommandBuffer();
	virtual void createPresentCommandBuffers();

	virtual void prefilterEnvironmentAndComputeBrdfLut();
	virtual void savePrecomputationResults();

	virtual VkFormat findDepthFormat();
};


#ifdef DEFERED_RENDERER_IMPLEMENTATION

void DeferredRenderer::run()
{
	//system("pause");
	initWindow();
	initVulkan();
	prefilterEnvironmentAndComputeBrdfLut();
	mainLoop();
	savePrecomputationResults();
}

void DeferredRenderer::updateUniformBuffers()
{
	// cube map views and projection
	m_uCubeViews->V[0] = glm::lookAt(glm::vec3(0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));	// +X
	m_uCubeViews->V[1] = glm::lookAt(glm::vec3(0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));	// -X
	m_uCubeViews->V[2] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));	// +Y
	m_uCubeViews->V[3] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));	// -Y
	m_uCubeViews->V[4] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));	// +Z
	m_uCubeViews->V[5] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));	// -Z
	m_uCubeViews->P = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 100.f);

	// update transformation matrices
	glm::mat4 V, P;
	m_camera.getViewProjMatrix(V, P);

	m_uTransMats->VP = P * V;

	// update lighting info
	m_uLightInfo->pointLights[0] =
	{
		V * glm::vec4(2.f, 2.f, 1.f, 1.f),
		glm::vec3(2.f, 2.f, 2.f),
		5.f
	};
	m_uLightInfo->pointLights[1] =
	{
		V * glm::vec4(-2.f, -1.f, -.5f, 1.f),
		glm::vec3(.2f, .2f, .2f),
		5.f
	};
	m_uLightInfo->eyePos = glm::vec4(m_camera.position, 1.0f);

	// update final output pass info
	*m_uDisplayInfo =
	{
		m_displayMode,
	};

	// update per model information
	for (auto &model : m_models)
	{
		model.updateHostUniformBuffer();
	}

	void* data;
	vkMapMemory(m_device, m_allUniformBuffer.bufferMemory, 0, m_allUniformBuffer.sizeInBytes, 0, &data);
	memcpy(data, &m_allUniformHostData, m_allUniformBuffer.sizeInBytes);
	vkUnmapMemory(m_device, m_allUniformBuffer.bufferMemory);
}

void DeferredRenderer::drawFrame()
{
	uint32_t imageIndex;

	// acquired image may not be renderable because the presentation engine is still using it
	// when @m_imageAvailableSemaphore is signaled, presentation is complete and the image can be used for rendering
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain.swapChain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	updateText(imageIndex, &m_perfTimer);
	m_perfTimer.start();

	VkSubmitInfo submitInfos[2] = {};

	VkSemaphore waitSemaphores0[] = { m_imageAvailableSemaphore };
	VkSemaphore signalSemaphores0[] = { m_geomAndLightingCompleteSemaphore };
	VkPipelineStageFlags waitStages0[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfos[0].waitSemaphoreCount = 1;
	submitInfos[0].pWaitSemaphores = waitSemaphores0;
	submitInfos[0].pWaitDstStageMask = waitStages0;
	submitInfos[0].commandBufferCount = 1;
	submitInfos[0].signalSemaphoreCount = 1;
	submitInfos[0].pSignalSemaphores = signalSemaphores0;
	submitInfos[0].pCommandBuffers = &m_geomAndLightingCommandBuffer;

	VkSemaphore waitSemaphores1[] = { m_geomAndLightingCompleteSemaphore };
	VkSemaphore signalSemaphores1[] = { m_finalOutputFinishedSemaphore };
	VkPipelineStageFlags waitStages1[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfos[1].waitSemaphoreCount = 1;
	submitInfos[1].pWaitSemaphores = waitSemaphores1;
	submitInfos[1].pWaitDstStageMask = waitStages1;
	submitInfos[1].commandBufferCount = 1;
	submitInfos[1].signalSemaphoreCount = 1;
	submitInfos[1].pSignalSemaphores = signalSemaphores1;
	submitInfos[1].pCommandBuffers = &m_presentCommandBuffers[imageIndex];

	if (vkQueueSubmit(m_graphicsQueue, 2, submitInfos, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	std::vector<VkSemaphore> waitSemaphores2 = { m_finalOutputFinishedSemaphore };
	std::vector<VkSemaphore> signalSemaphores2 = { m_renderFinishedSemaphore };
	std::vector<VkPipelineStageFlags> waitStages2 = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	m_textOverlay.submit(imageIndex, waitStages2, waitSemaphores2, signalSemaphores2);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores2.data();

	VkSwapchainKHR swapChains[] = { m_swapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr; // Optional

	result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void DeferredRenderer::createRenderPasses()
{
	createSpecEnvPrefilterRenderPass();
	createGeometryAndLightingRenderPass();
	createBloomRenderPasses();
	createFinalOutputRenderPass();
}

void DeferredRenderer::createDescriptorSetLayouts()
{
	createBrdfLutDescriptorSetLayout();
	createSpecEnvPrefilterDescriptorSetLayout();
	createGeomPassDescriptorSetLayout();
	createLightingPassDescriptorSetLayout();
	createFinalOutputDescriptorSetLayout();
}

void DeferredRenderer::createComputePipelines()
{
	createBrdfLutPipeline();
}

void DeferredRenderer::createGraphicsPipelines()
{
	createDiffEnvPrefilterPipeline();
	createSpecEnvPrefilterPipeline();
	createGeomPassPipeline();
	createLightingPassPipeline();
	createFinalOutputPassPipeline();
}

void DeferredRenderer::createCommandPools()
{
	createCommandPool(m_device, m_queueFamilyIndices.graphicsFamily, m_graphicsCommandPool);
	createCommandPool(m_device, m_queueFamilyIndices.computeFamily, m_computeCommandPool);
}

void DeferredRenderer::createComputeResources()
{
	// BRDF LUT
	std::string brdfFileName = "";
	if (fileExist(BRDF_BASE_DIR BRDF_NAME))
	{
		brdfFileName = BRDF_BASE_DIR BRDF_NAME;
	}

	m_bakedBRDFs.resize(1, { m_device });

	VkSamplerCreateInfo clampedSampler = {};
	getDefaultSamplerCreateInfo(clampedSampler);
	clampedSampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	clampedSampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	clampedSampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	clampedSampler.anisotropyEnable = VK_FALSE;
	clampedSampler.maxAnisotropy = 0.f;

	if (brdfFileName != "")
	{
		// Do not generate mip levels for BRDF LUTs
		loadTextureWithSampler(m_physicalDevice, m_device, m_graphicsCommandPool, m_graphicsQueue, false, clampedSampler, brdfFileName, m_bakedBRDFs[0]);
		m_bakedBrdfReady = true;
	}
	else
	{
		m_bakedBRDFs[0].mipLevels = 1;
		m_bakedBRDFs[0].format = VK_FORMAT_R32G32_SFLOAT;

		createImage(
			m_physicalDevice, m_device,
			BRDF_LUT_SIZE, BRDF_LUT_SIZE, m_bakedBRDFs[0].format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // want to read back and store to disk
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_bakedBRDFs[0].image, m_bakedBRDFs[0].imageMemory);

		transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
			m_bakedBRDFs[0].image, m_bakedBRDFs[0].format, 1, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL);

		createImageView2D(m_device, m_bakedBRDFs[0].image, m_bakedBRDFs[0].format, VK_IMAGE_ASPECT_COLOR_BIT, 1, m_bakedBRDFs[0].imageView);

		clampedSampler.maxLod = static_cast<float>(m_bakedBRDFs[0].mipLevels - 1);
		if (vkCreateSampler(m_device, &clampedSampler, nullptr, m_bakedBRDFs[0].sampler.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create brdf lut sampler!");
		}

		m_shouldSaveBakedBrdf = true;
	}
}

void DeferredRenderer::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat();

	m_depthImage.format = depthFormat;

	createImage(m_physicalDevice, m_device,
		m_swapChain.swapChainExtent.width, m_swapChain.swapChainExtent.height, depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_depthImage.image, m_depthImage.imageMemory);

	createImageView2D(m_device, m_depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, m_depthImage.imageView);

	transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
		m_depthImage.image, depthFormat, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkSamplerCreateInfo samplerInfo = {};
	getDefaultSamplerCreateInfo(samplerInfo);

	if (vkCreateSampler(m_device, &samplerInfo, nullptr, m_depthImage.sampler.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting result image sampler.");
	}
}

void DeferredRenderer::createColorAttachmentResources()
{
	// Gbuffer images
	for (auto &image : m_gbufferImages)
	{
		createImage(m_physicalDevice, m_device,
			m_swapChain.swapChainExtent.width, m_swapChain.swapChainExtent.height, image.format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			image.image, image.imageMemory);

		createImageView2D(m_device, image.image, image.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, image.imageView);

		VkSamplerCreateInfo samplerInfo = {};
		getDefaultSamplerCreateInfo(samplerInfo);

		if (vkCreateSampler(m_device, &samplerInfo, nullptr, image.sampler.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create lighting result image sampler.");
		}
	}

	// Lighting result image
	createImage(m_physicalDevice, m_device,
		m_swapChain.swapChainExtent.width, m_swapChain.swapChainExtent.height, m_lightingResultImage.format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_lightingResultImage.image, m_lightingResultImage.imageMemory);

	createImageView2D(m_device, m_lightingResultImage.image, m_lightingResultImage.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, m_lightingResultImage.imageView);

	VkSamplerCreateInfo lightingSamplerInfo = {};
	getDefaultSamplerCreateInfo(lightingSamplerInfo);
	lightingSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	lightingSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	lightingSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if (vkCreateSampler(m_device, &lightingSamplerInfo, nullptr, m_lightingResultImage.sampler.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting result image sampler.");
	}
}

void DeferredRenderer::loadAndPrepareAssets()
{
	// TODO: implement scene file to allow flexible model loading
	std::string skyboxFileName = "../models/sky_sphere.obj";
	std::string unfilteredProbeFileName = PROBE_BASE_DIR "Unfiltered_HDR.dds";
	std::string specProbeFileName = "";
	if (fileExist(PROBE_BASE_DIR "Specular_HDR.dds"))
	{
		specProbeFileName = PROBE_BASE_DIR "Specular_HDR.dds";
	}
	std::string diffuseProbeFileName = "";
	if (fileExist(PROBE_BASE_DIR "Diffuse_HDR.dds"))
	{
		diffuseProbeFileName = PROBE_BASE_DIR "Diffuse_HDR.dds";
	}

	m_skybox.load(m_physicalDevice, m_device, m_graphicsCommandPool, m_graphicsQueue, skyboxFileName, unfilteredProbeFileName, specProbeFileName, diffuseProbeFileName);

	std::vector<std::string> modelNames = MODEL_NAMES;
	m_models.resize(modelNames.size(), { m_device });

	for (size_t i = 0; i < m_models.size(); ++i)
	{
		const std::string &name = modelNames[i];

		std::string modelFileName = "../models/" + name + ".obj";
		std::string albedoMapName = "../textures/" + name + "/A.dds";
		std::string normalMapName = "../textures/" + name + "/N.dds";
		std::string roughnessMapName = "../textures/" + name + "/R.dds";
		std::string metalnessMapName = "../textures/" + name + "/M.dds";

		m_models[i].load(
			m_physicalDevice, m_device,
			m_graphicsCommandPool, m_graphicsQueue,
			modelFileName, albedoMapName, normalMapName, roughnessMapName, metalnessMapName);
		m_models[i].worldRotation = glm::quat(glm::vec3(0.f, glm::pi<float>(), 0.f));
	}
}

void DeferredRenderer::createUniformBuffers()
{
	// host
	m_uCubeViews = reinterpret_cast<CubeMapCameraUniformBuffer *>(m_allUniformHostData.alloc(sizeof(CubeMapCameraUniformBuffer)));
	m_uTransMats = reinterpret_cast<TransMatsUniformBuffer *>(m_allUniformHostData.alloc(sizeof(TransMatsUniformBuffer)));
	m_uLightInfo = reinterpret_cast<LightingPassUniformBuffer *>(m_allUniformHostData.alloc(sizeof(LightingPassUniformBuffer)));
	m_uDisplayInfo = reinterpret_cast<DisplayInfoUniformBuffer *>(m_allUniformHostData.alloc(sizeof(DisplayInfoUniformBuffer)));

	for (auto &model : m_models)
	{
		model.uPerModelInfo = reinterpret_cast<PerModelUniformBuffer *>(m_allUniformHostData.alloc(sizeof(PerModelUniformBuffer)));
	}

	// device
	m_allUniformBuffer.sizeInBytes = m_allUniformHostData.size();
	m_allUniformBuffer.numElements = 1;

	createBuffer(
		m_physicalDevice, m_device,
		m_allUniformBuffer.sizeInBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		m_allUniformBuffer.buffer, m_allUniformBuffer.bufferMemory);
}

void DeferredRenderer::createDescriptorPools()
{
	// create descriptor pool
	std::array<VkDescriptorPoolSize, 4> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 6 + 2 * m_models.size();

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// BRDF LUTs + radiance map + diffuse irradiance map + specular irradiance map + lighting result + g-buffers + depth image + model textures
	poolSizes[1].descriptorCount = 9 + m_models.size() * VMesh::numMapsPerMesh + m_bakedBRDFs.size();

	poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizes[2].descriptorCount = 3;

	poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[3].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 5 + m_models.size();

	if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, m_descriptorPool.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void DeferredRenderer::createDescriptorSets()
{
	if (vkResetDescriptorPool(m_device, m_descriptorPool, 0) != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot reset m_descriptorPool");
	}

	// create descriptor sets
	std::vector<VkDescriptorSetLayout> layouts;
	layouts.push_back(m_brdfLutDescriptorSetLayout);
	layouts.push_back(m_specEnvPrefilterDescriptorSetLayout);
	layouts.push_back(m_skyboxDescriptorSetLayout);
	layouts.push_back(m_lightingDescriptorSetLayout);
	layouts.push_back(m_finalOutputDescriptorSetLayout);
	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		layouts.push_back(m_geomDescriptorSetLayout);
	}

	std::vector<VkDescriptorSet> sets(5 + m_models.size());

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(sets.size());
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(m_device, &allocInfo, sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set!");
	}

	m_brdfLutDescriptorSet = sets[0];
	m_specEnvPrefilterDescriptorSet = sets[1];
	m_skyboxDescriptorSet = sets[2];
	m_lightingDescriptorSet = sets[3];
	m_finalOutputDescriptorSet = sets[4];
	m_geomDescriptorSets.resize(m_models.size());
	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		m_geomDescriptorSets[i] = sets[5 + i];
	}

	// geometry pass descriptor set will be updated for every model
	// so there is no need to pre-initialize it
	createBrdfLutDescriptorSet();
	createSpecEnvPrefilterDescriptorSet();
	createGeomPassDescriptorSets();
	createLightingPassDescriptorSets();
	createFinalOutputPassDescriptorSets();
}

void DeferredRenderer::createFramebuffers()
{
	// Diffuse irradiance map pass
	if (!m_skybox.diffMapReady)
	{
		std::array<VkImageView, 1> attachments =
		{
			m_skybox.diffuseIrradianceMap.imageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_specEnvPrefilterRenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = DIFF_IRRADIANCE_MAP_SIZE;
		framebufferInfo.height = DIFF_IRRADIANCE_MAP_SIZE;
		framebufferInfo.layers = 6;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, m_diffEnvPrefilterFramebuffer.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}

	// Specular irradiance map pass
	if (!m_skybox.specMapReady)
	{
		m_specEnvPrefilterFramebuffers.resize(m_skybox.specularIrradianceMap.mipLevels, { m_device, vkDestroyFramebuffer });
		for (uint32_t level = 0; level < m_skybox.specularIrradianceMap.mipLevels; ++level)
		{
			uint32_t faceWidth = SPEC_IRRADIANCE_MAP_SIZE / (1 << level);
			uint32_t faceHeight = faceWidth;

			std::array<VkImageView, 1> attachments =
			{
				m_skybox.specularIrradianceMap.imageViews[level]
			};

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_specEnvPrefilterRenderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = faceWidth;
			framebufferInfo.height = faceHeight;
			framebufferInfo.layers = 6;

			if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, m_specEnvPrefilterFramebuffers[level].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	// Used in geometry and lighting pass
	{
		std::array<VkImageView, 4> attachments =
		{
			m_depthImage.imageView,
			m_gbufferImages[0].imageView,
			m_gbufferImages[1].imageView,
			m_lightingResultImage.imageView
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_geomAndLightRenderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapChain.swapChainExtent.width;
		framebufferInfo.height = m_swapChain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, m_geomAndLightingFramebuffer.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}

	// Used in final output pass
	m_finalOutputFramebuffers.resize(m_swapChain.swapChainImages.size(), VDeleter<VkFramebuffer>{m_device, vkDestroyFramebuffer});

	for (size_t i = 0; i < m_finalOutputFramebuffers.size(); ++i)
	{
		std::array<VkImageView, 1> attachments =
		{
			m_swapChain.swapChainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_finalOutputRenderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = m_swapChain.swapChainExtent.width;
		framebufferInfo.height = m_swapChain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, m_finalOutputFramebuffers[i].replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void DeferredRenderer::createCommandBuffers()
{
	// Allocate graphics command buffers
	if (vkResetCommandPool(m_device, m_graphicsCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot reset m_graphicsCommandPool");
	}

	m_presentCommandBuffers.resize(m_swapChain.swapChainImages.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_presentCommandBuffers.size() + 2;

	std::vector<VkCommandBuffer> commandBuffers(allocInfo.commandBufferCount);
	if (vkAllocateCommandBuffers(m_device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	int idx;
	for (idx = 0; idx < m_presentCommandBuffers.size(); ++idx)
	{
		m_presentCommandBuffers[idx] = commandBuffers[idx];
	}
	m_geomAndLightingCommandBuffer = commandBuffers[idx++];
	m_envPrefilterCommandBuffer = commandBuffers[idx++];

	// Create command buffers for different purposes
	createEnvPrefilterCommandBuffer();
	createGeomAndLightingCommandBuffer();
	createPresentCommandBuffers();

	// compute command buffers
	if (vkResetCommandPool(m_device, m_computeCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot reset m_computeCommandPool");
	}

	allocInfo.commandPool = m_computeCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	commandBuffers.clear();
	commandBuffers.resize(allocInfo.commandBufferCount);
	if (vkAllocateCommandBuffers(m_device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate command buffers!");
	}

	m_brdfLutCommandBuffer = commandBuffers[0];

	createBrdfLutCommandBuffer();
}

void DeferredRenderer::createSynchronizationObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, m_imageAvailableSemaphore.replace()) != VK_SUCCESS ||
		vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, m_geomAndLightingCompleteSemaphore.replace()) != VK_SUCCESS ||
		vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, m_finalOutputFinishedSemaphore.replace()) != VK_SUCCESS ||
		vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, m_renderFinishedSemaphore.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create semaphores!");
	}

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if (vkCreateFence(m_device, &fenceInfo, nullptr, m_brdfLutFence.replace()) != VK_SUCCESS ||
		vkCreateFence(m_device, &fenceInfo, nullptr, m_envPrefilterFence.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create fences!");
	}
}

void DeferredRenderer::createSpecEnvPrefilterRenderPass()
{
	std::array<VkAttachmentDescription, 1> attachments = {};

	// Cube map faces of one mip level
	attachments[0].format = m_skybox.specularIrradianceMap.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 1> colorAttachmentRefs = {};
	colorAttachmentRefs[0].attachment = 0;
	colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDescription, 1> subPasses = {};
	subPasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPasses[0].colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
	subPasses[0].pColorAttachments = colorAttachmentRefs.data();

	std::array<VkSubpassDependency, 2> dependencies = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dependencies[1].dstAccessMask = 0;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = static_cast<uint32_t>(subPasses.size());
	renderPassInfo.pSubpasses = subPasses.data();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_specEnvPrefilterRenderPass.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void DeferredRenderer::createGeometryAndLightingRenderPass()
{
	// --- Attachments used in this render pass
	std::array<VkAttachmentDescription, 4> attachments = {};

	// Depth
	attachments[0].format = findDepthFormat();
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// World space normal + albedo
	// Normal has been perturbed by normal mapping
	attachments[1].format = m_gbufferImages[0].format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // only happen in the FIRST subpass that uses this attachment
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // we don't care about the initial layout of this attachment image (content may not be preserved)
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// World postion
	attachments[2].format = m_gbufferImages[1].format;
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // only happen in the FIRST subpass that uses this attachment
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // we don't care about the initial layout of this attachment image (content may not be preserved)
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Lighting result
	attachments[3].format = m_lightingResultImage.format;
	attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// --- Reference to render pass attachments used in each subpass
	std::array<VkAttachmentReference, 2> geomColorAttachmentRefs = {};
	geomColorAttachmentRefs[0].attachment = 1; // corresponds to the index of the corresponding element in the pAttachments array of the VkRenderPassCreateInfo structure
	geomColorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	geomColorAttachmentRefs[1].attachment = 2;
	geomColorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 1> lightingColorAttachmentRefs = {};
	lightingColorAttachmentRefs[0].attachment = 3;
	lightingColorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 3> lightingInputAttachmentRefs = {};
	lightingInputAttachmentRefs[0].attachment = 1;
	lightingInputAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	lightingInputAttachmentRefs[1].attachment = 2;
	lightingInputAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	lightingInputAttachmentRefs[2].attachment = 0;
	lightingInputAttachmentRefs[2].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference geomDepthAttachmentRef = {};
	geomDepthAttachmentRef.attachment = 0;
	geomDepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference lightingDepthAttachmentRef = {};
	lightingDepthAttachmentRef.attachment = 0;
	lightingDepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	// --- Subpasses
	std::array<VkSubpassDescription, 2> subPasses = {};

	// Geometry subpass
	subPasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPasses[0].colorAttachmentCount = static_cast<uint32_t>(geomColorAttachmentRefs.size());
	subPasses[0].pColorAttachments = geomColorAttachmentRefs.data();
	subPasses[0].pDepthStencilAttachment = &geomDepthAttachmentRef; // at most one depth-stencil attachment

	// Lighting subpass
	subPasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPasses[1].colorAttachmentCount = static_cast<uint32_t>(lightingColorAttachmentRefs.size());
	subPasses[1].pColorAttachments = lightingColorAttachmentRefs.data();
	subPasses[1].inputAttachmentCount = static_cast<uint32_t>(lightingInputAttachmentRefs.size());
	subPasses[1].pInputAttachments = lightingInputAttachmentRefs.data();
	subPasses[1].pDepthStencilAttachment = &lightingDepthAttachmentRef;

	// --- Subpass dependencies
	std::array<VkSubpassDependency, 3> dependencies = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstAccessMask = 
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = 
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[1].dstAccessMask =
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].srcAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstStageMask =
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstAccessMask = 
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// --- Create render pass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = static_cast<uint32_t>(subPasses.size());
	renderPassInfo.pSubpasses = subPasses.data();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_geomAndLightRenderPass.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void DeferredRenderer::createBloomRenderPasses()
{
	m_bloomRenderPasses.resize(2, { m_device, vkDestroyRenderPass });

	std::array<VkAttachmentDescription, 1> attachments = {};

	attachments[0].format = m_postEffectImages[0].format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	std::array<VkAttachmentReference, 1> colorAttachmentRefs = {};
	colorAttachmentRefs[0].attachment = 0;
	colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkSubpassDescription, 1> subPasses = {};

	subPasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPasses[0].colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
	subPasses[0].pColorAttachments = colorAttachmentRefs.data();

	std::array<VkSubpassDependency, 1> dependencies = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = static_cast<uint32_t>(subPasses.size());
	renderPassInfo.pSubpasses = subPasses.data();
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	// Brightnesss and Gaussian blur passes
	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_bloomRenderPasses[0].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}

	attachments[0].format = m_lightingResultImage.format;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

	// Merge pass
	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_bloomRenderPasses[1].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void DeferredRenderer::createFinalOutputRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_swapChain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subPass = {};
	subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPass.colorAttachmentCount = 1;
	subPass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask = 
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = 
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subPass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_finalOutputRenderPass.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create render pass!");
	}
}

void DeferredRenderer::createBrdfLutDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 1> bindings = {};

	// output buffer
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_brdfLutDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createSpecEnvPrefilterDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};

	// 6 View matrices + projection matrix
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	// HDR probe a.k.a. radiance environment map with mips
	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_specEnvPrefilterDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createGeomPassDescriptorSetLayout()
{
	createStaticMeshDescriptorSetLayout();
	createSkyboxDescriptorSetLayout();
}

void DeferredRenderer::createSkyboxDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};

	// Transformation matrices
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr; // Optional

	// Albedo map
	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].pImmutableSamplers = nullptr;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_skyboxDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createStaticMeshDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 6> bindings = {};

	// Transformation matrices
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = nullptr; // Optional

	// Per model information
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[1].pImmutableSamplers = nullptr;

	// Albedo map
	bindings[2].binding = 2;
	bindings[2].descriptorCount = 1;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].pImmutableSamplers = nullptr;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Normal map
	bindings[3].binding = 3;
	bindings[3].descriptorCount = 1;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].pImmutableSamplers = nullptr;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Roughness map
	bindings[4].binding = 4;
	bindings[4].descriptorCount = 1;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].pImmutableSamplers = nullptr;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// Metalness map
	bindings[5].binding = 5;
	bindings[5].descriptorCount = 1;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].pImmutableSamplers = nullptr;
	bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_geomDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createLightingPassDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 7> bindings = {};

	// Light information
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr; // Optional

	// gbuffer 1
	bindings[1].binding = 1;
	bindings[1].descriptorCount = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[1].pImmutableSamplers = nullptr;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// gbuffer 2
	bindings[2].binding = 2;
	bindings[2].descriptorCount = 1;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[2].pImmutableSamplers = nullptr;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// depth image
	bindings[3].binding = 3;
	bindings[3].descriptorCount = 1;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[3].pImmutableSamplers = nullptr;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// diffuse irradiance map
	bindings[4].binding = 4;
	bindings[4].descriptorCount = 1;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].pImmutableSamplers = nullptr;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// specular irradiance map (prefiltered environment map)
	bindings[5].binding = 5;
	bindings[5].descriptorCount = 1;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].pImmutableSamplers = nullptr;
	bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// BRDF LUT
	bindings[6].binding = 6;
	bindings[6].descriptorCount = 1;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[6].pImmutableSamplers = nullptr;
	bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_lightingDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createBloomDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 1> bindings = {};

	// input image
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_bloomDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createFinalOutputDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 5> bindings = {};

	// Final image, gbuffer, depth image
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[2].pImmutableSamplers = nullptr;

	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[3].pImmutableSamplers = nullptr;

	// Uniform buffer
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[4].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_finalOutputDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createBrdfLutPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.cs = "../shaders/brdf_lut_pass/brdf_lut.comp.spv";

	DefaultComputePipelineCreateInfo infos{ m_device, shaderFiles };

	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = &m_brdfLutDescriptorSetLayout;

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_brdfLutPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create brdf lut pass pipeline layout!");
	}

	infos.pipelineInfo.layout = m_brdfLutPipelineLayout;

	if (vkCreateComputePipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_brdfLutPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create brdf lut pass pipeline!");
	}
}

void DeferredRenderer::createDiffEnvPrefilterPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/env_prefilter_pass/env_prefilter.vert.spv";
	shaderFiles.gs = "../shaders/env_prefilter_pass/env_prefilter.geom.spv";
	shaderFiles.fs = "../shaders/env_prefilter_pass/diff_env_prefilter.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	infos.vertexInputInfo.vertexBindingDescriptionCount = 1;
	infos.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	infos.vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	infos.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	infos.rasterizerInfo.cullMode = VK_CULL_MODE_NONE;

	infos.depthStencilInfo.depthTestEnable = VK_FALSE;
	infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
	infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	std::vector<VkDynamicState> dynamicStateEnables =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	infos.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	infos.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	infos.dynamicStateInfo.pDynamicStates = dynamicStateEnables.data();

	VkDescriptorSetLayout setLayouts[] = { m_specEnvPrefilterDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_diffEnvPrefilterPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create diffuse environment pipeline layout!");
	}

	infos.pipelineInfo.layout = m_diffEnvPrefilterPipelineLayout;
	infos.pipelineInfo.renderPass = m_specEnvPrefilterRenderPass;
	infos.pipelineInfo.subpass = 0;
	infos.pipelineInfo.pDynamicState = &infos.dynamicStateInfo;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_diffEnvPrefilterPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create diffuse environment pipeline!");
	}
}

void DeferredRenderer::createSpecEnvPrefilterPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/env_prefilter_pass/env_prefilter.vert.spv";
	shaderFiles.gs = "../shaders/env_prefilter_pass/env_prefilter.geom.spv";
	shaderFiles.fs = "../shaders/env_prefilter_pass/spec_env_prefilter.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	infos.vertexInputInfo.vertexBindingDescriptionCount = 1;
	infos.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	infos.vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	infos.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	infos.rasterizerInfo.cullMode = VK_CULL_MODE_NONE;

	infos.depthStencilInfo.depthTestEnable = VK_FALSE;
	infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
	infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	std::vector<VkDynamicState> dynamicStateEnables =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	infos.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	infos.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
	infos.dynamicStateInfo.pDynamicStates = dynamicStateEnables.data();

	VkDescriptorSetLayout setLayouts[] = { m_specEnvPrefilterDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	infos.pushConstantRanges.resize(1);
	infos.pushConstantRanges[0].offset = 0;
	infos.pushConstantRanges[0].size = sizeof(float);
	infos.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	infos.pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(infos.pushConstantRanges.size());
	infos.pipelineLayoutInfo.pPushConstantRanges = infos.pushConstantRanges.data();

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_specEnvPrefilterPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create specular environment pipeline layout!");
	}

	infos.pipelineInfo.layout = m_specEnvPrefilterPipelineLayout;
	infos.pipelineInfo.renderPass = m_specEnvPrefilterRenderPass;
	infos.pipelineInfo.subpass = 0;
	infos.pipelineInfo.pDynamicState = &infos.dynamicStateInfo;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_specEnvPrefilterPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create specular environment pipeline!");
	}
}

void DeferredRenderer::createGeomPassPipeline()
{
	createSkyboxPipeline();
	createStaticMeshPipeline();
}

void DeferredRenderer::createSkyboxPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/geom_pass/skybox.vert.spv";
	shaderFiles.fs = "../shaders/geom_pass/skybox.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	infos.vertexInputInfo.vertexBindingDescriptionCount = 1;
	infos.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	infos.vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	infos.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkViewport viewport;
	VkRect2D scissor;
	defaultViewportAndScissor(m_swapChain.swapChainExtent, viewport, scissor);

	infos.viewportStateInfo.viewportCount = 1;
	infos.viewportStateInfo.pViewports = &viewport;
	infos.viewportStateInfo.scissorCount = 1;
	infos.viewportStateInfo.pScissors = &scissor;

	infos.rasterizerInfo.cullMode = VK_CULL_MODE_NONE;

	infos.colorBlendAttachmentStates.resize(2);
	infos.colorBlendAttachmentStates[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	infos.colorBlendAttachmentStates[1].blendEnable = VK_FALSE;

	infos.colorBlendInfo.attachmentCount = 2;
	infos.colorBlendInfo.pAttachments = infos.colorBlendAttachmentStates.data();

	VkDescriptorSetLayout setLayouts[] = { m_skyboxDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	infos.pushConstantRanges.resize(1);
	infos.pushConstantRanges[0].offset = 0;
	infos.pushConstantRanges[0].size = sizeof(uint32_t);
	infos.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	infos.pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(infos.pushConstantRanges.size());
	infos.pipelineLayoutInfo.pPushConstantRanges = infos.pushConstantRanges.data();

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_skyboxPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create geom pipeline layout!");
	}

	infos.pipelineInfo.layout = m_skyboxPipelineLayout;
	infos.pipelineInfo.renderPass = m_geomAndLightRenderPass;
	infos.pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_skyboxPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create geom pipeline!");
	}
}

void DeferredRenderer::createStaticMeshPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/geom_pass/geom.vert.spv";
	shaderFiles.fs = "../shaders/geom_pass/geom.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	infos.vertexInputInfo.vertexBindingDescriptionCount = 1;
	infos.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	infos.vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	infos.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkViewport viewport;
	VkRect2D scissor;
	defaultViewportAndScissor(m_swapChain.swapChainExtent, viewport, scissor);

	infos.viewportStateInfo.viewportCount = 1;
	infos.viewportStateInfo.pViewports = &viewport;
	infos.viewportStateInfo.scissorCount = 1;
	infos.viewportStateInfo.pScissors = &scissor;

	infos.colorBlendAttachmentStates.resize(2);
	infos.colorBlendAttachmentStates[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	infos.colorBlendAttachmentStates[1].blendEnable = VK_FALSE;

	infos.colorBlendInfo.attachmentCount = 2;
	infos.colorBlendInfo.pAttachments = infos.colorBlendAttachmentStates.data();

	VkDescriptorSetLayout setLayouts[] = { m_geomDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	infos.pushConstantRanges.resize(1);
	infos.pushConstantRanges[0].offset = 0;
	infos.pushConstantRanges[0].size = sizeof(uint32_t);
	infos.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	infos.pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(infos.pushConstantRanges.size());
	infos.pipelineLayoutInfo.pPushConstantRanges = infos.pushConstantRanges.data();

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_geomPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create geom pipeline layout!");
	}

	infos.pipelineInfo.layout = m_geomPipelineLayout;
	infos.pipelineInfo.renderPass = m_geomAndLightRenderPass;
	infos.pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_geomPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create geom pipeline!");
	}
}

void DeferredRenderer::createLightingPassPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/fullscreen.vert.spv";
	shaderFiles.fs = "../shaders/lighting_pass/lighting.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	// Use specialization constants to pass number of lights to the shader
	VkSpecializationMapEntry specializationEntry{};
	specializationEntry.constantID = 0;
	specializationEntry.offset = 0;
	specializationEntry.size = sizeof(uint32_t);

	uint32_t specializationData = NUM_LIGHTS;

	VkSpecializationInfo specializationInfo;
	specializationInfo.mapEntryCount = 1;
	specializationInfo.pMapEntries = &specializationEntry;
	specializationInfo.dataSize = sizeof(specializationData);
	specializationInfo.pData = &specializationData;

	infos.shaderStages[1].pSpecializationInfo = &specializationInfo;

	VkViewport viewport;
	VkRect2D scissor;
	defaultViewportAndScissor(m_swapChain.swapChainExtent, viewport, scissor);

	infos.viewportStateInfo.viewportCount = 1;
	infos.viewportStateInfo.pViewports = &viewport;
	infos.viewportStateInfo.scissorCount = 1;
	infos.viewportStateInfo.pScissors = &scissor;

	infos.depthStencilInfo.depthTestEnable = VK_TRUE;
	infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
	infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS; // TODO: reenable it when proxy is implemented

	VkDescriptorSetLayout setLayouts[] = { m_lightingDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	infos.pushConstantRanges.resize(1);
	infos.pushConstantRanges[0].offset = 0;
	infos.pushConstantRanges[0].size = sizeof(uint32_t);
	infos.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	infos.pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(infos.pushConstantRanges.size());
	infos.pipelineLayoutInfo.pPushConstantRanges = infos.pushConstantRanges.data();

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_lightingPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting pipeline layout!");
	}

	infos.pipelineInfo.layout = m_lightingPipelineLayout;
	infos.pipelineInfo.renderPass = m_geomAndLightRenderPass;
	infos.pipelineInfo.subpass = 1;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_lightingPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting pipeline!");
	}
}

void DeferredRenderer::createBloomPipelines()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/fullscreen.vert.spv";
	shaderFiles.fs = "../shaders/bloom_pass/brightness_mask.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	VkDescriptorSetLayout setLayouts[] = { m_bloomDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	// brightness_mask and merge
	m_bloomPipelineLayout.resize(2, { m_device, vkDestroyPipelineLayout });
	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_bloomPipelineLayout[0].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create bloom pipeline layout!");
	}

	infos.pushConstantRanges.resize(1);
	infos.pushConstantRanges[0].offset = 0;
	infos.pushConstantRanges[0].size = sizeof(uint32_t);
	infos.pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	infos.pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(infos.pushConstantRanges.size());
	infos.pipelineLayoutInfo.pPushConstantRanges = infos.pushConstantRanges.data();

	// gaussian blur
	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_bloomPipelineLayout[1].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create bloom pipeline layout!");
	}

	VkViewport viewport;
	VkRect2D scissor;
	defaultViewportAndScissor(m_swapChain.swapChainExtent, viewport, scissor);

	infos.viewportStateInfo.viewportCount = 1;
	infos.viewportStateInfo.pViewports = &viewport;
	infos.viewportStateInfo.scissorCount = 1;
	infos.viewportStateInfo.pScissors = &scissor;

	infos.depthStencilInfo.depthTestEnable = VK_FALSE;
	infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
	infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	infos.pipelineInfo.layout = m_bloomPipelineLayout[0];
	infos.pipelineInfo.renderPass = m_bloomRenderPasses[0];
	infos.pipelineInfo.subpass = 1;

	m_bloomPipelines.resize(3, { m_device, vkDestroyPipeline });
	// brightness mask
	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_bloomPipelines[0].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create bloom pipeline!");
	}

	auto shaderCode = readFile("../shaders/bloom_pass/gaussian_blur.frag.spv");
	createShaderModule(m_device, shaderCode, infos.fragShaderModule);
	infos.pipelineInfo.layout = m_bloomPipelineLayout[1];
	infos.pipelineInfo.renderPass = m_bloomRenderPasses[0];
	infos.shaderStages[1].module = infos.fragShaderModule;

	// gaussian blur
	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_bloomPipelines[1].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create bloom pipeline!");
	}

	auto shaderCode = readFile("../shaders/bloom_pass/merge.frag.spv");
	createShaderModule(m_device, shaderCode, infos.fragShaderModule);
	infos.pipelineInfo.layout = m_bloomPipelineLayout[0];
	infos.pipelineInfo.renderPass = m_bloomRenderPasses[1];
	infos.shaderStages[1].module = infos.fragShaderModule;

	// merge
	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_bloomPipelines[2].replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create bloom pipeline!");
	}
}

void DeferredRenderer::createFinalOutputPassPipeline()
{
	ShaderFileNames shaderFiles;
	shaderFiles.vs = "../shaders/fullscreen.vert.spv";
	shaderFiles.fs = "../shaders/final_output_pass/final_output.frag.spv";

	DefaultGraphicsPipelineCreateInfo infos{ m_device, shaderFiles };

	VkViewport viewport;
	VkRect2D scissor;
	defaultViewportAndScissor(m_swapChain.swapChainExtent, viewport, scissor);

	infos.viewportStateInfo.viewportCount = 1;
	infos.viewportStateInfo.pViewports = &viewport;
	infos.viewportStateInfo.scissorCount = 1;
	infos.viewportStateInfo.pScissors = &scissor;

	infos.depthStencilInfo.depthTestEnable = VK_FALSE;
	infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
	infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	VkDescriptorSetLayout setLayouts[] = { m_finalOutputDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

	if (vkCreatePipelineLayout(m_device, &infos.pipelineLayoutInfo, nullptr, m_finalOutputPipelineLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create final output pipeline layout!");
	}

	infos.pipelineInfo.layout = m_finalOutputPipelineLayout;
	infos.pipelineInfo.renderPass = m_finalOutputRenderPass;
	infos.pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &infos.pipelineInfo, nullptr, m_finalOutputPipeline.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting pipeline!");
	}
}

void DeferredRenderer::createBrdfLutDescriptorSet()
{
	if (m_bakedBrdfReady) return;

	std::vector<VkDescriptorImageInfo> imageInfos =
	{
		m_bakedBRDFs[0].getDescriptorInfo(VK_IMAGE_LAYOUT_GENERAL)
	};

	std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_brdfLutDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrites[0].descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrites[0].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createSpecEnvPrefilterDescriptorSet()
{
	if (m_skybox.diffMapReady && m_skybox.specMapReady) return;

	std::vector<VkDescriptorBufferInfo> bufferInfos =
	{
		m_allUniformBuffer.getDescriptorInfo(
			m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uCubeViews)),
			sizeof(CubeMapCameraUniformBuffer))
	};

	std::vector<VkDescriptorImageInfo> imageInfos =
	{
		m_skybox.radianceMap.getDescriptorInfo()
	};

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_specEnvPrefilterDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = static_cast<uint32_t>(bufferInfos.size());
	descriptorWrites[0].pBufferInfo = bufferInfos.data();

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_specEnvPrefilterDescriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrites[1].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createGeomPassDescriptorSets()
{
	createSkyboxDescriptorSet();
	createStaticMeshDescriptorSet();
}

void DeferredRenderer::createSkyboxDescriptorSet()
{
	VkDescriptorBufferInfo bufferInfo = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uTransMats)),
		sizeof(DisplayInfoUniformBuffer));

	std::array<VkDescriptorImageInfo, 1> imageInfos;
	imageInfos[0] = m_skybox.radianceMap.getDescriptorInfo();

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_skyboxDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_skyboxDescriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrites[1].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createStaticMeshDescriptorSet()
{
	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		std::vector<VkDescriptorImageInfo> imageInfos =
		{
			m_models[i].albedoMap.getDescriptorInfo(),
			m_models[i].normalMap.getDescriptorInfo(),
			m_models[i].roughnessMap.getDescriptorInfo(),
			m_models[i].metalnessMap.getDescriptorInfo()
		};

		std::vector<VkDescriptorBufferInfo> bufferInfos =
		{
			m_allUniformBuffer.getDescriptorInfo(
				m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uTransMats)),
				sizeof(TransMatsUniformBuffer)),
			m_allUniformBuffer.getDescriptorInfo(
				m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_models[i].uPerModelInfo)),
				sizeof(PerModelUniformBuffer))
		};

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_geomDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = static_cast<uint32_t>(bufferInfos.size());
		descriptorWrites[0].pBufferInfo = bufferInfos.data();

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_geomDescriptorSets[i];
		descriptorWrites[1].dstBinding = 2;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
		descriptorWrites[1].pImageInfo = imageInfos.data();

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void DeferredRenderer::createLightingPassDescriptorSets()
{
	std::array<VkDescriptorBufferInfo, 1> bufferInfos = {};
	bufferInfos[0] = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uLightInfo)),
		sizeof(LightingPassUniformBuffer));

	std::array<VkDescriptorImageInfo, 3> inputAttachmentImageInfos = {};
	inputAttachmentImageInfos[0] = m_gbufferImages[0].getDescriptorInfo();
	inputAttachmentImageInfos[1] = m_gbufferImages[1].getDescriptorInfo();
	inputAttachmentImageInfos[2] = m_depthImage.getDescriptorInfo();

	std::array<VkDescriptorImageInfo, 3> samplerImageInfos = {};
	samplerImageInfos[0] = m_skybox.diffuseIrradianceMap.getDescriptorInfo();
	samplerImageInfos[1] = m_skybox.specularIrradianceMap.getDescriptorInfo();
	samplerImageInfos[2] = m_bakedBRDFs[0].getDescriptorInfo();

	std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_lightingDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = static_cast<uint32_t>(bufferInfos.size());
	descriptorWrites[0].pBufferInfo = bufferInfos.data();

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_lightingDescriptorSet;
	descriptorWrites[1].dstBinding = 1;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	descriptorWrites[1].descriptorCount = static_cast<uint32_t>(inputAttachmentImageInfos.size());
	descriptorWrites[1].pImageInfo = inputAttachmentImageInfos.data();

	descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[2].dstSet = m_lightingDescriptorSet;
	descriptorWrites[2].dstBinding = 4;
	descriptorWrites[2].dstArrayElement = 0;
	descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[2].descriptorCount = static_cast<uint32_t>(samplerImageInfos.size());
	descriptorWrites[2].pImageInfo = samplerImageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createFinalOutputPassDescriptorSets()
{
	VkDescriptorBufferInfo bufferInfo = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uDisplayInfo)),
		sizeof(DisplayInfoUniformBuffer));

	std::array<VkDescriptorImageInfo, 4> imageInfos;
	imageInfos[0] = m_lightingResultImage.getDescriptorInfo();
	imageInfos[1] = m_gbufferImages[0].getDescriptorInfo();
	imageInfos[2] = m_gbufferImages[1].getDescriptorInfo();
	imageInfos[3] = m_depthImage.getDescriptorInfo();

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_finalOutputDescriptorSet;
	descriptorWrites[0].dstBinding = 4;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = 1;
	descriptorWrites[0].pBufferInfo = &bufferInfo;

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_finalOutputDescriptorSet;
	descriptorWrites[1].dstBinding = 0;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrites[1].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createBrdfLutCommandBuffer()
{
	if (m_bakedBrdfReady) return;

	vkQueueWaitIdle(m_computeQueue);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(m_brdfLutCommandBuffer, &beginInfo);

	vkCmdBindPipeline(m_brdfLutCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdfLutPipeline);
	vkCmdBindDescriptorSets(m_brdfLutCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdfLutPipelineLayout, 0, 1, &m_brdfLutDescriptorSet, 0, nullptr);

	const uint32_t bsx = 16, bsy = 16;
	const uint32_t brdfLutSizeX = BRDF_LUT_SIZE, brdfLutSizeY = BRDF_LUT_SIZE;
	vkCmdDispatch(m_brdfLutCommandBuffer, brdfLutSizeX / bsx, brdfLutSizeY / bsy, 1);

	if (vkEndCommandBuffer(m_brdfLutCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void DeferredRenderer::createEnvPrefilterCommandBuffer()
{
	if (m_skybox.diffMapReady && m_skybox.specMapReady) return;

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(m_envPrefilterCommandBuffer, &beginInfo);

	VkBuffer vertexBuffers[] = { m_skybox.vertexBuffer.buffer };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindVertexBuffers(m_envPrefilterCommandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(m_envPrefilterCommandBuffer, m_skybox.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_specEnvPrefilterRenderPass;
	renderPassInfo.renderArea.offset = { 0, 0 };

	std::array<VkClearValue, 1> clearValues = {};
	clearValues[0].color = { { 0.f, 0.f, 0.f, 0.f } };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	VkViewport viewport = {};
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	VkRect2D scissor = {};

	// Layered rendering allows render into multiple layers but only one mip level
	// In order to render all mips, multiple passes are required
	// Diffuse prefilter pass
	renderPassInfo.framebuffer = m_diffEnvPrefilterFramebuffer;
	renderPassInfo.renderArea.extent = { DIFF_IRRADIANCE_MAP_SIZE, DIFF_IRRADIANCE_MAP_SIZE };

	vkCmdBeginRenderPass(m_envPrefilterCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_diffEnvPrefilterPipeline);
	vkCmdBindDescriptorSets(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_diffEnvPrefilterPipelineLayout, 0, 1, &m_specEnvPrefilterDescriptorSet, 0, nullptr);

	viewport.width = DIFF_IRRADIANCE_MAP_SIZE;
	viewport.height = DIFF_IRRADIANCE_MAP_SIZE;
	vkCmdSetViewport(m_envPrefilterCommandBuffer, 0, 1, &viewport);

	scissor.extent = { DIFF_IRRADIANCE_MAP_SIZE, DIFF_IRRADIANCE_MAP_SIZE };
	vkCmdSetScissor(m_envPrefilterCommandBuffer, 0, 1, &scissor);

	vkCmdDrawIndexed(m_envPrefilterCommandBuffer, static_cast<uint32_t>(m_skybox.indexBuffer.numElements), 1, 0, 0, 0);

	vkCmdEndRenderPass(m_envPrefilterCommandBuffer);

	// Specular prefitler pass
	uint32_t mipLevels = m_skybox.specularIrradianceMap.mipLevels;
	float roughness = 0.f;
	float roughnessDelta = 1.f / static_cast<float>(mipLevels - 1);

	for (uint32_t level = 0; level < mipLevels; ++level)
	{
		uint32_t faceWidth = SPEC_IRRADIANCE_MAP_SIZE / (1 << level);
		uint32_t faceHeight = faceWidth;

		renderPassInfo.framebuffer = m_specEnvPrefilterFramebuffers[level];
		renderPassInfo.renderArea.extent = { faceWidth, faceHeight };

		vkCmdBeginRenderPass(m_envPrefilterCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_specEnvPrefilterPipeline);

		vkCmdBindDescriptorSets(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_specEnvPrefilterPipelineLayout, 0, 1, &m_specEnvPrefilterDescriptorSet, 0, nullptr);
		vkCmdPushConstants(m_envPrefilterCommandBuffer, m_specEnvPrefilterPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &roughness);

		viewport.width = faceWidth;
		viewport.height = faceHeight;
		vkCmdSetViewport(m_envPrefilterCommandBuffer, 0, 1, &viewport);

		scissor.extent = { faceWidth, faceHeight };
		vkCmdSetScissor(m_envPrefilterCommandBuffer, 0, 1, &scissor);

		vkCmdDrawIndexed(m_envPrefilterCommandBuffer, static_cast<uint32_t>(m_skybox.indexBuffer.numElements), 1, 0, 0, 0);

		vkCmdEndRenderPass(m_envPrefilterCommandBuffer);

		roughness += roughnessDelta;
	}

	if (vkEndCommandBuffer(m_envPrefilterCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void DeferredRenderer::createGeomAndLightingCommandBuffer()
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr; // Optional

	vkBeginCommandBuffer(m_geomAndLightingCommandBuffer, &beginInfo);

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_geomAndLightRenderPass;
	renderPassInfo.framebuffer = m_geomAndLightingFramebuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_swapChain.swapChainExtent;

	std::array<VkClearValue, 4> clearValues = {};
	clearValues[0].depthStencil = { 1.0f, 0 };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 1
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 2
	clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // lighting result

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_geomAndLightingCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Geometry pass
	{
		vkCmdBindPipeline(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);

		VkBuffer vertexBuffers[] = { m_skybox.vertexBuffer.buffer };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(m_geomAndLightingCommandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(m_geomAndLightingCommandBuffer, m_skybox.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipelineLayout, 0, 1, &m_skyboxDescriptorSet, 0, nullptr);
		vkCmdPushConstants(m_geomAndLightingCommandBuffer, m_skyboxPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &m_skybox.materialType);

		vkCmdDrawIndexed(m_geomAndLightingCommandBuffer, static_cast<uint32_t>(m_skybox.indexBuffer.numElements), 1, 0, 0, 0);
	}

	vkCmdBindPipeline(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipeline);

	for (uint32_t j = 0; j < m_models.size(); ++j)
	{
		VkBuffer vertexBuffers[] = { m_models[j].vertexBuffer.buffer };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(m_geomAndLightingCommandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(m_geomAndLightingCommandBuffer, m_models[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipelineLayout, 0, 1, &m_geomDescriptorSets[j], 0, nullptr);
		vkCmdPushConstants(m_geomAndLightingCommandBuffer, m_geomPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &m_models[j].materialType);

		vkCmdDrawIndexed(m_geomAndLightingCommandBuffer, static_cast<uint32_t>(m_models[j].indexBuffer.numElements), 1, 0, 0, 0);
	}

	// Lighting pass
	vkCmdNextSubpass(m_geomAndLightingCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline);
	vkCmdBindDescriptorSets(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipelineLayout, 0, 1, &m_lightingDescriptorSet, 0, nullptr);
	vkCmdPushConstants(m_geomAndLightingCommandBuffer, m_lightingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof(uint32_t), &m_skybox.specularIrradianceMap.mipLevels);

	vkCmdDraw(m_geomAndLightingCommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(m_geomAndLightingCommandBuffer);

	if (vkEndCommandBuffer(m_geomAndLightingCommandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to record command buffer!");
	}
}

void DeferredRenderer::createPresentCommandBuffers()
{
	// Final ouput pass
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr; // Optional

	for (uint32_t i = 0; i < m_presentCommandBuffers.size(); ++i)
	{
		vkBeginCommandBuffer(m_presentCommandBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_finalOutputRenderPass;
		renderPassInfo.framebuffer = m_finalOutputFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChain.swapChainExtent;

		std::array<VkClearValue, 1> clearValues = {};
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(m_presentCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Geometry pass
		vkCmdBindPipeline(m_presentCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalOutputPipeline);
		vkCmdBindDescriptorSets(m_presentCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalOutputPipelineLayout, 0, 1, &m_finalOutputDescriptorSet, 0, nullptr);
		vkCmdDraw(m_presentCommandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(m_presentCommandBuffers[i]);

		if (vkEndCommandBuffer(m_presentCommandBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}
}

void DeferredRenderer::prefilterEnvironmentAndComputeBrdfLut()
{
	// References:
	//   [1] http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	//   [2] https://github.com/derkreature/IBLBaker

	// Set up cube map camera
	updateUniformBuffers();

	// Bake BRDF terms
	std::vector<VkFence> fences;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_brdfLutCommandBuffer;
	
	if (!m_bakedBrdfReady)
	{
		if (vkQueueSubmit(m_computeQueue, 1, &submitInfo, m_brdfLutFence) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit brdf lut command buffer.");
		}
		fences.push_back(m_brdfLutFence);
	}

	// Prefilter radiance map
	submitInfo.pCommandBuffers = &m_envPrefilterCommandBuffer;

	if (!m_skybox.diffMapReady || !m_skybox.specMapReady)
	{
		if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_envPrefilterFence) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit environment prefilter command buffer.");
		}
		fences.push_back(m_envPrefilterFence);
	}

	if (fences.size() > 0)
	{
		vkWaitForFences(m_device, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(m_device, static_cast<uint32_t>(fences.size()), fences.data());

		if (!m_bakedBrdfReady)
		{
			transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
				m_bakedBRDFs[0].image, m_bakedBRDFs[0].format, m_bakedBRDFs[0].mipLevels,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			m_bakedBrdfReady = true;
		}

		if (!m_skybox.diffMapReady)
		{
			transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
				m_skybox.diffuseIrradianceMap.image, m_skybox.diffuseIrradianceMap.format, 6, m_skybox.diffuseIrradianceMap.mipLevels,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		if (!m_skybox.specMapReady)
		{
			transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
				m_skybox.specularIrradianceMap.image, m_skybox.specularIrradianceMap.format, 6, m_skybox.specularIrradianceMap.mipLevels,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}
}

void DeferredRenderer::savePrecomputationResults()
{
	// read back computation results and save to disk
	if (m_shouldSaveBakedBrdf)
	{
		transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
			m_bakedBRDFs[0].image, m_bakedBRDFs[0].format, m_bakedBRDFs[0].mipLevels,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		BufferWrapper stagingBuffer{ m_device };
		VkDeviceSize sizeInBytes = BRDF_LUT_SIZE * BRDF_LUT_SIZE * sizeof(glm::vec2);

		createBuffer(m_physicalDevice, m_device, sizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer.buffer, stagingBuffer.bufferMemory);

		VkBufferImageCopy imageCopyRegion = {};
		imageCopyRegion.imageExtent = { BRDF_LUT_SIZE, BRDF_LUT_SIZE, 1 };
		imageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.imageSubresource.layerCount = 1;
		imageCopyRegion.imageSubresource.mipLevel = 0;

		// Tiling mode difference is handled by Vulkan automatically
		copyImageToBuffer(m_device, m_graphicsCommandPool, m_graphicsQueue, { imageCopyRegion }, m_bakedBRDFs[0].image, stagingBuffer.buffer);

		void* data;
		std::vector<glm::vec2> hostBrdfLutPixels(BRDF_LUT_SIZE * BRDF_LUT_SIZE);
		vkMapMemory(m_device, stagingBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
		memcpy(&hostBrdfLutPixels[0], data, sizeInBytes);
		vkUnmapMemory(m_device, stagingBuffer.bufferMemory);

		saveImage2D(BRDF_BASE_DIR BRDF_NAME,
			BRDF_LUT_SIZE, BRDF_LUT_SIZE, sizeof(glm::vec2), 1, gli::FORMAT_RG32_SFLOAT_PACK32, hostBrdfLutPixels.data());
	}

	if (m_skybox.shouldSaveDiffMap)
	{
		transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
			m_skybox.diffuseIrradianceMap.image, m_skybox.diffuseIrradianceMap.format, 6, m_skybox.diffuseIrradianceMap.mipLevels,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		BufferWrapper stagingBuffer{ m_device };
		VkDeviceSize sizeInBytes = compute2DImageSizeInBytes(
			DIFF_IRRADIANCE_MAP_SIZE, DIFF_IRRADIANCE_MAP_SIZE, sizeof(glm::vec4), m_skybox.diffuseIrradianceMap.mipLevels, 6);

		createBuffer(m_physicalDevice, m_device, sizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer.buffer, stagingBuffer.bufferMemory);

		VkBufferImageCopy imageCopyRegion = {};
		imageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.imageSubresource.mipLevel = 0;
		imageCopyRegion.imageSubresource.baseArrayLayer = 0;
		imageCopyRegion.imageSubresource.layerCount = 6;
		imageCopyRegion.imageExtent.width = DIFF_IRRADIANCE_MAP_SIZE;
		imageCopyRegion.imageExtent.height = DIFF_IRRADIANCE_MAP_SIZE;
		imageCopyRegion.imageExtent.depth = 1;
		imageCopyRegion.bufferOffset = 0;

		copyImageToBuffer(m_device, m_graphicsCommandPool, m_graphicsQueue, { imageCopyRegion }, m_skybox.diffuseIrradianceMap.image, stagingBuffer.buffer);

		void* data;
		std::vector<glm::vec4> hostPixels(sizeInBytes / sizeof(glm::vec4));
		vkMapMemory(m_device, stagingBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
		memcpy(&hostPixels[0], data, sizeInBytes);
		vkUnmapMemory(m_device, stagingBuffer.bufferMemory);

		saveImageCube(PROBE_BASE_DIR "Diffuse_HDR.dds",
			DIFF_IRRADIANCE_MAP_SIZE, DIFF_IRRADIANCE_MAP_SIZE, sizeof(glm::vec4),
			m_skybox.diffuseIrradianceMap.mipLevels, gli::FORMAT_RGBA32_SFLOAT_PACK32, hostPixels.data());
	}

	if (m_skybox.shouldSaveSpecMap)
	{
		transitionImageLayout(m_device, m_graphicsCommandPool, m_graphicsQueue,
			m_skybox.specularIrradianceMap.image, m_skybox.specularIrradianceMap.format, 6, m_skybox.specularIrradianceMap.mipLevels,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		BufferWrapper stagingBuffer{ m_device };
		VkDeviceSize sizeInBytes = compute2DImageSizeInBytes(
			SPEC_IRRADIANCE_MAP_SIZE, SPEC_IRRADIANCE_MAP_SIZE, sizeof(glm::vec4), m_skybox.specularIrradianceMap.mipLevels, 6);

		createBuffer(m_physicalDevice, m_device, sizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer.buffer, stagingBuffer.bufferMemory);

		std::vector<VkBufferImageCopy> imageCopyRegions;
		VkDeviceSize offset = 0;

		for (uint32_t face = 0; face < 6; ++face)
		{
			for (uint32_t level = 0; level < m_skybox.specularIrradianceMap.mipLevels; ++level)
			{
				uint32_t faceWidth = SPEC_IRRADIANCE_MAP_SIZE / (1 << level);
				uint32_t faceHeight = faceWidth;

				VkBufferImageCopy region = {};
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = level;
				region.imageSubresource.baseArrayLayer = face;
				region.imageSubresource.layerCount = 1;
				region.imageExtent.width = faceWidth;
				region.imageExtent.height = faceHeight;
				region.imageExtent.depth = 1;
				region.bufferOffset = offset;

				imageCopyRegions.push_back(region);

				offset += faceWidth * faceHeight * sizeof(glm::vec4);
			}
		}

		copyImageToBuffer(m_device, m_graphicsCommandPool, m_graphicsQueue, imageCopyRegions, m_skybox.specularIrradianceMap.image, stagingBuffer.buffer);

		void* data;
		std::vector<glm::vec4> hostPixels(sizeInBytes / sizeof(glm::vec4));
		vkMapMemory(m_device, stagingBuffer.bufferMemory, 0, sizeInBytes, 0, &data);
		memcpy(&hostPixels[0], data, sizeInBytes);
		vkUnmapMemory(m_device, stagingBuffer.bufferMemory);

		saveImageCube(PROBE_BASE_DIR "Specular_HDR.dds",
			SPEC_IRRADIANCE_MAP_SIZE, SPEC_IRRADIANCE_MAP_SIZE, sizeof(glm::vec4),
			m_skybox.specularIrradianceMap.mipLevels, gli::FORMAT_RGBA32_SFLOAT_PACK32, hostPixels.data());
	}

	vkDeviceWaitIdle(m_device);
}

VkFormat DeferredRenderer::findDepthFormat()
{
	return findSupportedFormat(
		m_physicalDevice,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

#endif