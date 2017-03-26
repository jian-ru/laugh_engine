#include "deferred_renderer.h"


DeferredRenderer::DeferredRenderer()
{
	m_verNumMajor = 0;
	m_verNumMinor = 1;

	m_windowTitle = "Laugh Engine";
	m_vulkanManager.windowSetTitle(m_windowTitle);

	VkPhysicalDeviceProperties props;
	m_vulkanManager.getPhysicalDeviceProperties(&props);
	m_oneTimeUniformHostData.setAlignment(props.limits.minUniformBufferOffsetAlignment);
	m_perFrameUniformHostData.setAlignment(props.limits.minUniformBufferOffsetAlignment);
}

void DeferredRenderer::run()
{
	//system("pause");
	initVulkan();
	prefilterEnvironmentAndComputeBrdfLut();
	mainLoop();
	savePrecomputationResults();
}

void DeferredRenderer::updateUniformHostData()
{
	// update final output pass info
	*m_uDisplayInfo =
	{
		m_displayMode
	};

	// update transformation matrices
	glm::mat4 V, P;
	m_camera.getViewProjMatrix(V, P);

	m_uCameraVP->VP = P * V;

	// update lighting info
	m_uLightInfo->eyeWorldPos = m_camera.getPosition();
	m_uLightInfo->emissiveStrength = 5.f;
	for (uint32_t i = 0; i < 9; ++i)
	{
		m_uLightInfo->diffuseSHCoefficients[i] = glm::vec4(m_scene.skybox.diffuseSHCoefficients[i], 0.f);
	}
	m_uLightInfo->diffuseSHCoefficients[0].w = m_distEnvLightStrength;
	m_uLightInfo->diracLights[0] =
	{
		glm::normalize(-m_scene.shadowLight.getDirection()),
		0,
		m_scene.shadowLight.getColor(),
		0.f
	};

	// update per model information
	for (auto &model : m_scene.meshes)
	{
		model.updateHostUniformBuffer();
	}

	// shadow light information
	std::vector<glm::vec3> frustumCornersWS;
	std::vector<float> frustumSegmentDepths;
	m_camera.getCornersWorldSpace(&frustumCornersWS);
	m_camera.getSegmentDepths(&frustumSegmentDepths);
	m_scene.shadowLight.computeCascadeScalesAndOffsets(frustumCornersWS, frustumSegmentDepths,
		m_scene.aabbWorldSpace.min, m_scene.aabbWorldSpace.max, SHADOW_MAP_SIZE);
	
	m_uLightInfo->normFarPlaneZs = glm::vec4(0.f);

	for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
	{
		glm::mat4 VP;
		m_scene.shadowLight.getCascadeViewProjMatrix(i, &VP);

		m_uShadowLightInfos[i]->cascadeVP = VP;
		m_uLightInfo->normFarPlaneZs[i] = m_camera.getNormFarPlaneZ(i);
		m_uLightInfo->cascadeVPs[i] = VP;
	}
}

void DeferredRenderer::updateUniformDeviceData(uint32_t imgIdx)
{
	void* data = m_vulkanManager.mapBuffer(m_perFrameUniformDeviceData[imgIdx].buffer);
	memcpy(data, &m_perFrameUniformHostData, m_perFrameUniformDeviceData[imgIdx].size);
	m_vulkanManager.unmapBuffer(m_perFrameUniformDeviceData[imgIdx].buffer);
}

void DeferredRenderer::drawFrame()
{
	uint32_t imageIndex;

	// acquired image may not be renderable because the presentation engine is still using it
	// when @m_imageAvailableSemaphore is signaled, presentation is complete and the image can be used for rendering
	VkResult result = m_vulkanManager.swapChainNextImageIndex(&imageIndex, m_imageAvailableSemaphore, std::numeric_limits<uint32_t>::max());

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// Swapchain image @imageIndex may still be used by the presentation engine but rendering
	// is done. So it is safe to update the per-frame data for that swapchain image
	updateUniformDeviceData(imageIndex);
	updateText(imageIndex);

	// Prevent overwriting render resources (e.g. G-buffers) when the GPU is still rendering
	// Could have duplicated the resources for each swapchain image but it takes up too much memory
	m_vulkanManager.waitForFences({ m_renderFinishedFence });
	m_vulkanManager.resetFences({ m_renderFinishedFence });

	m_vulkanManager.beginQueueSubmit(VK_QUEUE_GRAPHICS_BIT);

	m_vulkanManager.queueSubmitNewSubmit({ m_perFrameCommandBuffers[imageIndex].m_geomShadowLightingCommandBuffer },
		{}, {}, { m_geomAndLightingCompleteSemaphore });

	m_vulkanManager.queueSubmitNewSubmit({ m_perFrameCommandBuffers[imageIndex].m_postEffectCommandBuffer },
		{ m_geomAndLightingCompleteSemaphore }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }, { m_postEffectSemaphore });

	m_vulkanManager.queueSubmitNewSubmit({ m_perFrameCommandBuffers[imageIndex].m_presentCommandBuffer },
		{ m_postEffectSemaphore, m_imageAvailableSemaphore },
		{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }, { m_finalOutputFinishedSemaphore });

	m_vulkanManager.endQueueSubmit(m_renderFinishedFence, false);

	m_textOverlay.submit(imageIndex, { m_finalOutputFinishedSemaphore }, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		{ m_renderFinishedSemaphore });

	result = m_vulkanManager.queuePresent({ m_renderFinishedSemaphore }, imageIndex);

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
	createGeometryRenderPass();
	createShadowRenderPass();
	createLightingRenderPass();
	createBloomRenderPasses();
	createFinalOutputRenderPass();
}

void DeferredRenderer::createDescriptorSetLayouts()
{
	createBrdfLutDescriptorSetLayout();
	createSpecEnvPrefilterDescriptorSetLayout();
	createGeomPassDescriptorSetLayout();
	createShadowPassDescriptorSetLayout();
	createLightingPassDescriptorSetLayout();
	createBloomDescriptorSetLayout();
	createFinalOutputDescriptorSetLayout();
}

void DeferredRenderer::createComputePipelines()
{
	createBrdfLutPipeline();
}

void DeferredRenderer::createGraphicsPipelines()
{
	createSpecEnvPrefilterPipeline();
	createGeomPassPipeline();
	createShadowPassPipeline();
	createLightingPassPipeline();
	createBloomPipelines();
	createFinalOutputPassPipeline();
}

void DeferredRenderer::createCommandPools()
{
	m_graphicsCommandPool = m_vulkanManager.createCommandPool(VK_QUEUE_GRAPHICS_BIT);
	m_computeCommandPool = m_vulkanManager.createCommandPool(VK_QUEUE_COMPUTE_BIT);
}

