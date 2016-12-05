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
#include "vmesh.h"

#define ALL_UNIFORM_BLOB_SIZE (64 * 1024)
#define NUM_LIGHTS 2

struct GeomPassUniformBuffer
{
	glm::mat4 MVP;
	glm::mat4 MV;
	glm::mat4 MV_invTrans;
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
	PointLight pointLights[NUM_LIGHTS];
};

struct DisplayInfoUniformBuffer
{
	typedef int DisplayMode_t;
	
	DisplayMode_t displayMode;
	float imageWidth;
	float imageHeight;
	float twoTimesTanHalfFovy;
	float zNear;
	float zFar;
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

protected:
	VDeleter<VkRenderPass> m_geomAndLightRenderPass{ m_device, vkDestroyRenderPass };
	VDeleter<VkRenderPass> m_finalOutputRenderPass{ m_device, vkDestroyRenderPass };

	VDeleter<VkDescriptorSetLayout> m_geomDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_lightingDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorSetLayout> m_finalOutputDescriptorSetLayout{ m_device, vkDestroyDescriptorSetLayout };

	VDeleter<VkPipelineLayout> m_geomPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_geomPipeline{ m_device, vkDestroyPipeline };

	VDeleter<VkPipelineLayout> m_lightingPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_lightingPipeline{ m_device, vkDestroyPipeline };

	VDeleter<VkPipelineLayout> m_finalOutputPipelineLayout{ m_device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> m_finalOutputPipeline{ m_device, vkDestroyPipeline };

	ImageWrapper m_depthImage{ m_device };
	ImageWrapper m_lightingResultImage{ m_device, VK_FORMAT_R16G16B16A16_SFLOAT };
	std::vector<ImageWrapper> m_gbufferImages{ { { m_device, VK_FORMAT_R32G32B32A32_SFLOAT } } };

	std::vector<VMesh> m_models;

	AllUniformBlob<ALL_UNIFORM_BLOB_SIZE> m_allUniformHostData{ m_physicalDevice };
	GeomPassUniformBuffer *m_uTransMats = nullptr;
	LightingPassUniformBuffer *m_uLightInfo = nullptr;
	DisplayInfoUniformBuffer *m_uDisplayInfo = nullptr;
	BufferWrapper m_allUniformBuffer{ m_device };

	std::vector<VkDescriptorSet> m_geomDescriptorSets; // one set per model
	VkDescriptorSet m_lightingDescriptorSet;
	VkDescriptorSet m_finalOutputDescriptorSet;

	VDeleter<VkFramebuffer> m_geomAndLightingFramebuffer{ m_device, vkDestroyFramebuffer };

	VDeleter<VkSemaphore> m_imageAvailableSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_geomAndLightingCompleteSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_finalOutputFinishedSemaphore{ m_device, vkDestroySemaphore };
	VDeleter<VkSemaphore> m_renderFinishedSemaphore{ m_device, vkDestroySemaphore };

	VkCommandBuffer m_geomAndLightingCommandBuffer;


	virtual void createRenderPasses();
	virtual void createDescriptorSetLayouts();
	virtual void createGraphicsPipelines();
	virtual void createCommandPools();
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
	virtual void createGeometryAndLightingRenderPass();
	virtual void createFinalOutputRenderPass();

	virtual void createGeomPassDescriptorSetLayout();
	virtual void createLightingPassDescriptorSetLayout();
	virtual void createFinalOutputDescriptorSetLayout();

	virtual void createGeomPassPipeline();
	virtual void createLightingPassPipeline();
	virtual void createFinalOutputPassPipeline();

	// Descriptor sets cannot be altered once they are bound until execution of all related
	// commands complete. So each model will need a different descriptor set because they use
	// different textures
	virtual void createGeomPassDescriptorSets();
	virtual void createLightingPassDescriptorSets();
	virtual void createFinalOutputPassDescriptorSets();

	virtual void createGeomAndLightingCommandBuffer();
	virtual void createPresentCommandBuffers();

	virtual VkFormat findDepthFormat();
};


#ifdef DEFERED_RENDERER_IMPLEMENTATION

void DeferredRenderer::updateUniformBuffers()
{
	// update transformation matrices
	glm::mat4 V, P;
	m_camera.getViewProjMatrix(V, P);

	m_uTransMats->MV = V;
	m_uTransMats->MVP = P * m_uTransMats->MV;
	m_uTransMats->MV_invTrans = glm::transpose(glm::inverse(m_uTransMats->MV));

	// update lighting info
	m_uLightInfo->pointLights[0] =
	{
		V * glm::vec4(2.f, 2.f, 2.f, 1.f),
		glm::vec3(1.f, 1.f, 1.f),
		5.f
	};
	m_uLightInfo->pointLights[1] =
	{
		V * glm::vec4(0.f, 1.f, -2.f, 1.f),
		glm::vec3(0.1f, 0.1f, 0.1f),
		5.f
	};

	// update final output pass info
	*m_uDisplayInfo =
	{
		m_displayMode,
		static_cast<float>(m_swapChain.swapChainExtent.width),
		static_cast<float>(m_swapChain.swapChainExtent.height),
		2.f * tanf(m_camera.fovy * .5f),
		m_camera.zNear,
		m_camera.zFar
	};

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
	createGeometryAndLightingRenderPass();
	createFinalOutputRenderPass();
}

void DeferredRenderer::createDescriptorSetLayouts()
{
	createGeomPassDescriptorSetLayout();
	createLightingPassDescriptorSetLayout();
	createFinalOutputDescriptorSetLayout();
}

void DeferredRenderer::createGraphicsPipelines()
{
	createGeomPassPipeline();
	createLightingPassPipeline();
	createFinalOutputPassPipeline();
}

void DeferredRenderer::createCommandPools()
{
	createCommandPool(m_device, m_queueFamilyIndices.graphicsFamily, m_graphicsCommandPool);
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

	if (vkCreateSampler(m_device, &lightingSamplerInfo, nullptr, m_lightingResultImage.sampler.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create lighting result image sampler.");
	}
}

void DeferredRenderer::loadAndPrepareAssets()
{
	// TODO: implement scene file to allow flexible model loading
	std::string modelFileName = "../models/armor.obj";
	std::string albedoMapName = "../textures/armor_a.ktx";
	std::string normalMapName = "../textures/armor_n.ktx";

	m_models.resize(1, { m_device });
	m_models[0].load(
		m_physicalDevice, m_device,
		m_graphicsCommandPool, m_graphicsQueue,
		modelFileName, albedoMapName, normalMapName);
}

void DeferredRenderer::createUniformBuffers()
{
	// host
	m_uTransMats = reinterpret_cast<GeomPassUniformBuffer *>(m_allUniformHostData.alloc(sizeof(GeomPassUniformBuffer)));
	m_uLightInfo = reinterpret_cast<LightingPassUniformBuffer *>(m_allUniformHostData.alloc(sizeof(LightingPassUniformBuffer)));
	m_uDisplayInfo = reinterpret_cast<DisplayInfoUniformBuffer *>(m_allUniformHostData.alloc(sizeof(DisplayInfoUniformBuffer)));

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
	std::array<VkDescriptorPoolSize, 3> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 4;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 3 + m_models.size() * 2; // lighting result + g-buffer + depth image + model textures
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	poolSizes[2].descriptorCount = 2;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 2 + m_models.size();

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
	layouts.push_back(m_lightingDescriptorSetLayout);
	layouts.push_back(m_finalOutputDescriptorSetLayout);
	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		layouts.push_back(m_geomDescriptorSetLayout);
	}

	std::vector<VkDescriptorSet> sets(2 + m_models.size());

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(sets.size());
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(m_device, &allocInfo, sets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set!");
	}

	m_lightingDescriptorSet = sets[0];
	m_finalOutputDescriptorSet = sets[1];
	m_geomDescriptorSets.resize(m_models.size());
	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		m_geomDescriptorSets[i] = sets[2 + i];
	}

