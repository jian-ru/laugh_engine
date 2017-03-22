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
#include "directional_light.h"


#define BRDF_LUT_SIZE					256
#define ONE_TIME_UNIFORM_BLOB_SIZE		1024
#define PER_FRAME_UNIFORM_BLOB_SIZE		(64 * 1024)
#define NUM_LIGHTS						1
#define MAX_SHADOW_LIGHT_COUNT			2
#define SAMPLE_COUNT					VK_SAMPLE_COUNT_4_BIT

#define BRDF_BASE_DIR					"../textures/BRDF_LUTs/"
#define BRDF_NAME						"FSchlick_DGGX_GSmith.dds"

#define PROBE_BASE_DIR					"../textures/Environment/PaperMill/"
 //#define PROBE_BASE_DIR					"../textures/Environment/Factory/"
 //#define PROBE_BASE_DIR					"../textures/Environment/MonValley/"
 //#define PROBE_BASE_DIR					"../textures/Environment/Canyon/"

//#define USE_GLTF

#ifdef USE_GLTF
//#define GLTF_2_0
extern std::string GLTF_VERSION;
extern std::string GLTF_NAME;
#else
//#define MODEL_NAMES						{ "Cerberus" }
//#define MODEL_NAMES						{ "Jeep_Wagoneer" }
//#define MODEL_NAMES						{ "9mm_Pistol" }
#define MODEL_NAMES						{ "Drone_Body", "Drone_Legs", "Floor" }
//#define MODEL_NAMES						{ "Combat_Helmet" }
//#define MODEL_NAMES						{ "Bug_Ship" }
//#define MODEL_NAMES						{ "Knight_Base", "Knight_Helmet", "Knight_Chainmail", "Knight_Skirt", "Knight_Sword", "Knight_Armor" }
#endif


struct CubeMapCameraUniformBuffer
{
	glm::mat4 V[6];
	glm::mat4 P;
};

struct TransMatsUniformBuffer
{
	glm::mat4 VP;
};

struct ShadowLightUniformBuffer
{
	glm::mat4 lightVPC;
};

struct DiracLight
{
	glm::vec3 posOrDir;
	int lightVPCsIdx;
	glm::vec3 color;
	float radius;
};

// due to std140 padding for uniform buffer object
// only use data types that are vec4 or multiple of vec4's
struct LightingPassUniformBuffer
{
	glm::vec3 eyeWorldPos;
	float emissiveStrength;
	glm::vec4 diffuseSHCoefficients[9];
	glm::vec4 normFarPlaneZs;
	glm::mat4 lightVPCs[CSM_MAX_SEG_COUNT * MAX_SHADOW_LIGHT_COUNT];
	DiracLight diracLights[NUM_LIGHTS];
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
		m_verNumMajor = 0;
		m_verNumMinor = 1;

		m_windowTitle = "Laugh Engine";
		m_vulkanManager.windowSetTitle(m_windowTitle);

		VkPhysicalDeviceProperties props;
		m_vulkanManager.getPhysicalDeviceProperties(&props);
		m_oneTimeUniformHostData.setAlignment(props.limits.minUniformBufferOffsetAlignment);
		m_perFrameUniformHostData.setAlignment(props.limits.minUniformBufferOffsetAlignment);
	}

	virtual void run();