void DeferredRenderer::createComputeResources()
{
	using namespace rj::helper_functions;

	// BRDF LUT
	std::string brdfFileName = "";
	if (fileExist(BRDF_BASE_DIR BRDF_NAME))
	{
		brdfFileName = BRDF_BASE_DIR BRDF_NAME;
	}

	m_bakedBRDFs.resize(1, {});

	if (brdfFileName != "")
	{
		// Do not generate mip levels for BRDF LUTs
		loadTexture2D(&m_bakedBRDFs[0], &m_vulkanManager, brdfFileName, false);
		m_bakedBRDFs[0].samplers.resize(1);
		m_bakedBRDFs[0].samplers[0] = m_vulkanManager.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		m_bakedBrdfReady = true;
	}
	else
	{
		m_bakedBRDFs[0].format = VK_FORMAT_R32G32_SFLOAT;
		m_bakedBRDFs[0].width = BRDF_LUT_SIZE;
		m_bakedBRDFs[0].height = BRDF_LUT_SIZE;
		m_bakedBRDFs[0].depth = 1;
		m_bakedBRDFs[0].mipLevelCount = 1;
		m_bakedBRDFs[0].layerCount = 1;

		m_bakedBRDFs[0].image = m_vulkanManager.createImage2D(m_bakedBRDFs[0].width, m_bakedBRDFs[0].height, m_bakedBRDFs[0].format,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		m_vulkanManager.transitionImageLayout(m_bakedBRDFs[0].image, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL);

		m_bakedBRDFs[0].imageViews.resize(1);
		m_bakedBRDFs[0].imageViews[0] = m_vulkanManager.createImageView2D(m_bakedBRDFs[0].image, VK_IMAGE_ASPECT_COLOR_BIT);

		m_bakedBRDFs[0].samplers.resize(1);
		m_bakedBRDFs[0].samplers[0] = m_vulkanManager.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

		m_shouldSaveBakedBrdf = true;
	}
}

void DeferredRenderer::createDepthResources()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyImage(m_depthImage.image);

		for (auto name : m_depthImage.imageViews)
		{
			m_vulkanManager.destroyImageView(name);
		}

		for (auto name : m_depthImage.samplers)
		{
			m_vulkanManager.destroySampler(name);
		}

		m_vulkanManager.destroyImage(m_shadowImage.image);

		for (auto name : m_shadowImage.imageViews)
		{
			m_vulkanManager.destroyImageView(name);
		}

		for (auto name : m_shadowImage.samplers)
		{
			m_vulkanManager.destroySampler(name);
		}
	}

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	VkFormat depthFormat = findDepthFormat();

	m_depthImage.format = depthFormat;
	m_depthImage.width = swapChainExtent.width;
	m_depthImage.height = swapChainExtent.height;
	m_depthImage.depth = 1;
	m_depthImage.mipLevelCount = 1;
	m_depthImage.layerCount = 1;
	m_depthImage.sampleCount = SAMPLE_COUNT;

	m_depthImage.image = m_vulkanManager.createImage2D(m_depthImage.width, m_depthImage.height, m_depthImage.format,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 1, SAMPLE_COUNT);

	m_depthImage.imageViews.resize(1);
	VkImageAspectFlags aspectMask =
		rj::helper_functions::hasStencilComponent(depthFormat) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_DEPTH_BIT;
	m_depthImage.imageViews[0] = m_vulkanManager.createImageView2D(m_depthImage.image, aspectMask);

	m_vulkanManager.transitionImageLayout(m_depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_depthImage.samplers.resize(1);
	m_depthImage.samplers[0] = m_vulkanManager.createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	// Layered image used to store shadow maps
	m_shadowImage.format = depthFormat;
	m_shadowImage.width = SHADOW_MAP_SIZE;
	m_shadowImage.height = SHADOW_MAP_SIZE;
	m_shadowImage.depth = 1;
	m_shadowImage.mipLevelCount = 1;
	m_shadowImage.layerCount = m_camera.getSegmentCount();
	m_shadowImage.sampleCount = VK_SAMPLE_COUNT_1_BIT;

	m_shadowImage.image = m_vulkanManager.createImage2D(m_shadowImage.width, m_shadowImage.height, m_shadowImage.format,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_shadowImage.mipLevelCount, m_shadowImage.layerCount);

	m_shadowImage.imageViews.resize(m_camera.getSegmentCount() + 1);
	for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
	{
		m_shadowImage.imageViews[i] = m_vulkanManager.createImageView2D(m_shadowImage.image, aspectMask, 0, m_shadowImage.mipLevelCount, i);
	}
	m_shadowImage.imageViews.back() = m_vulkanManager.createImageView(m_shadowImage.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		aspectMask, 0, m_shadowImage.mipLevelCount, 0, m_shadowImage.layerCount);

	m_vulkanManager.transitionImageLayout(m_shadowImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_shadowImage.samplers.resize(1);
	m_shadowImage.samplers[0] = m_vulkanManager.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		0.f, 0.f, 0.f, VK_FALSE, 0.f, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_BORDER_COLOR_INT_OPAQUE_WHITE);
}

void DeferredRenderer::createColorAttachmentResources()
{
	if (m_initialized)
	{
		for (const auto &image : m_gbufferImages)
		{
			m_vulkanManager.destroyImage(image.image);

			for (auto name : image.imageViews)
			{
				m_vulkanManager.destroyImageView(name);
			}

			for (auto name : image.samplers)
			{
				m_vulkanManager.destroySampler(name);
			}
		}

		m_vulkanManager.destroyImage(m_lightingResultImage.image);

		for (auto name : m_lightingResultImage.imageViews)
		{
			m_vulkanManager.destroyImageView(name);
		}

		for (auto name : m_lightingResultImage.samplers)
		{
			m_vulkanManager.destroySampler(name);
		}

		for (const auto &image : m_postEffectImages)
		{
			m_vulkanManager.destroyImage(image.image);

			for (auto name : image.imageViews)
			{
				m_vulkanManager.destroyImageView(name);
			}

			for (auto name : image.samplers)
			{
				m_vulkanManager.destroySampler(name);
			}
		}
	}

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();

	// Gbuffer images
	m_gbufferImages.resize(m_numGBuffers);
	for (uint32_t i = 0; i < m_numGBuffers; ++i)
	{
		auto &image = m_gbufferImages[i];
		image.format = m_gbufferFormats[i];
		image.width = swapChainExtent.width;
		image.height = swapChainExtent.height;
		image.depth = 1;
		image.mipLevelCount = 1;
		image.layerCount = 1;
		image.sampleCount = SAMPLE_COUNT;

		image.image = m_vulkanManager.createImage2D(image.width, image.height, image.format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, 1, SAMPLE_COUNT);

		image.imageViews.resize(1);
		image.imageViews[0] = m_vulkanManager.createImageView2D(image.image, VK_IMAGE_ASPECT_COLOR_BIT);

		image.samplers.resize(1);
		image.samplers[0] = m_vulkanManager.createSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	}

	// Lighting result image
	m_lightingResultImage.format = m_lightingResultImageFormat;
	m_lightingResultImage.width = swapChainExtent.width;
	m_lightingResultImage.height = swapChainExtent.height;
	m_lightingResultImage.depth = 1;
	m_lightingResultImage.mipLevelCount = 1;
	m_lightingResultImage.layerCount = 1;

	m_lightingResultImage.image = m_vulkanManager.createImage2D(m_lightingResultImage.width, m_lightingResultImage.height, m_lightingResultImage.format,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	m_lightingResultImage.imageViews.resize(1);
	m_lightingResultImage.imageViews[0] = m_vulkanManager.createImageView2D(m_lightingResultImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	m_lightingResultImage.samplers.resize(1);
	m_lightingResultImage.samplers[0] = m_vulkanManager.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	// post effects - perform post processing on 1/2 resolution for the sake of performance
	m_postEffectImages.resize(m_numPostEffectImages);
	for (uint32_t i = 0; i < m_numPostEffectImages; ++i)
	{
		auto &image = m_postEffectImages[i];
		image.format = m_postEffectImageFormats[i];
		image.width = swapChainExtent.width >> 1;
		image.height = swapChainExtent.height >> 1;
		image.depth = 1;
		image.mipLevelCount = 1;
		image.layerCount = 1;

		image.image = m_vulkanManager.createImage2D(image.width, image.height, image.format,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		image.imageViews.resize(1);
		image.imageViews[0] = m_vulkanManager.createImageView2D(image.image, VK_IMAGE_ASPECT_COLOR_BIT);

		image.samplers.resize(1);
		image.samplers[0] = m_vulkanManager.createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	}
}

// TODO: implement scene file to allow flexible model loading
void DeferredRenderer::loadAndPrepareAssets()
{
	using namespace rj::helper_functions;

	// Skybox
	std::string skyboxFileName = "../models/sky_sphere.obj";
	std::string unfilteredProbeFileName = PROBE_BASE_DIR "Unfiltered_HDR.dds";
	std::string specProbeFileName = "";
	if (fileExist(PROBE_BASE_DIR "Specular_HDR.dds"))
	{
		specProbeFileName = PROBE_BASE_DIR "Specular_HDR.dds";
	}
	std::string diffuseProbeFileName = "";
	if (fileExist(PROBE_BASE_DIR "Diffuse_SH.bin"))
	{
		diffuseProbeFileName = PROBE_BASE_DIR "Diffuse_SH.bin";
	}

	m_scene.skybox.load(skyboxFileName, unfilteredProbeFileName, specProbeFileName, diffuseProbeFileName);

	// Models
#ifdef USE_GLTF
	VMesh::loadFromGLTF(m_scene.meshes, &m_vulkanManager, GLTF_NAME, GLTF_VERSION);
#else
	std::vector<std::string> modelNames = MODEL_NAMES;
	m_scene.meshes.resize(modelNames.size(), { &m_vulkanManager });

	for (size_t i = 0; i < m_scene.meshes.size(); ++i)
	{
		const std::string &name = modelNames[i];

		std::string modelFileName = "../models/" + name + ".obj";
		std::string albedoMapName = "../textures/" + name + "/A.dds";
		std::string normalMapName = "../textures/" + name + "/N.dds";
		std::string roughnessMapName = "../textures/" + name + "/R.dds";
		std::string metalnessMapName = "../textures/" + name + "/M.dds";
		std::string aoMapName = "../textures/" + name + "/AO.dds";
		if (!fileExist(aoMapName))
		{
			aoMapName = "";
		}
		std::string emissiveMapName = "../textures/" + name + "/E.dds";
		if (!fileExist(emissiveMapName))
		{
			emissiveMapName = "";
		}

		m_scene.meshes[i].load(modelFileName, albedoMapName, normalMapName, roughnessMapName, metalnessMapName, aoMapName, emissiveMapName);
		m_scene.meshes[i].setRotation(glm::quat(glm::vec3(0.f, glm::pi<float>(), 0.f)));
	}
#endif

	// Lights
	m_scene.shadowLight.setPositionAndDirection(glm::vec3(1.f), glm::vec3(-1.f));
	m_scene.shadowLight.setColor(glm::vec3(2.f));
	m_scene.shadowLight.setCastShadow(true);

	m_scene.computeAABBWorldSpace();
}

void DeferredRenderer::createUniformBuffers()
{
	// host
	if (!m_initialized)
	{
		m_uCubeViews = reinterpret_cast<CubeMapCameraUniformBuffer *>(m_oneTimeUniformHostData.alloc(sizeof(CubeMapCameraUniformBuffer)));
		m_uCameraVP = reinterpret_cast<TransMatsUniformBuffer *>(m_perFrameUniformHostData.alloc(sizeof(TransMatsUniformBuffer)));
		m_uLightInfo = reinterpret_cast<LightingPassUniformBuffer *>(m_perFrameUniformHostData.alloc(sizeof(LightingPassUniformBuffer)));
		m_uDisplayInfo = reinterpret_cast<DisplayInfoUniformBuffer *>(m_perFrameUniformHostData.alloc(sizeof(DisplayInfoUniformBuffer)));

		m_uShadowLightInfos.resize(m_camera.getSegmentCount());
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			m_uShadowLightInfos[i] = reinterpret_cast<ShadowLightUniformBuffer *>(m_perFrameUniformHostData.alloc(sizeof(ShadowLightUniformBuffer)));
		}

		for (auto &model : m_scene.meshes)
		{
			model.uPerModelInfo = reinterpret_cast<PerModelUniformBuffer *>(m_perFrameUniformHostData.alloc(sizeof(PerModelUniformBuffer)));
		}
	}

	// device
	if (!m_initialized)
	{
		m_oneTimeUniformDeviceData.size = m_oneTimeUniformHostData.size();
		m_oneTimeUniformDeviceData.offset = 0;
		m_oneTimeUniformDeviceData.buffer = m_vulkanManager.createBuffer(m_oneTimeUniformDeviceData.size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	if (m_initialized)
	{
		for (const auto &b : m_perFrameUniformDeviceData)
		{
			m_vulkanManager.destroyBuffer(b.buffer);
		}
	}

	uint32_t swapchainImageCount = m_vulkanManager.getSwapChainSize();
	m_perFrameUniformDeviceData.resize(swapchainImageCount);

	for (uint32_t i = 0; i < swapchainImageCount; ++i)
	{
		m_perFrameUniformDeviceData[i].size = m_perFrameUniformHostData.size();
		m_perFrameUniformDeviceData[i].offset = 0;
		m_perFrameUniformDeviceData[i].buffer = m_vulkanManager.createBuffer(m_perFrameUniformDeviceData[i].size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}
}

void DeferredRenderer::createDescriptorPools()
{
	const uint32_t maxSetCount = 128;
	const uint32_t maxUBDescCount = 128;
	const uint32_t maxCISDescCount = 128;
	const uint32_t maxSIDescCount = 1;
	m_vulkanManager.beginCreateDescriptorPool(maxSetCount);

	m_vulkanManager.descriptorPoolAddDescriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxUBDescCount);
	m_vulkanManager.descriptorPoolAddDescriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxCISDescCount);
	m_vulkanManager.descriptorPoolAddDescriptors(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxSIDescCount);

	m_descriptorPool = m_vulkanManager.endCreateDescriptorPool();
}

void DeferredRenderer::createDescriptorSets()
{
	m_vulkanManager.resetDescriptorPool(m_descriptorPool);

	// create descriptor sets
	std::vector<uint32_t> layouts;
	layouts.push_back(m_brdfLutDescriptorSetLayout);
	layouts.push_back(m_specEnvPrefilterDescriptorSetLayout);

	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		layouts.push_back(m_skyboxDescriptorSetLayout);
		layouts.push_back(m_lightingDescriptorSetLayout);
		layouts.push_back(m_finalOutputDescriptorSetLayout);
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			layouts.push_back(m_shadowDescriptorSetLayout1);
		}
		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			layouts.push_back(m_shadowDescriptorSetLayout2);
		}
		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			layouts.push_back(m_geomDescriptorSetLayout);
		}
		for (uint32_t i = 0; i < 3; ++i)
		{
			layouts.push_back(m_bloomDescriptorSetLayout);
		}
	}

	std::vector<uint32_t> sets = m_vulkanManager.allocateDescriptorSets(m_descriptorPool, layouts);

	uint32_t idx = 0;
	m_brdfLutDescriptorSet = sets[idx++];
	m_specEnvPrefilterDescriptorSet = sets[idx++];

	m_perFrameDescriptorSets.resize(swapChainImageCount);
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		m_perFrameDescriptorSets[imgIdx].m_skyboxDescriptorSet = sets[idx++];
		m_perFrameDescriptorSets[imgIdx].m_lightingDescriptorSet = sets[idx++];
		m_perFrameDescriptorSets[imgIdx].m_finalOutputDescriptorSet = sets[idx++];
		m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets1.resize(m_camera.getSegmentCount());
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets1[i] = sets[idx++];
		}
		m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets2.resize(m_scene.meshes.size());
		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets2[i] = sets[idx++];
		}
		m_perFrameDescriptorSets[imgIdx].m_geomDescriptorSets.resize(m_scene.meshes.size());
		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			m_perFrameDescriptorSets[imgIdx].m_geomDescriptorSets[i] = sets[idx++];
		}
		m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets.resize(3);
		for (uint32_t i = 0; i < 3; ++i)
		{
			m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[i] = sets[idx++];
		}
	}

	createBrdfLutDescriptorSet();
	createSpecEnvPrefilterDescriptorSet();
	createGeomPassDescriptorSets();
	createShadowPassDescriptorSets();
	createLightingPassDescriptorSets();
	createBloomDescriptorSets();
	createFinalOutputPassDescriptorSets();
}