	// geometry pass descriptor set will be updated for every model
	// so there is no need to pre-initialize it
	createGeomPassDescriptorSets();
	createLightingPassDescriptorSets();
	createFinalOutputPassDescriptorSets();
}

void DeferredRenderer::createFramebuffers()
{
	m_finalOutputFramebuffers.resize(m_swapChain.swapChainImages.size(), VDeleter<VkFramebuffer>{m_device, vkDestroyFramebuffer});

	// Used in geometry and lighting pass
	std::array<VkImageView, 3> attachments =
	{
		m_depthImage.imageView,
		m_gbufferImages[0].imageView,
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

	// Used in final output pass
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
	// Allocate command buffers
	if (vkResetCommandPool(m_device, m_graphicsCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS)
	{
		throw std::runtime_error("Cannot reset m_graphicsCommandPool");
	}

	m_presentCommandBuffers.resize(m_swapChain.swapChainImages.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_graphicsCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_presentCommandBuffers.size() + 1;

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

	// Create command buffers for different purposes
	createGeomAndLightingCommandBuffer();
	createPresentCommandBuffers();
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
}

void DeferredRenderer::createGeometryAndLightingRenderPass()
{
	// --- Attachments used in this render pass
	std::array<VkAttachmentDescription, 3> attachments = {};

	// Depth
	attachments[0].format = findDepthFormat();
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Eye space normal + albedo
	// Normal has been perturbed by normal mapping
	attachments[1].format = m_gbufferImages[0].format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // only happen in the FIRST subpass that uses this attachment
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // we don't care about the initial layout of this attachment image (content may not be preserved)
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Lighting result
	attachments[2].format = m_lightingResultImage.format;
	attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// --- Reference to render pass attachments used in each subpass
	std::array<VkAttachmentReference, 1> geomColorAttachmentRefs = {};
	geomColorAttachmentRefs[0].attachment = 1; // corresponds to the index of the corresponding element in the pAttachments array of the VkRenderPassCreateInfo structure
	geomColorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 1> lightingColorAttachmentRefs = {};
	lightingColorAttachmentRefs[0].attachment = 2;
	lightingColorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 2> lightingInputAttachmentRefs = {};
	lightingInputAttachmentRefs[0].attachment = 1;
	lightingInputAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	lightingInputAttachmentRefs[1].attachment = 0;
	lightingInputAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

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

void DeferredRenderer::createGeomPassDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 3> bindings = {};

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

	// Normal map
	bindings[2].binding = 2;
	bindings[2].descriptorCount = 1;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].pImmutableSamplers = nullptr;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

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
	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};

	// Light information
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = nullptr; // Optional

	// viewport info
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = nullptr; // Optional

	// gbuffer
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

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindings.size();
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_lightingDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createFinalOutputDescriptorSetLayout()
{
	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};

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

	// Uniform buffer
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[3].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_finalOutputDescriptorSetLayout.replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void DeferredRenderer::createGeomPassPipeline()
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

	VkDescriptorSetLayout setLayouts[] = { m_geomDescriptorSetLayout };
	infos.pipelineLayoutInfo.setLayoutCount = 1;
	infos.pipelineLayoutInfo.pSetLayouts = setLayouts;

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

void DeferredRenderer::createGeomPassDescriptorSets()
{
	VkDescriptorBufferInfo bufferInfo = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uTransMats)),
		sizeof(GeomPassUniformBuffer));

	for (uint32_t i = 0; i < m_models.size(); ++i)
	{
		std::array<VkDescriptorImageInfo, 2> imageInfos;
		imageInfos[0] = m_models[i].albedoMap.getDescriptorInfo();
		imageInfos[1] = m_models[i].normalMap.getDescriptorInfo();

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_geomDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_geomDescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
		descriptorWrites[1].pImageInfo = imageInfos.data();

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void DeferredRenderer::createLightingPassDescriptorSets()
{
	std::array<VkDescriptorBufferInfo, 2> bufferInfos = {};
	bufferInfos[0] = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uLightInfo)),
		sizeof(LightingPassUniformBuffer));
	bufferInfos[1] = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uDisplayInfo)),
		sizeof(DisplayInfoUniformBuffer));

	std::array<VkDescriptorImageInfo, 2> imageInfos = {};
	imageInfos[0] = m_gbufferImages[0].getDescriptorInfo();
	imageInfos[1] = m_depthImage.getDescriptorInfo();

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_lightingDescriptorSet;
	descriptorWrites[0].dstBinding = 0;
	descriptorWrites[0].dstArrayElement = 0;
	descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrites[0].descriptorCount = static_cast<uint32_t>(bufferInfos.size());
	descriptorWrites[0].pBufferInfo = bufferInfos.data();

	descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[1].dstSet = m_lightingDescriptorSet;
	descriptorWrites[1].dstBinding = 2;
	descriptorWrites[1].dstArrayElement = 0;
	descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	descriptorWrites[1].descriptorCount = static_cast<uint32_t>(imageInfos.size());
	descriptorWrites[1].pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void DeferredRenderer::createFinalOutputPassDescriptorSets()
{
	VkDescriptorBufferInfo bufferInfo = m_allUniformBuffer.getDescriptorInfo(
		m_allUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uDisplayInfo)),
		sizeof(DisplayInfoUniformBuffer));

	std::array<VkDescriptorImageInfo, 3> imageInfos;
	imageInfos[0] = m_lightingResultImage.getDescriptorInfo();
	imageInfos[1] = m_gbufferImages[0].getDescriptorInfo();
	imageInfos[2] = m_depthImage.getDescriptorInfo();

	std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

	descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites[0].dstSet = m_finalOutputDescriptorSet;
	descriptorWrites[0].dstBinding = 3;
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

	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].depthStencil = { 1.0f, 0 };
	clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 1
	clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // lighting result

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(m_geomAndLightingCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Geometry pass
	vkCmdBindPipeline(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipeline);

	for (uint32_t j = 0; j < m_models.size(); ++j)
	{
		VkBuffer vertexBuffers[] = { m_models[j].vertexBuffer.buffer };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(m_geomAndLightingCommandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(m_geomAndLightingCommandBuffer, m_models[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipelineLayout, 0, 1, &m_geomDescriptorSets[j], 0, nullptr);

		vkCmdDrawIndexed(m_geomAndLightingCommandBuffer, static_cast<uint32_t>(m_models[j].indexBuffer.numElements), 1, 0, 0, 0);
	}

	// Lighting pass
	vkCmdNextSubpass(m_geomAndLightingCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline);
	vkCmdBindDescriptorSets(m_geomAndLightingCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipelineLayout, 0, 1, &m_lightingDescriptorSet, 0, nullptr);
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

VkFormat DeferredRenderer::findDepthFormat()
{
	return findSupportedFormat(
		m_physicalDevice,
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

#endif