protected:
	uint32_t m_specEnvPrefilterRenderPass;
	uint32_t m_shadowRenderPass;
	uint32_t m_geomRenderPass;
	uint32_t m_lightingRenderPass;
	std::vector<uint32_t> m_bloomRenderPasses;
	uint32_t m_finalOutputRenderPass;

	uint32_t m_brdfLutDescriptorSetLayout;
	uint32_t m_specEnvPrefilterDescriptorSetLayout;
	uint32_t m_skyboxDescriptorSetLayout;
	uint32_t m_geomDescriptorSetLayout;
	uint32_t m_shadowDescriptorSetLayout1; // per segment
	uint32_t m_shadowDescriptorSetLayout2; // per model
	uint32_t m_lightingDescriptorSetLayout;
	uint32_t m_bloomDescriptorSetLayout;
	uint32_t m_finalOutputDescriptorSetLayout;

	uint32_t m_brdfLutPipelineLayout;
	uint32_t m_specEnvPrefilterPipelineLayout;
	uint32_t m_skyboxPipelineLayout;
	uint32_t m_geomPipelineLayout;
	uint32_t m_shadowPipelineLayout;
	uint32_t m_lightingPipelineLayout;
	std::vector<uint32_t> m_bloomPipelineLayouts;
	uint32_t m_finalOutputPipelineLayout;

	uint32_t m_brdfLutPipeline;
	uint32_t m_specEnvPrefilterPipeline;
	uint32_t m_skyboxPipeline;
	uint32_t m_geomPipeline;
	std::vector<uint32_t> m_shadowPipelines;
	uint32_t m_lightingPipeline;
	std::vector<uint32_t> m_bloomPipelines;
	uint32_t m_finalOutputPipeline;

	rj::helper_functions::ImageWrapper m_depthImage;
	rj::helper_functions::ImageWrapper m_shadowImage;

	const VkFormat m_lightingResultImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	rj::helper_functions::ImageWrapper m_lightingResultImage; // VK_FORMAT_R16G16B16A16_SFLOAT
	const uint32_t m_numGBuffers = 3;
	const std::vector<VkFormat> m_gbufferFormats =
	{
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R8G8B8A8_UNORM
	};
	std::vector<rj::helper_functions::ImageWrapper> m_gbufferImages; // GB1: VK_FORMAT_R32G32B32A32_SFLOAT, GB2: VK_FORMAT_R32G32B32A32_SFLOAT, GB3: VK_FORMAT_R8G8B8A8_UNORM
	const uint32_t m_numPostEffectImages = 2;
	const std::vector<VkFormat> m_postEffectImageFormats =
	{
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R16G16B16A16_SFLOAT
	};
	std::vector<rj::helper_functions::ImageWrapper> m_postEffectImages; // Image1: VK_FORMAT_R16G16B16A16_SFLOAT, Image2: VK_FORMAT_R16G16B16A16_SFLOAT

	rj::helper_functions::UniformBlob<ONE_TIME_UNIFORM_BLOB_SIZE> m_oneTimeUniformHostData;
	rj::helper_functions::UniformBlob<PER_FRAME_UNIFORM_BLOB_SIZE> m_perFrameUniformHostData;
	CubeMapCameraUniformBuffer *m_uCubeViews = nullptr;
	TransMatsUniformBuffer *m_uTransMats = nullptr;
	std::vector<ShadowLightUniformBuffer *> m_uShadowLightInfos;
	LightingPassUniformBuffer *m_uLightInfo = nullptr;
	DisplayInfoUniformBuffer *m_uDisplayInfo = nullptr;
	rj::helper_functions::BufferWrapper m_oneTimeUniformDeviceData;
	std::vector<rj::helper_functions::BufferWrapper> m_perFrameUniformDeviceData;

	uint32_t m_brdfLutDescriptorSet;
	uint32_t m_specEnvPrefilterDescriptorSet;
	typedef struct
	{
		uint32_t m_skyboxDescriptorSet;
		std::vector<uint32_t> m_geomDescriptorSets; // one set per model
		std::vector<uint32_t> m_shadowDescriptorSets1; // one set per segment
		std::vector<uint32_t> m_shadowDescriptorSets2; // one per model
		uint32_t m_lightingDescriptorSet;
		std::vector<uint32_t> m_bloomDescriptorSets;
		uint32_t m_finalOutputDescriptorSet;
	} PerFrameDescriptorSets;
	std::vector<PerFrameDescriptorSets> m_perFrameDescriptorSets;

	std::vector<uint32_t> m_specEnvPrefilterFramebuffers;
	uint32_t m_geomFramebuffer;
	uint32_t m_shadowFramebuffer;
	uint32_t m_lightingFramebuffer;
	std::vector<uint32_t> m_postEffectFramebuffers;
	std::vector<uint32_t> m_finalOutputFramebuffers; // present framebuffer names

	uint32_t m_imageAvailableSemaphore;
	uint32_t m_geomAndLightingCompleteSemaphore;
	uint32_t m_postEffectSemaphore;
	uint32_t m_finalOutputFinishedSemaphore;
	uint32_t m_renderFinishedSemaphore;

	uint32_t m_brdfLutFence;
	uint32_t m_envPrefilterFence;
	uint32_t m_renderFinishedFence;

	uint32_t m_brdfLutCommandBuffer;
	uint32_t m_envPrefilterCommandBuffer;
	typedef struct
	{
		uint32_t m_geomShadowLightingCommandBuffer;
		uint32_t m_postEffectCommandBuffer;
		uint32_t m_presentCommandBuffer;
	} PerFrameCommandBuffers;
	std::vector<PerFrameCommandBuffers> m_perFrameCommandBuffers;

	DirectionalLight m_shadowLight{ glm::vec3(1.f, 1.f, 1.f), glm::vec3(-1.f, -1.f, -1.f), glm::vec3(2.f) };


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

	virtual void updateUniformHostData();
	virtual void updateUniformDeviceData(uint32_t imgIdx);
	virtual void drawFrame();

	// Helpers
	virtual void createSpecEnvPrefilterRenderPass();
	virtual void createGeometryRenderPass();
	virtual void createShadowRenderPass();
	virtual void createLightingRenderPass();
	virtual void createBloomRenderPasses();
	virtual void createFinalOutputRenderPass();

	virtual void createBrdfLutDescriptorSetLayout();
	virtual void createSpecEnvPrefilterDescriptorSetLayout();
	virtual void createSkyboxDescriptorSetLayout();
	virtual void createStaticMeshDescriptorSetLayout();
	virtual void createGeomPassDescriptorSetLayout();
	virtual void createShadowPassDescriptorSetLayout();
	virtual void createLightingPassDescriptorSetLayout();
	virtual void createBloomDescriptorSetLayout();
	virtual void createFinalOutputDescriptorSetLayout();

	virtual void createBrdfLutPipeline();
	virtual void createSpecEnvPrefilterPipeline();
	virtual void createSkyboxPipeline();
	virtual void createStaticMeshPipeline();
	virtual void createGeomPassPipeline();
	virtual void createShadowPassPipeline();
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
	virtual void createShadowPassDescriptorSets();
	virtual void createLightingPassDescriptorSets();
	virtual void createBloomDescriptorSets();
	virtual void createFinalOutputPassDescriptorSets();

	virtual void createBrdfLutCommandBuffer();
	virtual void createEnvPrefilterCommandBuffer();
	virtual void createGeomShadowLightingCommandBuffers();
	virtual void createPostEffectCommandBuffers();
	virtual void createPresentCommandBuffers();

	virtual void prefilterEnvironmentAndComputeBrdfLut();
	virtual void savePrecomputationResults();

	virtual VkFormat findDepthFormat();
};