void DeferredRenderer::createFramebuffers()
{
	// Used in final output pass
	m_finalOutputFramebuffers = m_vulkanManager.createSwapChainFramebuffers(m_finalOutputRenderPass);

	// Specular irradiance map pass
	if (!m_scene.skybox.specMapReady)
	{
		m_specEnvPrefilterFramebuffers.resize(m_scene.skybox.specularIrradianceMap.mipLevelCount);
		for (uint32_t level = 0; level < m_scene.skybox.specularIrradianceMap.mipLevelCount; ++level)
		{
			m_specEnvPrefilterFramebuffers[level] =
				m_vulkanManager.createFramebuffer(m_specEnvPrefilterRenderPass, { m_scene.skybox.specularIrradianceMap.imageViews[level + 1] });
		}
	}

	// Geometry pass
	{
		if (m_initialized)
		{
			m_vulkanManager.destroyFramebuffer(m_geomFramebuffer);
		}

		std::vector<uint32_t> attachmentViews =
		{
			m_depthImage.imageViews[0],
			m_gbufferImages[0].imageViews[0],
			m_gbufferImages[1].imageViews[0],
			m_gbufferImages[2].imageViews[0]
		};

		m_geomFramebuffer = m_vulkanManager.createFramebuffer(m_geomRenderPass, attachmentViews);
	}

	// Shadow pass
	{
		if (m_initialized)
		{
			m_vulkanManager.destroyFramebuffer(m_shadowFramebuffer);
		}

		std::vector<uint32_t> attachmentViews(m_camera.getSegmentCount());
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			attachmentViews[i] = m_shadowImage.imageViews[i];
		}
		m_shadowFramebuffer = m_vulkanManager.createFramebuffer(m_shadowRenderPass, attachmentViews);
	}

	// Lighting pass
	if (m_initialized)
	{
		m_vulkanManager.destroyFramebuffer(m_lightingFramebuffer);
	}

	m_lightingFramebuffer = m_vulkanManager.createFramebuffer(m_lightingRenderPass, { m_lightingResultImage.imageViews[0] });

	// Bloom
	if (m_initialized)
	{
		for (auto name : m_postEffectFramebuffers)
		{
			m_vulkanManager.destroyFramebuffer(name);
		}
	}

	m_postEffectFramebuffers.resize(3);

	m_postEffectFramebuffers[0] = m_vulkanManager.createFramebuffer(m_bloomRenderPasses[0], { m_postEffectImages[0].imageViews[0] });
	m_postEffectFramebuffers[1] = m_vulkanManager.createFramebuffer(m_bloomRenderPasses[0], { m_postEffectImages[1].imageViews[0] });
	m_postEffectFramebuffers[2] = m_vulkanManager.createFramebuffer(m_bloomRenderPasses[1], { m_lightingResultImage.imageViews[0] });
}

