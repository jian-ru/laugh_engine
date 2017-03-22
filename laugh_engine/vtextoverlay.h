#pragma once

#include <vulkan/vulkan.h>

#include "VManager.h"

#include "stb_font_consolas_24_latin1.inl"


// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT 
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define MAX_CHAR_COUNT 2048


class VTextOverlay
{
public:
	enum TextAlign { alignLeft, alignCenter, alignRight };


	VTextOverlay(rj::VManager *manager) :
		pManager(manager)
	{}

	// load in font descriptors and 
	void prepareResources()
	{
		createCommandPools();
		createFontTexture();
		createVertexBuffer();
		createDescriptorPoolAndSetLayouts();
		createDescriptorSets();
		createRenderPasses();
		createPipelines();
	}

	void beginTextUpdate()
	{
		mapped = reinterpret_cast<glm::vec4 *>(pManager->mapBuffer(fontQuadVertexBuffer.buffer));
		numLetters = 0;
	}

	void addText(std::string text, float x, float y, TextAlign align = alignLeft)
	{
		assert(mapped != nullptr);

		auto swapChainExtent = pManager->getSwapChainExtent();
		float fbW = static_cast<float>(swapChainExtent.width);
		float fbH = static_cast<float>(swapChainExtent.height);
		
		const float charW = 1.5f / fbW;
		const float charH = 1.5f / fbH;

		x = (x / fbW * 2.0f) - 1.0f;
		y = (y / fbH * 2.0f) - 1.0f;

		// Calculate text width
		float textWidth = 0;
		for (auto letter : text)
		{
			stb_fontchar *charData = &fontDescriptors[(uint32_t)letter - STB_FIRST_CHAR];
			textWidth += charData->advance * charW;
		}

		switch (align)
		{
		case alignRight:
			x -= textWidth;
			break;
		case alignCenter:
			x -= textWidth / 2.0f;
			break;
		}

		// Generate a uv mapped quad per char in the new text
		for (auto letter : text)
		{
			stb_fontchar *charData = &fontDescriptors[(uint32_t)letter - STB_FIRST_CHAR];

			mapped->x = (x + (float)charData->x0 * charW);
			mapped->y = (y + (float)charData->y0 * charH);
			mapped->z = charData->s0;
			mapped->w = charData->t0;
			mapped++;

			mapped->x = (x + (float)charData->x1 * charW);
			mapped->y = (y + (float)charData->y0 * charH);
			mapped->z = charData->s1;
			mapped->w = charData->t0;
			mapped++;

			mapped->x = (x + (float)charData->x0 * charW);
			mapped->y = (y + (float)charData->y1 * charH);
			mapped->z = charData->s0;
			mapped->w = charData->t1;
			mapped++;

			mapped->x = (x + (float)charData->x1 * charW);
			mapped->y = (y + (float)charData->y1 * charH);
			mapped->z = charData->s1;
			mapped->w = charData->t1;
			mapped++;

			x += charData->advance * charW;

			numLetters++;
		}
	}

	void endTextUpdate(uint32_t imageIdx)
	{
		pManager->unmapBuffer(fontQuadVertexBuffer.buffer);
		mapped = nullptr;
		updateCommandBuffers(imageIdx);
	}

	void updateCommandBuffers(uint32_t imageIdx)
	{
		pManager->beginCommandBuffer(commandBuffers[imageIdx]);

		auto framebuffers = pManager->getSwapChainFramebuffers();
		pManager->cmdBeginRenderPass(commandBuffers[imageIdx], renderPass, framebuffers[imageIdx], {});

		pManager->cmdBindPipeline(commandBuffers[imageIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		pManager->cmdSetViewport(commandBuffers[imageIdx], framebuffers[imageIdx]);
		pManager->cmdSetScissor(commandBuffers[imageIdx], framebuffers[imageIdx]);

		pManager->cmdBindDescriptorSets(commandBuffers[imageIdx], VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout, { descriptorSet });

		pManager->cmdBindVertexBuffers(commandBuffers[imageIdx], { fontQuadVertexBuffer.buffer }, { 0 });

		for (uint32_t j = 0; j < numLetters; j++)
		{
			pManager->cmdDraw(commandBuffers[imageIdx], 4, 1, j * 4, 0);
		}

		pManager->cmdEndRenderPass(commandBuffers[imageIdx]);

		pManager->endCommandBuffer(commandBuffers[imageIdx]);
	}

	void submit(
		uint32_t bufferindex, 
		const std::vector<uint32_t> &waitSemaphores,
		const std::vector<VkPipelineStageFlags> &waitStages, 
		const std::vector<uint32_t> &signalSemaphores,
		uint32_t fence = std::numeric_limits<uint32_t>::max(),
		bool waitFence = false) const
	{
		pManager->beginQueueSubmit(VK_QUEUE_GRAPHICS_BIT);
		pManager->queueSubmitNewSubmit({ commandBuffers[bufferindex] }, waitSemaphores, waitStages, signalSemaphores);
		pManager->endQueueSubmit(fence, waitFence);
	}

private:
	rj::VManager *pManager;

	uint32_t commandPool;

	const VkFormat fontDeviceTextureFormat = VK_FORMAT_R8_UNORM;
	rj::helper_functions::ImageWrapper fontDeviceTexture;
	rj::helper_functions::BufferWrapper fontQuadVertexBuffer;
	uint32_t descriptorSetLayout;
	uint32_t descriptorPool;
	uint32_t descriptorSet;
	uint32_t renderPass;
	uint32_t pipelineLayout;
	uint32_t pipeline;
	std::vector<uint32_t> commandBuffers;

	stb_fontchar fontDescriptors[STB_NUM_CHARS]; // meta info like x/y offsets to upper left corner of a character window. s/t texture coordinates
	glm::vec4 *mapped = nullptr;
	uint32_t numLetters;


	struct TextQuadVertex
	{
		glm::vec4 xyst;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(TextQuadVertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDesc;
		}

		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2, {});

			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[0].offset = 0;

			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[1].offset = sizeof(glm::vec2);

			return attributeDescriptions;
		}
	};

