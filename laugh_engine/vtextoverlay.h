#pragma once

#include <vulkan/vulkan.h>

#include "vutils.h"

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


	VTextOverlay(
		const VkPhysicalDevice &pd, const VDeleter<VkDevice> &d,
		const QueueFamilyIndices &qfi, const VkQueue &sq, const SwapChainWrapper &sc,
		const std::vector<VDeleter<VkFramebuffer>> &fbs) :
		physicalDevice(pd),
		device(d),
		queueFamilyIndices(qfi),
		submitQueue(sq),
		swapChain(sc),
		framebuffers(fbs)
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
		createFences();
	}

	void beginTextUpdate()
	{
		if (vkMapMemory(device, fontQuadVertexBuffer.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&mapped))
		{
			throw std::runtime_error("VTextOverlay::beginTextUpdate");
		}
		numLetters = 0;
	}

	void addText(std::string text, float x, float y, TextAlign align = alignLeft)
	{
		assert(mapped != nullptr);

		const float charW = 1.5f / swapChain.swapChainExtent.width;
		const float charH = 1.5f / swapChain.swapChainExtent.height;

		float fbW = (float)swapChain.swapChainExtent.width;
		float fbH = (float)swapChain.swapChainExtent.height;
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
		vkUnmapMemory(device, fontQuadVertexBuffer.bufferMemory);
		mapped = nullptr;
		updateCommandBuffers(imageIdx);
	}

	void updateCommandBuffers(uint32_t imageIdx)
	{
		VkCommandBufferBeginInfo cmdBufInfo = {};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.extent.width = swapChain.swapChainExtent.width;
		renderPassBeginInfo.renderArea.extent.height = swapChain.swapChainExtent.height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		renderPassBeginInfo.framebuffer = framebuffers[imageIdx];

		waitForFence(imageIdx);
		vkBeginCommandBuffer(commandBuffers[imageIdx], &cmdBufInfo);

		vkCmdBeginRenderPass(commandBuffers[imageIdx], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[imageIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = {};
		viewport.width = swapChain.swapChainExtent.width;
		viewport.height = swapChain.swapChainExtent.height;
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffers[imageIdx], 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent = swapChain.swapChainExtent;
		vkCmdSetScissor(commandBuffers[imageIdx], 0, 1, &scissor);

		vkCmdBindDescriptorSets(commandBuffers[imageIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

		VkDeviceSize offsets = 0;
		vkCmdBindVertexBuffers(commandBuffers[imageIdx], 0, 1, &fontQuadVertexBuffer.buffer, &offsets);
		vkCmdBindVertexBuffers(commandBuffers[imageIdx], 1, 1, &fontQuadVertexBuffer.buffer, &offsets);
		for (uint32_t j = 0; j < numLetters; j++)
		{
			vkCmdDraw(commandBuffers[imageIdx], 4, 1, j * 4, 0);
		}

		vkCmdEndRenderPass(commandBuffers[imageIdx]);

		if (vkEndCommandBuffer(commandBuffers[imageIdx]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void submit(
		uint32_t bufferindex, 
		const std::vector<VkPipelineStageFlags> &waitStages, 
		const std::vector<VkSemaphore> &waitSemaphores, 
		const std::vector<VkSemaphore> &signalSemaphores) const
	{
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[bufferindex];
		submitInfo.pWaitDstStageMask = waitStages.data();
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
		submitInfo.pSignalSemaphores = signalSemaphores.data();

		if (vkQueueSubmit(submitQueue, 1, &submitInfo, fences[bufferindex]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}
	}

private:
	const VkPhysicalDevice &physicalDevice;
	const VDeleter<VkDevice> &device;
	const QueueFamilyIndices &queueFamilyIndices;
	const VkQueue &submitQueue;
	VDeleter<VkCommandPool> commandPool{ device, vkDestroyCommandPool };
	const SwapChainWrapper &swapChain;
	const std::vector<VDeleter<VkFramebuffer>> &framebuffers;

	ImageWrapper fontDeviceTexture{ device, VK_FORMAT_R8_UNORM };
	BufferWrapper fontQuadVertexBuffer{ device };
	VDeleter<VkDescriptorSetLayout> descriptorSetLayout{ device, vkDestroyDescriptorSetLayout };
	VDeleter<VkDescriptorPool> descriptorPool{ device, vkDestroyDescriptorPool };
	VkDescriptorSet descriptorSet;
	VDeleter<VkRenderPass> renderPass{ device, vkDestroyRenderPass };
	VDeleter<VkPipelineLayout> pipelineLayout{ device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> pipeline{ device, vkDestroyPipeline };
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VDeleter<VkFence>> fences;

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

		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

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

	void waitForFence(uint32_t idx)
	{
		vkWaitForFences(device, 1, &fences[idx], VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(device, 1, &fences[idx]);
	}

	void createFences()
	{
		fences.resize(swapChain.swapChainImages.size(), { device, vkDestroyFence });

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (auto &fence : fences)
		{
			if (vkCreateFence(device, &fenceInfo, nullptr, fence.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create fence.");
			}
		}
	}

	void createCommandPools()
	{
		// Each command buffer can be reset individually
		createCommandPool(device, queueFamilyIndices.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, commandPool);

		commandBuffers.resize(swapChain.swapChainImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void createFontTexture()
	{
		unsigned char fontTexture[STB_FONT_HEIGHT][STB_FONT_WIDTH]; // a texture atlas containing all the characters

		STB_FONT_NAME(fontDescriptors, fontTexture, STB_FONT_HEIGHT);

		// create staging buffer and copy host data to it
		BufferWrapper stagingBuffer{ device };
		createStagingBuffer(physicalDevice, device, fontTexture, STB_FONT_HEIGHT * STB_FONT_WIDTH,
			STB_FONT_HEIGHT * STB_FONT_WIDTH, stagingBuffer);

		// create device local image
		createImage(physicalDevice, device, STB_FONT_WIDTH, STB_FONT_HEIGHT,
			fontDeviceTexture.format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			fontDeviceTexture.image, fontDeviceTexture.imageMemory);

		// copy data from staging buffer to image and transition image to correct layouts
		transitionImageLayout(device, commandPool, submitQueue, fontDeviceTexture.image, fontDeviceTexture.format,
			1, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = STB_FONT_WIDTH;
		bufferCopyRegion.imageExtent.height = STB_FONT_HEIGHT;
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = 0;

		copyBufferToImage(device, commandPool, submitQueue, { bufferCopyRegion }, stagingBuffer.buffer, fontDeviceTexture.image);

		transitionImageLayout(device, commandPool, submitQueue, fontDeviceTexture.image, fontDeviceTexture.format,
			1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// create image view
		createImageView2D(device, fontDeviceTexture.image, fontDeviceTexture.format, VK_IMAGE_ASPECT_COLOR_BIT, 1, fontDeviceTexture.imageView);

		// create sampler
		VkSamplerCreateInfo samplerInfo = {};
		getDefaultSamplerCreateInfo(samplerInfo);
		vkCreateSampler(device, &samplerInfo, nullptr, fontDeviceTexture.sampler.replace());
	}

	void createVertexBuffer()
	{
		fontQuadVertexBuffer.numElements = MAX_CHAR_COUNT;
		fontQuadVertexBuffer.sizeInBytes = MAX_CHAR_COUNT * sizeof(glm::vec4); // (x, y, s, t)

		createBuffer(physicalDevice, device,
			fontQuadVertexBuffer.sizeInBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			fontQuadVertexBuffer.buffer, fontQuadVertexBuffer.bufferMemory);
	}

	void createDescriptorPoolAndSetLayouts()
	{
		// descriptor set layout
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.binding = 0;
		layoutBinding.descriptorCount = 1;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &layoutBinding;

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, descriptorSetLayout.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("VTextOverlay::createDescriptorPoolAndSetLayouts - cannot create descriptor set layout.");
		}

		// descriptor pool
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = 1;
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, descriptorPool.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("VTextOverlay::createDescriptorPoolAndSetLayouts - cannot create descriptor pool.");
		}
	}

	void createDescriptorSets()
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
		descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocInfo.descriptorPool = descriptorPool;
		descriptorSetAllocInfo.descriptorSetCount = 1;
		descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;

		if (vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &descriptorSet) != VK_SUCCESS)
		{
			throw std::runtime_error("VTextOverlay::createDescriptorSets - cannot create descriptor sets.");
		}

		VkDescriptorImageInfo imageInfo = fontDeviceTexture.getDescriptorInfo();

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
	}

	void createRenderPasses()
	{
		VkAttachmentDescription attachments[1] = {};

		// Color attachment
		attachments[0].format = swapChain.swapChainImageFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		// Don't clear the framebuffer (like the renderpass from the example does)
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Use subpass dependencies for image layout transitions
		VkSubpassDependency subpassDependencies[2] = {};

		// Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commmands executed outside of the actual renderpass)
		subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependencies[0].dstSubpass = 0;
		subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Transition from initial to final
		subpassDependencies[1].srcSubpass = 0;
		subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.flags = 0;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = NULL;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pResolveAttachments = NULL;
		subpassDescription.pDepthStencilAttachment = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = NULL;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pNext = NULL;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = subpassDependencies;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, renderPass.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("VTextOverlay::createRenderPasses - cannot create render pass.");
		}
	}

	void createPipelines()
	{
		ShaderFileNames shaderFiles;
		shaderFiles.vs = "../shaders/text_pass/text.vert.spv";
		shaderFiles.fs = "../shaders/text_pass/text.frag.spv";

		DefaultGraphicsPipelineCreateInfo infos{ device, shaderFiles };

		auto bindingDescription = TextQuadVertex::getBindingDescription();
		auto attributeDescriptions = TextQuadVertex::getAttributeDescriptions();

		infos.vertexInputInfo.vertexBindingDescriptionCount = 1;
		infos.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		infos.vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		infos.vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		infos.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

		infos.colorBlendAttachmentStates[0].blendEnable = VK_TRUE;
		infos.colorBlendAttachmentStates[0].colorWriteMask = 0xf;
		infos.colorBlendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		infos.colorBlendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		infos.colorBlendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
		infos.colorBlendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		infos.colorBlendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		infos.colorBlendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;

		infos.depthStencilInfo.depthTestEnable = VK_FALSE;
		infos.depthStencilInfo.depthWriteEnable = VK_FALSE;
		infos.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;

		std::vector<VkDynamicState> dynamicStateEnables =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		infos.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
		infos.dynamicStateInfo.pDynamicStates = dynamicStateEnables.data();

		infos.pipelineLayoutInfo.setLayoutCount = 1;
		infos.pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

		if (vkCreatePipelineLayout(device, &infos.pipelineLayoutInfo, nullptr, pipelineLayout.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		infos.pipelineInfo.layout = pipelineLayout;
		infos.pipelineInfo.renderPass = renderPass;
		infos.pipelineInfo.subpass = 0;
		infos.pipelineInfo.pDynamicState = &infos.dynamicStateInfo;

		if (vkCreateGraphicsPipelines(device, nullptr, 1, &infos.pipelineInfo, nullptr, pipeline.replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline!");
		}
	}
};