void DeferredRenderer::createCommandBuffers()
{
	// Allocate graphics command buffers
	m_vulkanManager.resetCommandPool(m_graphicsCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	m_perFrameCommandBuffers.resize(swapChainImageCount);

	std::vector<uint32_t> commandBuffers = m_vulkanManager.allocateCommandBuffers(m_graphicsCommandPool,
		static_cast<uint32_t>(m_perFrameCommandBuffers.size() * 3 + 1));

	int idx = 0;
	for (uint32_t imgIdx = 0; imgIdx < m_perFrameCommandBuffers.size(); ++imgIdx)
	{
		m_perFrameCommandBuffers[imgIdx].m_geomShadowLightingCommandBuffer = commandBuffers[idx++];
		m_perFrameCommandBuffers[imgIdx].m_postEffectCommandBuffer = commandBuffers[idx++];
		m_perFrameCommandBuffers[imgIdx].m_presentCommandBuffer = commandBuffers[idx++];
	}
	m_envPrefilterCommandBuffer = commandBuffers[idx++];

	// Create command buffers for different purposes
	createEnvPrefilterCommandBuffer();
	createGeomShadowLightingCommandBuffers();
	createPostEffectCommandBuffers();
	createPresentCommandBuffers();

	// compute command buffers
	m_vulkanManager.resetCommandPool(m_computeCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

	commandBuffers = m_vulkanManager.allocateCommandBuffers(m_computeCommandPool, 1);
	m_brdfLutCommandBuffer = commandBuffers[0];

	createBrdfLutCommandBuffer();
}

void DeferredRenderer::createSynchronizationObjects()
{
	m_imageAvailableSemaphore = m_vulkanManager.createSemaphore();
	m_geomAndLightingCompleteSemaphore = m_vulkanManager.createSemaphore();
	m_postEffectSemaphore = m_vulkanManager.createSemaphore();
	m_finalOutputFinishedSemaphore = m_vulkanManager.createSemaphore();
	m_renderFinishedSemaphore = m_vulkanManager.createSemaphore();

	m_brdfLutFence = m_vulkanManager.createFence();
	m_envPrefilterFence = m_vulkanManager.createFence();
	m_renderFinishedFence = m_vulkanManager.createFence(VK_FENCE_CREATE_SIGNALED_BIT);
}

void DeferredRenderer::createSpecEnvPrefilterRenderPass()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyRenderPass(m_specEnvPrefilterRenderPass);
	}

	m_vulkanManager.beginCreateRenderPass();

	m_vulkanManager.renderPassAddAttachment(m_scene.skybox.specularIrradianceMap.format,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	m_vulkanManager.renderPassAddSubpassDependency(0, VK_SUBPASS_EXTERNAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0);

	m_specEnvPrefilterRenderPass = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createShadowRenderPass()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyRenderPass(m_shadowRenderPass);
	}

	m_vulkanManager.beginCreateRenderPass();

	// --- Attachments
	// Depth of the scene from light's perspective
	for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
	{
		m_vulkanManager.renderPassAddAttachment(findDepthFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	// --- Subpasses
	for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
	{
		m_vulkanManager.beginDescribeSubpass();
		m_vulkanManager.subpassAddDepthAttachmentReference(i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		m_vulkanManager.endDescribeSubpass();
	}

	// --- Subpass dependencies
	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0);

	for (uint32_t i = 0; i < m_camera.getSegmentCount() - 1; ++i)
	{
		m_vulkanManager.renderPassAddSubpassDependency(i, i + 1,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0);
	}

	m_shadowRenderPass = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createGeometryRenderPass()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyRenderPass(m_geomRenderPass);
	}

	m_vulkanManager.beginCreateRenderPass();

	// --- Attachments used in this render pass
	// Depth
	// Clear only happens in the FIRST subpass that uses this attachment
	// VK_IMAGE_LAYOUT_UNDEFINED as initial layout means that we don't care about the initial layout of this attachment image (content may not be preserved)
	m_vulkanManager.renderPassAddAttachment(findDepthFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT);

	// World space normal + albedo
	// Normal has been perturbed by normal mapping
	m_vulkanManager.renderPassAddAttachment(m_gbufferFormats[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT);

	// World postion
	m_vulkanManager.renderPassAddAttachment(m_gbufferFormats[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT);

	// RMAI
	m_vulkanManager.renderPassAddAttachment(m_gbufferFormats[2], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, SAMPLE_COUNT);

	// --- Reference to render pass attachments used in each subpass
	// --- Subpasses
	// Geometry subpass
	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.subpassAddColorAttachmentReference(2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.subpassAddColorAttachmentReference(3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.subpassAddDepthAttachmentReference(0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	// --- Subpass dependencies
	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	// --- Create render pass
	m_geomRenderPass = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createLightingRenderPass()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyRenderPass(m_lightingRenderPass);
	}

	m_vulkanManager.beginCreateRenderPass();

	m_vulkanManager.renderPassAddAttachment(m_lightingResultImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

	m_lightingRenderPass = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createBloomRenderPasses()
{
	if (m_initialized)
	{
		for (auto renderPass : m_bloomRenderPasses)
		{
			m_vulkanManager.destroyRenderPass(renderPass);
		}
	}

	m_bloomRenderPasses.resize(2);

	// --- Bloom render pass 1 (brightness and blur passes): will clear framebuffer
	m_vulkanManager.beginCreateRenderPass();

	m_vulkanManager.renderPassAddAttachment(m_postEffectImageFormats[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

	m_bloomRenderPasses[0] = m_vulkanManager.endCreateRenderPass();

	// --- Bloom render pass 2 (Merge pass): will not clear framebuffer
	m_vulkanManager.beginCreateRenderPass();

	m_vulkanManager.renderPassAddAttachment(m_lightingResultImageFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD);

	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

	m_bloomRenderPasses[1] = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createFinalOutputRenderPass()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyRenderPass(m_finalOutputRenderPass);
	}

	m_vulkanManager.beginCreateRenderPass();

	m_vulkanManager.renderPassAddAttachment(m_vulkanManager.getSwapChainImageFormat(),
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	m_vulkanManager.beginDescribeSubpass();
	m_vulkanManager.subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	m_vulkanManager.endDescribeSubpass();

	m_vulkanManager.renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	m_finalOutputRenderPass = m_vulkanManager.endCreateRenderPass();
}

void DeferredRenderer::createBrdfLutDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);

	m_brdfLutDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createSpecEnvPrefilterDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// 6 View matrices + projection matrix
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT);

	// HDR probe a.k.a. radiance environment map with mips
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_specEnvPrefilterDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createGeomPassDescriptorSetLayout()
{
	createStaticMeshDescriptorSetLayout();
	createSkyboxDescriptorSetLayout();
}

void DeferredRenderer::createSkyboxDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// Transformation matrices
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	// Albedo map
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_skyboxDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createStaticMeshDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// Transformation matrices
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	// Per model information
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);

	// Albedo map
	m_vulkanManager.setLayoutAddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Normal map
	m_vulkanManager.setLayoutAddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Roughness map
	m_vulkanManager.setLayoutAddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Metalness map
	m_vulkanManager.setLayoutAddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// AO map
	m_vulkanManager.setLayoutAddBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Emissive map
	m_vulkanManager.setLayoutAddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_geomDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createShadowPassDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();
	// light View Project Crop Matrix
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_shadowDescriptorSetLayout1 = m_vulkanManager.endCreateDescriptorSetLayout();

	m_vulkanManager.beginCreateDescriptorSetLayout();
	// Per model information
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	m_shadowDescriptorSetLayout2 = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createLightingPassDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// Light information
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// gbuffer 1
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// gbuffer 2
	m_vulkanManager.setLayoutAddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// gbuffer 3
	m_vulkanManager.setLayoutAddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// depth image
	m_vulkanManager.setLayoutAddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// specular irradiance map (prefiltered environment map)
	m_vulkanManager.setLayoutAddBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// BRDF LUT
	m_vulkanManager.setLayoutAddBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// shadow maps
	m_vulkanManager.setLayoutAddBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_lightingDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createBloomDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// input image
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_bloomDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createFinalOutputDescriptorSetLayout()
{
	m_vulkanManager.beginCreateDescriptorSetLayout();

	// Final image, gbuffer, depth image
	m_vulkanManager.setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vulkanManager.setLayoutAddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vulkanManager.setLayoutAddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vulkanManager.setLayoutAddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_vulkanManager.setLayoutAddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	// Uniform buffer
	m_vulkanManager.setLayoutAddBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);

	m_finalOutputDescriptorSetLayout = m_vulkanManager.endCreateDescriptorSetLayout();
}

void DeferredRenderer::createBrdfLutPipeline()
{
	const std::string csFileName = "../shaders/brdf_lut_pass/brdf_lut.comp.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_brdfLutDescriptorSetLayout });
	m_brdfLutPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateComputePipeline(m_brdfLutPipelineLayout);
	m_vulkanManager.computePipelineAddShaderStage(csFileName);
	m_brdfLutPipeline = m_vulkanManager.endCreateComputePipeline();
}

void DeferredRenderer::createSpecEnvPrefilterPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_specEnvPrefilterPipelineLayout);
		m_vulkanManager.destroyPipeline(m_specEnvPrefilterPipeline);
	}

	const std::string vsFileName = "../shaders/env_prefilter_pass/env_prefilter.vert.spv";
	const std::string gsFileName = "../shaders/env_prefilter_pass/env_prefilter.geom.spv";
	const std::string fsFileName = "../shaders/env_prefilter_pass/spec_env_prefilter.frag.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_specEnvPrefilterDescriptorSetLayout });
	m_vulkanManager.pipelineLayoutAddPushConstantRange(0, sizeof(float), VK_SHADER_STAGE_FRAGMENT_BIT);
	m_specEnvPrefilterPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateGraphicsPipeline(m_specEnvPrefilterPipelineLayout, m_specEnvPrefilterRenderPass, 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, gsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

	auto bindingDesc = Vertex::getBindingDescription();
	m_vulkanManager.graphicsPipelineAddBindingDescription(bindingDesc.binding, bindingDesc.stride, bindingDesc.inputRate);
	auto attrDescs = Vertex::getAttributeDescriptions();
	for (const auto &attrDesc : attrDescs)
	{
		m_vulkanManager.graphicsPipelineAddAttributeDescription(attrDesc.location, attrDesc.binding, attrDesc.format, attrDesc.offset);
	}

	m_vulkanManager.graphicsPipelineConfigureRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_vulkanManager.graphicsPipelineAddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	m_vulkanManager.graphicsPipelineAddDynamicState(VK_DYNAMIC_STATE_SCISSOR);

	m_specEnvPrefilterPipeline = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createGeomPassPipeline()
{
	createSkyboxPipeline();
	createStaticMeshPipeline();
}

void DeferredRenderer::createSkyboxPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_skyboxPipelineLayout);
		m_vulkanManager.destroyPipeline(m_skyboxPipeline);
	}

	const std::string vsFileName = "../shaders/geom_pass/skybox.vert.spv";
	const std::string fsFileName = "../shaders/geom_pass/skybox.frag.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_skyboxDescriptorSetLayout });
	m_vulkanManager.pipelineLayoutAddPushConstantRange(0, sizeof(uint32_t), VK_SHADER_STAGE_FRAGMENT_BIT);
	m_skyboxPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateGraphicsPipeline(m_skyboxPipelineLayout, m_geomRenderPass, 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

	auto bindingDesc = Vertex::getBindingDescription();
	m_vulkanManager.graphicsPipelineAddBindingDescription(bindingDesc.binding, bindingDesc.stride, bindingDesc.inputRate);
	auto attrDescs = Vertex::getAttributeDescriptions();
	for (const auto &attrDesc : attrDescs)
	{
		m_vulkanManager.graphicsPipelineAddAttributeDescription(attrDesc.location, attrDesc.binding, attrDesc.format, attrDesc.offset);
	}

	m_vulkanManager.graphicsPipelineConfigureMultisampleState(SAMPLE_COUNT);

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

	// Clamp fragment depth to [0, 1] instead of clipping to avoid clipping the sky box
	m_vulkanManager.graphicsPipelineConfigureRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE,
		1.f, VK_FALSE, 0.f, 0.f, VK_TRUE);
	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);
	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);
	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_skyboxPipeline = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createStaticMeshPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_geomPipelineLayout);
		m_vulkanManager.destroyPipeline(m_geomPipeline);
	}

	const std::string vsFileName = "../shaders/geom_pass/geom.vert.spv";
	const std::string fsFileName = "../shaders/geom_pass/geom.frag.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_geomDescriptorSetLayout });
	m_vulkanManager.pipelineLayoutAddPushConstantRange(0, 3 * sizeof(uint32_t), VK_SHADER_STAGE_FRAGMENT_BIT);
	m_geomPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateGraphicsPipeline(m_geomPipelineLayout, m_geomRenderPass, 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

	auto bindingDesc = Vertex::getBindingDescription();
	m_vulkanManager.graphicsPipelineAddBindingDescription(bindingDesc.binding, bindingDesc.stride, bindingDesc.inputRate);
	auto attrDescs = Vertex::getAttributeDescriptions();
	for (const auto &attrDesc : attrDescs)
	{
		m_vulkanManager.graphicsPipelineAddAttributeDescription(attrDesc.location, attrDesc.binding, attrDesc.format, attrDesc.offset);
	}

#ifdef USE_GLTF
	// temporary hack
	m_vulkanManager.graphicsPipelineConfigureRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
#endif

	m_vulkanManager.graphicsPipelineConfigureMultisampleState(SAMPLE_COUNT, VK_TRUE, 0.25f);

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);
	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);
	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_geomPipeline = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createShadowPassPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_shadowPipelineLayout);
		for (auto p : m_shadowPipelines)
		{
			m_vulkanManager.destroyPipeline(p);
		}
	}

	const std::string vsFileName = "../shaders/shadow_pass/shadow.vert.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_shadowDescriptorSetLayout1, m_shadowDescriptorSetLayout2 });
	m_shadowPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_shadowPipelines.resize(m_camera.getSegmentCount());
	for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
	{
		m_vulkanManager.beginCreateGraphicsPipeline(m_shadowPipelineLayout, m_shadowRenderPass, i);

		m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);

		auto bindingDesc = Vertex::getBindingDescription();
		m_vulkanManager.graphicsPipelineAddBindingDescription(bindingDesc.binding, bindingDesc.stride, bindingDesc.inputRate);
		auto attrDescs = Vertex::getAttributeDescriptions();
		m_vulkanManager.graphicsPipelineAddAttributeDescription(attrDescs[0].location, attrDescs[0].binding, attrDescs[0].format, attrDescs[0].offset);

		VkExtent2D swapChainExtent = { SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
		m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
			static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

#ifdef USE_GLTF
		m_vulkanManager.graphicsPipelineConfigureRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
			1.f, VK_TRUE, 1.f, 1.f);