	void createCommandPools()
	{
		// Each command buffer can be reset individually
		commandPool = pManager->createCommandPool(VK_QUEUE_GRAPHICS_BIT, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		uint32_t count = pManager->getSwapChainSize();
		commandBuffers = pManager->allocateCommandBuffers(commandPool, count);
	}

	void createFontTexture()
	{
		unsigned char fontTexture[STB_FONT_HEIGHT][STB_FONT_WIDTH]; // a texture atlas containing all the characters

		STB_FONT_NAME(fontDescriptors, fontTexture, STB_FONT_HEIGHT);

		fontDeviceTexture.format = fontDeviceTextureFormat;
		fontDeviceTexture.width = STB_FONT_WIDTH;
		fontDeviceTexture.height = STB_FONT_HEIGHT;
		fontDeviceTexture.depth = 1;
		fontDeviceTexture.mipLevelCount = 1;
		fontDeviceTexture.layerCount = 1;

		// create device local image
		fontDeviceTexture.image = pManager->createImage2D(STB_FONT_WIDTH, STB_FONT_HEIGHT, fontDeviceTextureFormat,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		pManager->transferHostDataToImage(fontDeviceTexture.image, STB_FONT_HEIGHT * STB_FONT_WIDTH * sizeof(unsigned char),
			fontTexture, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// create image view
		fontDeviceTexture.imageViews.resize(1);
		fontDeviceTexture.imageViews[0] = pManager->createImageView2D(fontDeviceTexture.image, VK_IMAGE_ASPECT_COLOR_BIT);

		// create sampler
		fontDeviceTexture.samplers.resize(1);
		fontDeviceTexture.samplers[0] = pManager->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	}

	void createVertexBuffer()
	{
		fontQuadVertexBuffer.offset = 0;
		fontQuadVertexBuffer.size = MAX_CHAR_COUNT * sizeof(glm::vec4); // (x, y, s, t)
		
		fontQuadVertexBuffer.buffer = pManager->createBuffer(fontQuadVertexBuffer.size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	void createDescriptorPoolAndSetLayouts()
	{
		// descriptor set layout
		pManager->beginCreateDescriptorSetLayout();
		pManager->setLayoutAddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		descriptorSetLayout = pManager->endCreateDescriptorSetLayout();

		// descriptor pool
		pManager->beginCreateDescriptorPool(1);
		pManager->descriptorPoolAddDescriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1);
		descriptorPool = pManager->endCreateDescriptorPool();
	}

	void createDescriptorSets()
	{
		auto sets = pManager->allocateDescriptorSets(descriptorPool, { descriptorSetLayout });
		descriptorSet = sets[0];

		pManager->beginUpdateDescriptorSet(descriptorSet);

		std::vector<rj::DescriptorSetUpdateImageInfo> imageInfos(1);
		imageInfos[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfos[0].imageViewName = fontDeviceTexture.imageViews[0];
		imageInfos[0].samplerName = fontDeviceTexture.samplers[0];
		
		pManager->descriptorSetAddImageDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageInfos);

		pManager->endUpdateDescriptorSet();
	}

	void createRenderPasses()
	{
		pManager->beginCreateRenderPass();

		auto swapChainFormat = pManager->getSwapChainImageFormat();
		pManager->renderPassAddAttachment(swapChainFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD);

		pManager->beginDescribeSubpass();
		pManager->subpassAddColorAttachmentReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		pManager->endDescribeSubpass();

		pManager->renderPassAddSubpassDependency(VK_SUBPASS_EXTERNAL, 0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_DEPENDENCY_BY_REGION_BIT);

		pManager->renderPassAddSubpassDependency(0, VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_MEMORY_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT);

		renderPass = pManager->endCreateRenderPass();
	}

	void createPipelines()
	{
		const std::string vsFileName = "../shaders/text_pass/text.vert.spv";
		const std::string fsFileName = "../shaders/text_pass/text.frag.spv";

		pManager->beginCreatePipelineLayout();
		pManager->pipelineLayoutAddDescriptorSetLayouts({ descriptorSetLayout });
		pipelineLayout = pManager->endCreatePipelineLayout();

		pManager->beginCreateGraphicsPipeline(pipelineLayout, renderPass, 0);

		pManager->graphicsPipelineAddShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vsFileName);
		pManager->graphicsPipelineAddShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fsFileName);

		auto bindingDesc = TextQuadVertex::getBindingDescription();
		pManager->graphicsPipelineAddBindingDescription(bindingDesc.binding, bindingDesc.stride, bindingDesc.inputRate);
		auto attrDescs = TextQuadVertex::getAttributeDescriptions();
		for (const auto &attrDesc : attrDescs)
		{
			pManager->graphicsPipelineAddAttributeDescription(attrDesc.location, attrDesc.binding, attrDesc.format, attrDesc.offset);
		}

		pManager->graphicsPipelineConfigureInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

		pManager->graphicsPipeLineAddColorBlendAttachment(VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD);

		pManager->graphicsPipelineConfigureDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

		pManager->graphicsPipelineAddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
		pManager->graphicsPipelineAddDynamicState(VK_DYNAMIC_STATE_SCISSOR);

		pipeline = pManager->endCreateGraphicsPipeline();
	}
};