#else
		m_vulkanManager.graphicsPipelineConfigureRasterizer(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE,
			1.f, VK_TRUE, 1.f, 1.f);
#endif

		m_shadowPipelines[i] = m_vulkanManager.endCreateGraphicsPipeline();
	}
}

void DeferredRenderer::createLightingPassPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_lightingPipelineLayout);
		m_vulkanManager.destroyPipeline(m_lightingPipeline);
	}

	const std::string vsFileName = "../shaders/fullscreen.vert.spv";
	const std::string fsFileName = "../shaders/lighting_pass/lighting.frag.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_lightingDescriptorSetLayout });
	m_vulkanManager.pipelineLayoutAddPushConstantRange(0, 3 * sizeof(uint32_t), VK_SHADER_STAGE_FRAGMENT_BIT);
	m_lightingPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateGraphicsPipeline(m_lightingPipelineLayout, m_lightingRenderPass, 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

	// Use specialization constants to pass number of lights and samples to the shader
	uint32_t numLights = NUM_LIGHTS;
	uint32_t sampleCount = SAMPLE_COUNT;
	m_vulkanManager.graphicsPipelineAddSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 0, sizeof(uint32_t), &numLights);
	m_vulkanManager.graphicsPipelineAddSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1, sizeof(uint32_t), sizeof(uint32_t), &sampleCount);

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_lightingPipeline = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createBloomPipelines()
{
	if (m_initialized)
	{
		for (auto name : m_bloomPipelineLayouts)
		{
			m_vulkanManager.destroyPipelineLayout(name);
		}

		for (auto name : m_bloomPipelines)
		{
			m_vulkanManager.destroyPipeline(name);
		}
	}

	const std::string vsFileName = "../shaders/fullscreen.vert.spv";
	const std::string fsFileName1 = "../shaders/bloom_pass/brightness_mask.frag.spv";
	const std::string fsFileName2 = "../shaders/bloom_pass/gaussian_blur.frag.spv";
	const std::string fsFileName3 = "../shaders/bloom_pass/merge.frag.spv";

	// --- Pipeline layouts
	m_bloomPipelineLayouts.resize(2);

	// brightness_mask and merge
	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_bloomDescriptorSetLayout });
	m_bloomPipelineLayouts[0] = m_vulkanManager.endCreatePipelineLayout();

	// gaussian blur
	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_bloomDescriptorSetLayout });
	m_vulkanManager.pipelineLayoutAddPushConstantRange(0, sizeof(uint32_t), VK_SHADER_STAGE_FRAGMENT_BIT);
	m_bloomPipelineLayouts[1] = m_vulkanManager.endCreatePipelineLayout();

	// --- Pipelines
	m_bloomPipelines.resize(3);

	// brightness mask
	m_vulkanManager.beginCreateGraphicsPipeline(m_bloomPipelineLayouts[0], m_bloomRenderPasses[0], 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName1);

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width >> 1), static_cast<float>(swapChainExtent.height >> 1));

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_bloomPipelines[0] = m_vulkanManager.endCreateGraphicsPipeline();

	// gaussian blur
	m_vulkanManager.beginCreateGraphicsPipeline(m_bloomPipelineLayouts[1], m_bloomRenderPasses[0], 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName2);

	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width >> 1), static_cast<float>(swapChainExtent.height >> 1));

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_bloomPipelines[1] = m_vulkanManager.endCreateGraphicsPipeline();

	// merge
	m_vulkanManager.beginCreateGraphicsPipeline(m_bloomPipelineLayouts[0], m_bloomRenderPasses[1], 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName3);

	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD);

	m_bloomPipelines[2] = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createFinalOutputPassPipeline()
{
	if (m_initialized)
	{
		m_vulkanManager.destroyPipelineLayout(m_finalOutputPipelineLayout);
		m_vulkanManager.destroyPipeline(m_finalOutputPipeline);
	}

	const std::string vsFileName = "../shaders/fullscreen.vert.spv";
	const std::string fsFileName = "../shaders/final_output_pass/final_output.frag.spv";

	m_vulkanManager.beginCreatePipelineLayout();
	m_vulkanManager.pipelineLayoutAddDescriptorSetLayouts({ m_finalOutputDescriptorSetLayout });
	m_finalOutputPipelineLayout = m_vulkanManager.endCreatePipelineLayout();

	m_vulkanManager.beginCreateGraphicsPipeline(m_finalOutputPipelineLayout, m_finalOutputRenderPass, 0);

	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
	m_vulkanManager.graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

	VkExtent2D swapChainExtent = m_vulkanManager.getSwapChainExtent();
	m_vulkanManager.graphicsPipelineAddViewportAndScissor(0.f, 0.f,
		static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));

	m_vulkanManager.graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

	m_vulkanManager.graphicsPipeLineAddColorBlendAttachment(VK_FALSE);

	m_finalOutputPipeline = m_vulkanManager.endCreateGraphicsPipeline();
}

void DeferredRenderer::createBrdfLutDescriptorSet()
{
	if (m_bakedBrdfReady) return;

	std::vector<rj::DescriptorSetUpdateImageInfo> updateInfos(1);
	updateInfos[0].layout = VK_IMAGE_LAYOUT_GENERAL;
	updateInfos[0].imageViewName = m_bakedBRDFs[0].imageViews[0];
	updateInfos[0].samplerName = std::numeric_limits<uint32_t>::max();

	m_vulkanManager.beginUpdateDescriptorSet(m_brdfLutDescriptorSet);
	m_vulkanManager.descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, updateInfos);
	m_vulkanManager.endUpdateDescriptorSet();
}

void DeferredRenderer::createSpecEnvPrefilterDescriptorSet()
{
	if (m_scene.skybox.specMapReady) return;

	std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);
	bufferInfos[0].bufferName = m_oneTimeUniformDeviceData.buffer;
	bufferInfos[0].offset = m_oneTimeUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uCubeViews));
	bufferInfos[0].sizeInBytes = sizeof(CubeMapCameraUniformBuffer);

	std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);
	imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfos[0].imageViewName = m_scene.skybox.radianceMap.imageViews[0];
	imageInfos[0].samplerName = m_scene.skybox.radianceMap.samplers[0];

	m_vulkanManager.beginUpdateDescriptorSet(m_specEnvPrefilterDescriptorSet);
	m_vulkanManager.descriptorSetAddBufferDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);
	m_vulkanManager.descriptorSetAddImageDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);
	m_vulkanManager.endUpdateDescriptorSet();
}

void DeferredRenderer::createGeomPassDescriptorSets()
{
	createSkyboxDescriptorSet();
	createStaticMeshDescriptorSet();
}

void DeferredRenderer::createSkyboxDescriptorSet()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();

	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);
		bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
		bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uCameraVP));
		bufferInfos[0].sizeInBytes = sizeof(DisplayInfoUniformBuffer);

		std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);
		imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[0].imageViewName = m_scene.skybox.radianceMap.imageViews[0];
		imageInfos[0].samplerName = m_scene.skybox.radianceMap.samplers[0];

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_skyboxDescriptorSet);
		m_vulkanManager.descriptorSetAddBufferDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);
		m_vulkanManager.descriptorSetAddImageDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);
		m_vulkanManager.endUpdateDescriptorSet();
	}
}

void DeferredRenderer::createStaticMeshDescriptorSet()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);
			std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);

			m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_geomDescriptorSets[i]);

			bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
			bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uCameraVP));
			bufferInfos[0].sizeInBytes = sizeof(TransMatsUniformBuffer);
			m_vulkanManager.descriptorSetAddBufferDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

			bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_scene.meshes[i].uPerModelInfo));
			bufferInfos[0].sizeInBytes = sizeof(PerModelUniformBuffer);
			m_vulkanManager.descriptorSetAddBufferDescriptor(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

			imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[0].imageViewName = m_scene.meshes[i].albedoMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].albedoMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			imageInfos[0].imageViewName = m_scene.meshes[i].normalMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].normalMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			imageInfos[0].imageViewName = m_scene.meshes[i].roughnessMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].roughnessMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			imageInfos[0].imageViewName = m_scene.meshes[i].metalnessMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].metalnessMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			imageInfos[0].imageViewName = m_scene.meshes[i].aoMap.image == std::numeric_limits<uint32_t>::max() ? m_scene.meshes[i].albedoMap.imageViews[0] : m_scene.meshes[i].aoMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].aoMap.image == std::numeric_limits<uint32_t>::max() ? m_scene.meshes[i].albedoMap.samplers[0] : m_scene.meshes[i].aoMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			imageInfos[0].imageViewName = m_scene.meshes[i].emissiveMap.image == std::numeric_limits<uint32_t>::max() ? m_scene.meshes[i].albedoMap.imageViews[0] : m_scene.meshes[i].emissiveMap.imageViews[0];
			imageInfos[0].samplerName = m_scene.meshes[i].emissiveMap.image == std::numeric_limits<uint32_t>::max() ? m_scene.meshes[i].albedoMap.samplers[0] : m_scene.meshes[i].emissiveMap.samplers[0];
			m_vulkanManager.descriptorSetAddImageDescriptor(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

			m_vulkanManager.endUpdateDescriptorSet();
		}
	}
}

void DeferredRenderer::createShadowPassDescriptorSets()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);

			m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets1[i]);

			bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
			bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uShadowLightInfos[i]));
			bufferInfos[0].sizeInBytes = sizeof(ShadowLightUniformBuffer);
			m_vulkanManager.descriptorSetAddBufferDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

			m_vulkanManager.endUpdateDescriptorSet();
		}

		for (uint32_t i = 0; i < m_scene.meshes.size(); ++i)
		{
			std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);

			m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets2[i]);

			bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
			bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_scene.meshes[i].uPerModelInfo));
			bufferInfos[0].sizeInBytes = sizeof(PerModelUniformBuffer);
			m_vulkanManager.descriptorSetAddBufferDescriptor(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

			m_vulkanManager.endUpdateDescriptorSet();
		}
	}
}

void DeferredRenderer::createLightingPassDescriptorSets()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);
		std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_lightingDescriptorSet);

		bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
		bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uLightInfo));
		bufferInfos[0].sizeInBytes = sizeof(LightingPassUniformBuffer);
		m_vulkanManager.descriptorSetAddBufferDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

		imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[0].imageViewName = m_gbufferImages[0].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[0].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_gbufferImages[1].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[1].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_gbufferImages[2].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[2].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_depthImage.imageViews[0];
		imageInfos[0].samplerName = m_depthImage.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_scene.skybox.specularIrradianceMap.imageViews[0];
		imageInfos[0].samplerName = m_scene.skybox.specularIrradianceMap.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_bakedBRDFs[0].imageViews[0];
		imageInfos[0].samplerName = m_bakedBRDFs[0].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_shadowImage.imageViews.back();
		imageInfos[0].samplerName = m_shadowImage.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		m_vulkanManager.endUpdateDescriptorSet();
	}
}

void DeferredRenderer::createBloomDescriptorSets()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[0]);
		imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[0].imageViewName = m_lightingResultImage.imageViews[0];
		imageInfos[0].samplerName = m_lightingResultImage.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);
		m_vulkanManager.endUpdateDescriptorSet();

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[1]);
		imageInfos[0].imageViewName = m_postEffectImages[0].imageViews[0];
		imageInfos[0].samplerName = m_postEffectImages[0].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);
		m_vulkanManager.endUpdateDescriptorSet();

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[2]);
		imageInfos[0].imageViewName = m_postEffectImages[1].imageViews[0];
		imageInfos[0].samplerName = m_postEffectImages[1].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);
		m_vulkanManager.endUpdateDescriptorSet();
	}
}

void DeferredRenderer::createFinalOutputPassDescriptorSets()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		std::vector<rj::DescriptorSetUpdateBufferInfo> bufferInfos(1);
		std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);

		m_vulkanManager.beginUpdateDescriptorSet(m_perFrameDescriptorSets[imgIdx].m_finalOutputDescriptorSet);

		bufferInfos[0].bufferName = m_perFrameUniformDeviceData[imgIdx].buffer;
		bufferInfos[0].offset = m_perFrameUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uDisplayInfo));
		bufferInfos[0].sizeInBytes = sizeof(DisplayInfoUniformBuffer);
		m_vulkanManager.descriptorSetAddBufferDescriptor(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, bufferInfos);

		imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[0].imageViewName = m_lightingResultImage.imageViews[0];
		imageInfos[0].samplerName = m_lightingResultImage.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_gbufferImages[0].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[0].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_gbufferImages[1].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[1].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_gbufferImages[2].imageViews[0];
		imageInfos[0].samplerName = m_gbufferImages[2].samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		imageInfos[0].imageViewName = m_depthImage.imageViews[0];
		imageInfos[0].samplerName = m_depthImage.samplers[0];
		m_vulkanManager.descriptorSetAddImageDescriptor(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		m_vulkanManager.endUpdateDescriptorSet();
	}
}

void DeferredRenderer::createBrdfLutCommandBuffer()
{
	if (m_bakedBrdfReady) return;

	m_vulkanManager.queueWaitIdle(VK_QUEUE_COMPUTE_BIT);

	m_vulkanManager.beginCommandBuffer(m_brdfLutCommandBuffer);

	m_vulkanManager.cmdBindPipeline(m_brdfLutCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdfLutPipeline);
	m_vulkanManager.cmdBindDescriptorSets(m_brdfLutCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_brdfLutPipelineLayout, { m_brdfLutDescriptorSet });

	const uint32_t bsx = 16, bsy = 16;
	const uint32_t brdfLutSizeX = BRDF_LUT_SIZE, brdfLutSizeY = BRDF_LUT_SIZE;
	m_vulkanManager.cmdDispatch(m_brdfLutCommandBuffer, brdfLutSizeX / bsx, brdfLutSizeY / bsy, 1);

	m_vulkanManager.endCommandBuffer(m_brdfLutCommandBuffer);
}

void DeferredRenderer::createEnvPrefilterCommandBuffer()
{
	if (m_scene.skybox.specMapReady) return;

	m_vulkanManager.beginCommandBuffer(m_envPrefilterCommandBuffer);

	m_vulkanManager.cmdBindVertexBuffers(m_envPrefilterCommandBuffer, { m_scene.skybox.vertexBuffer.buffer }, { 0 });
	m_vulkanManager.cmdBindIndexBuffer(m_envPrefilterCommandBuffer, m_scene.skybox.indexBuffer.buffer, VK_INDEX_TYPE_UINT32);

	std::vector<VkClearValue> clearValues(1);
	clearValues[0].color = { { 0.f, 0.f, 0.f, 0.f } };
	const uint32_t numIndices = static_cast<uint32_t>(m_scene.skybox.indexBuffer.size / sizeof(uint32_t));

	// Specular prefitler pass
	uint32_t mipLevels = m_scene.skybox.specularIrradianceMap.mipLevelCount;
	float roughness = 0.f;
	float roughnessDelta = 1.f / static_cast<float>(mipLevels - 1);

	// Layered rendering allows render into multiple layers but only one mip level
	// In order to render all mips, multiple passes are required
	for (uint32_t level = 0; level < mipLevels; ++level)
	{
		m_vulkanManager.cmdBeginRenderPass(m_envPrefilterCommandBuffer, m_specEnvPrefilterRenderPass, m_specEnvPrefilterFramebuffers[level], clearValues);

		m_vulkanManager.cmdBindPipeline(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_specEnvPrefilterPipeline);

		m_vulkanManager.cmdBindDescriptorSets(m_envPrefilterCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_specEnvPrefilterPipelineLayout, { m_specEnvPrefilterDescriptorSet });
		m_vulkanManager.cmdPushConstants(m_envPrefilterCommandBuffer, m_specEnvPrefilterPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &roughness);

		m_vulkanManager.cmdSetViewport(m_envPrefilterCommandBuffer, m_specEnvPrefilterFramebuffers[level]);
		m_vulkanManager.cmdSetScissor(m_envPrefilterCommandBuffer, m_specEnvPrefilterFramebuffers[level]);

		m_vulkanManager.cmdDrawIndexed(m_envPrefilterCommandBuffer, numIndices);

		m_vulkanManager.cmdEndRenderPass(m_envPrefilterCommandBuffer);

		roughness += roughnessDelta;
	}

	m_vulkanManager.endCommandBuffer(m_envPrefilterCommandBuffer);
}

void DeferredRenderer::createGeomShadowLightingCommandBuffers()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		uint32_t cb = m_perFrameCommandBuffers[imgIdx].m_geomShadowLightingCommandBuffer;
		m_vulkanManager.beginCommandBuffer(cb, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

		std::vector<VkClearValue> clearValues(4);
		clearValues[0].depthStencil = { 1.0f, 0 };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 1
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 2
		clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // g-buffer 3
		m_vulkanManager.cmdBeginRenderPass(cb, m_geomRenderPass, m_geomFramebuffer, clearValues);

		// Geometry pass
		{
			m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline);

			m_vulkanManager.cmdBindVertexBuffers(cb, { m_scene.skybox.vertexBuffer.buffer }, { 0 });
			m_vulkanManager.cmdBindIndexBuffer(cb, m_scene.skybox.indexBuffer.buffer, VK_INDEX_TYPE_UINT32);

			m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_skyboxPipelineLayout, { m_perFrameDescriptorSets[imgIdx].m_skyboxDescriptorSet });
			m_vulkanManager.cmdPushConstants(cb, m_skyboxPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &m_scene.skybox.materialType);

			const uint32_t numIndices = static_cast<uint32_t>(m_scene.skybox.indexBuffer.size / sizeof(uint32_t));
			m_vulkanManager.cmdDrawIndexed(cb, numIndices);
		}

		m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipeline);

		const uint32_t numModels = static_cast<uint32_t>(m_scene.meshes.size());
		for (uint32_t j = 0; j < numModels; ++j)
		{
			m_vulkanManager.cmdBindVertexBuffers(cb, { m_scene.meshes[j].vertexBuffer.buffer }, { 0 });
			m_vulkanManager.cmdBindIndexBuffer(cb, m_scene.meshes[j].indexBuffer.buffer, VK_INDEX_TYPE_UINT32);

			m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_geomPipelineLayout, { m_perFrameDescriptorSets[imgIdx].m_geomDescriptorSets[j] });

			struct
			{
				uint32_t materialId;
				uint32_t hasAoMap;
				uint32_t hasEmissiveMap;
			} pushConst;
			pushConst.materialId = m_scene.meshes[j].materialType;
			pushConst.hasAoMap = m_scene.meshes[j].aoMap.image != std::numeric_limits<uint32_t>::max();
			pushConst.hasEmissiveMap = m_scene.meshes[j].emissiveMap.image != std::numeric_limits<uint32_t>::max();

			m_vulkanManager.cmdPushConstants(cb, m_geomPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConst), &pushConst);

			const uint32_t numIndices = static_cast<uint32_t>(m_scene.meshes[j].indexBuffer.size / sizeof(uint32_t));
			m_vulkanManager.cmdDrawIndexed(cb, numIndices);
		}

		m_vulkanManager.cmdEndRenderPass(cb);

		// Shadow pass
		clearValues.resize(m_camera.getSegmentCount());
		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i) clearValues[i].depthStencil = { 1.f, 0 };
		m_vulkanManager.cmdBeginRenderPass(cb, m_shadowRenderPass, m_shadowFramebuffer, clearValues);

		for (uint32_t i = 0; i < m_camera.getSegmentCount(); ++i)
		{
			if (i > 0) m_vulkanManager.cmdNextSubpass(cb);

			m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelines[i]);

			for (uint32_t j = 0; j < numModels; ++j)
			{
				m_vulkanManager.cmdBindVertexBuffers(cb, { m_scene.meshes[j].vertexBuffer.buffer }, { 0 });
				m_vulkanManager.cmdBindIndexBuffer(cb, m_scene.meshes[j].indexBuffer.buffer, VK_INDEX_TYPE_UINT32);

				m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipelineLayout,
					{ m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets1[i], m_perFrameDescriptorSets[imgIdx].m_shadowDescriptorSets2[j] });

				const uint32_t numIndices = static_cast<uint32_t>(m_scene.meshes[j].indexBuffer.size / sizeof(uint32_t));
				m_vulkanManager.cmdDrawIndexed(cb, numIndices);
			}
		}

		m_vulkanManager.cmdEndRenderPass(cb);

		// Lighting pass
		clearValues.resize(1);
		clearValues[0].color = { { 0.f, 0.f, 0.f, 0.f } };
		m_vulkanManager.cmdBeginRenderPass(cb, m_lightingRenderPass, m_lightingFramebuffer, clearValues);

		m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline);
		m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_lightingPipelineLayout, { m_perFrameDescriptorSets[imgIdx].m_lightingDescriptorSet });

		struct
		{
			uint32_t specIrradianceMapMipCount;
			int32_t frustumSegmentCount;
			int32_t pcfKernelSize;
		} pushConst;
		pushConst.specIrradianceMapMipCount = m_scene.skybox.specularIrradianceMap.mipLevelCount;
		pushConst.frustumSegmentCount = m_camera.getSegmentCount();
		pushConst.pcfKernelSize = m_scene.shadowLight.getPCFKernlSize();
		m_vulkanManager.cmdPushConstants(cb, m_lightingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConst), &pushConst);

		m_vulkanManager.cmdDraw(cb, 3);

		m_vulkanManager.cmdEndRenderPass(cb);

		m_vulkanManager.endCommandBuffer(cb);
	}
}

void DeferredRenderer::createPostEffectCommandBuffers()
{
	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		uint32_t cb = m_perFrameCommandBuffers[imgIdx].m_postEffectCommandBuffer;
		m_vulkanManager.beginCommandBuffer(cb, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

		// brightness mask
		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		m_vulkanManager.cmdBeginRenderPass(cb, m_bloomRenderPasses[0], m_postEffectFramebuffers[0], clearValues);

		m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipelines[0]);
		m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_bloomPipelineLayouts[0], { m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[0] });

		m_vulkanManager.cmdDraw(cb, 3);

		m_vulkanManager.cmdEndRenderPass(cb);

		// gaussian blur
		const uint32_t bloomPassCount = 1;
		for (uint32_t i = 0; i < bloomPassCount; ++i)
		{
			// horizontal
			m_vulkanManager.cmdBeginRenderPass(cb, m_bloomRenderPasses[0], m_postEffectFramebuffers[1], clearValues);

			m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipelines[1]);
			m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_bloomPipelineLayouts[1], { m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[1] });

			uint32_t isHorizontal = VK_TRUE;
			m_vulkanManager.cmdPushConstants(cb, m_bloomPipelineLayouts[1], VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &isHorizontal);

			m_vulkanManager.cmdDraw(cb, 3);

			m_vulkanManager.cmdEndRenderPass(cb);

			// vertical
			m_vulkanManager.cmdBeginRenderPass(cb, m_bloomRenderPasses[0], m_postEffectFramebuffers[0], clearValues);

			m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipelines[1]);
			m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_bloomPipelineLayouts[1], { m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[2] });

			isHorizontal = VK_FALSE;
			m_vulkanManager.cmdPushConstants(cb, m_bloomPipelineLayouts[1], VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &isHorizontal);

			m_vulkanManager.cmdDraw(cb, 3);

			m_vulkanManager.cmdEndRenderPass(cb);
		}

		// merge
		m_vulkanManager.cmdBeginRenderPass(cb, m_bloomRenderPasses[1], m_postEffectFramebuffers[2], {});

		m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomPipelines[2]);
		m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_bloomPipelineLayouts[0], { m_perFrameDescriptorSets[imgIdx].m_bloomDescriptorSets[1] });

		m_vulkanManager.cmdDraw(cb, 3);

		m_vulkanManager.cmdEndRenderPass(cb);

		m_vulkanManager.endCommandBuffer(cb);
	}
}

void DeferredRenderer::createPresentCommandBuffers()
{
	// Final ouput pass
	std::vector<VkClearValue> clearValues(1);
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

	const uint32_t swapChainImageCount = m_vulkanManager.getSwapChainSize();
	for (uint32_t imgIdx = 0; imgIdx < swapChainImageCount; ++imgIdx)
	{
		uint32_t cb = m_perFrameCommandBuffers[imgIdx].m_presentCommandBuffer;
		m_vulkanManager.beginCommandBuffer(cb, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

		m_vulkanManager.cmdBeginRenderPass(cb, m_finalOutputRenderPass, m_finalOutputFramebuffers[imgIdx], clearValues);

		m_vulkanManager.cmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_finalOutputPipeline);
		m_vulkanManager.cmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_finalOutputPipelineLayout, { m_perFrameDescriptorSets[imgIdx].m_finalOutputDescriptorSet });

		m_vulkanManager.cmdDraw(cb, 3);

		m_vulkanManager.cmdEndRenderPass(cb);

		m_vulkanManager.endCommandBuffer(cb);
	}
}

void DeferredRenderer::prefilterEnvironmentAndComputeBrdfLut()
{
	// References:
	//   [1] http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
	//   [2] https://github.com/derkreature/IBLBaker

	// Set up cube map camera
	m_uCubeViews->V[0] = glm::lookAt(glm::vec3(0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));	// +X
	m_uCubeViews->V[1] = glm::lookAt(glm::vec3(0.f), glm::vec3(-1.f, 0.f, 0.f), glm::vec3(0.f, -1.f, 0.f));	// -X
	m_uCubeViews->V[2] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec3(0.f, 0.f, 1.f));	// +Y
	m_uCubeViews->V[3] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, -1.f, 0.f), glm::vec3(0.f, 0.f, -1.f));	// -Y
	m_uCubeViews->V[4] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, -1.f, 0.f));	// +Z
	m_uCubeViews->V[5] = glm::lookAt(glm::vec3(0.f), glm::vec3(0.f, 0.f, -1.f), glm::vec3(0.f, -1.f, 0.f));	// -Z
	m_uCubeViews->P = glm::perspective(glm::radians(90.f), 1.f, 0.1f, 100.f);

	void *mapped = m_vulkanManager.mapBuffer(m_oneTimeUniformDeviceData.buffer,
		m_oneTimeUniformHostData.offsetOf(reinterpret_cast<const char *>(m_uCubeViews)), sizeof(CubeMapCameraUniformBuffer));
	memcpy(mapped, m_uCubeViews, sizeof(CubeMapCameraUniformBuffer));
	m_vulkanManager.unmapBuffer(m_oneTimeUniformDeviceData.buffer);

	// Bake BRDF terms
	std::vector<uint32_t> fences;

	if (!m_bakedBrdfReady)
	{
		m_vulkanManager.beginQueueSubmit(VK_QUEUE_COMPUTE_BIT);
		m_vulkanManager.queueSubmitNewSubmit({ m_brdfLutCommandBuffer });
		m_vulkanManager.endQueueSubmit(m_brdfLutFence, false);

		fences.push_back(m_brdfLutFence);
	}

	// Prefilter radiance map
	if (!m_scene.skybox.specMapReady)
	{
		m_vulkanManager.beginQueueSubmit(VK_QUEUE_GRAPHICS_BIT);
		m_vulkanManager.queueSubmitNewSubmit({ m_envPrefilterCommandBuffer });
		m_vulkanManager.endQueueSubmit(m_envPrefilterFence, false);

		fences.push_back(m_envPrefilterFence);
	}

	if (fences.size() > 0)
	{
		m_vulkanManager.waitForFences(fences);
		m_vulkanManager.resetFences(fences);

		if (!m_bakedBrdfReady)
		{
			m_vulkanManager.transitionImageLayout(m_bakedBRDFs[0].image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			m_bakedBrdfReady = true;
		}

		if (!m_scene.skybox.specMapReady)
		{
			m_vulkanManager.transitionImageLayout(m_scene.skybox.specularIrradianceMap.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			m_scene.skybox.specMapReady = true;
		}
	}
}

void DeferredRenderer::savePrecomputationResults()
{
	// read back computation results and save to disk
	if (m_shouldSaveBakedBrdf)
	{
		std::vector<char> hostData;
		m_vulkanManager.readImage(hostData, m_bakedBRDFs[0].image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		rj::helper_functions::saveImage2D(BRDF_BASE_DIR BRDF_NAME,
			BRDF_LUT_SIZE, BRDF_LUT_SIZE, sizeof(glm::vec2), 1, gli::FORMAT_RG32_SFLOAT_PACK32, hostData.data());
	}

	if (m_scene.skybox.shouldSaveSpecMap)
	{
		std::vector<char> hostData;
		m_vulkanManager.readImage(hostData, m_scene.skybox.specularIrradianceMap.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		rj::helper_functions::saveImageCube(PROBE_BASE_DIR "Specular_HDR.dds",
			SPEC_IRRADIANCE_MAP_SIZE, SPEC_IRRADIANCE_MAP_SIZE, sizeof(glm::vec4),
			m_scene.skybox.specularIrradianceMap.mipLevelCount, gli::FORMAT_RGBA32_SFLOAT_PACK32, hostData.data());
	}

	m_vulkanManager.deviceWaitIdle();
}

VkFormat DeferredRenderer::findDepthFormat()
{
	return m_vulkanManager.chooseSupportedFormatFromCandidates(
	{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}