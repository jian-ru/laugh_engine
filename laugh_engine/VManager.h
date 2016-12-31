#pragma once

#include <shared_mutex>
#include <set>
#include <unordered_map>
#include <memory>
#include "VInstance.h"
#include "VWindow.h"
#include "VDevice.h"
#include "VQueueFamilyIndices.h"
#include "VImage.h"
#include "VBuffer.h"
#include "VSampler.h"
#include "VFramebuffer.h"
#include "VDescriptorPool.h"


namespace rj
{
	namespace helper_functions
	{
		struct SubpassCreateInfo
		{
			std::vector<VkAttachmentReference> colorAttachmentRefs;
			std::vector<VkAttachmentReference> resolveAttachmentRefs; // must have the same size as @colorAttachmentRefs
			std::vector<VkAttachmentReference> depthAttachmentRefs; // size must be <= 1
			std::vector<VkAttachmentReference> inputAttachmentRefs;
			std::vector<uint32_t> preserveAttachmentRefs;
		};

		struct RenderPassCreateInfo
		{
			std::vector<VkAttachmentDescription> attachmentDescs;
			std::vector<SubpassCreateInfo> subpassInfos;
			std::vector<VkSubpassDescription> subpassDescs;
			std::vector<VkSubpassDependency> subpassDependencies;
		};

		struct DescriptorSetLayoutCreateInfo
		{
			std::vector<std::vector<VkSampler>> immutableSamplers;
			std::vector<VkDescriptorSetLayoutBinding> bindings;
		};

		struct PipelineLayoutCreateInfo
		{
			std::set<VkDescriptorSetLayout> descriptorSetLayouts;
			std::vector<VkPushConstantRange> pushConstantRanges;
		};

		struct ComputePipelineCreateInfo
		{
			VkComputePipelineCreateInfo pipelineInfo;

			VDeleter<VkShaderModule> computeShaderModule;
			VkSpecializationInfo computeSpecializationInfo = {};
			std::vector<char> computeSpecializationData;
			std::vector<VkSpecializationMapEntry> computeSpecializationMapEntries;

			ComputePipelineCreateInfo(const VDeleter<VkDevice> &device)
				: computeShaderModule{ device, vkDestroyShaderModule }
			{
				pipelineInfo = {};
				pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
				pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
				pipelineInfo.stage.pName = "main";
			}
		};

		struct GraphicsPipelineCreateInfo
		{
			std::vector<VkVertexInputBindingDescription> viBindingDescs;
			std::vector<VkVertexInputAttributeDescription> viAttrDescs;
			VkPipelineVertexInputStateCreateInfo vertexInputInfo;

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;

			VkPipelineTessellationStateCreateInfo tessellationInfo;

			// If multiple viewports and scissors are not enabled, count <= 1
			std::vector<VkViewport> viewports;
			std::vector<VkRect2D> scissors;
			VkPipelineViewportStateCreateInfo viewportStateInfo;
			
			VkPipelineRasterizationStateCreateInfo rasterizerInfo;

			std::vector<VkSampleMask> sampleMask;
			VkPipelineMultisampleStateCreateInfo multisamplingInfo;
			
			VkPipelineDepthStencilStateCreateInfo depthStencilInfo;

			std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
			VkPipelineColorBlendStateCreateInfo colorBlendInfo;

			std::vector<VkDynamicState> dynamicStates;
			VkPipelineDynamicStateCreateInfo dynamicStateInfo;

			VDeleter<VkShaderModule> vertShaderModule;
			VkSpecializationInfo vertSpecializationInfo = {};
			std::vector<char> vertSpecializationData;
			std::vector<VkSpecializationMapEntry> vertSpecializationMapEntries;

			VDeleter<VkShaderModule> hullShaderModule;
			VkSpecializationInfo hullSpecializationInfo = {};
			std::vector<char> hullSpecializationData;
			std::vector<VkSpecializationMapEntry> hullSpecializationMapEntries;

			VDeleter<VkShaderModule> domainShaderModule;
			VkSpecializationInfo domainSpecializationInfo = {};
			std::vector<char> domainSpecializationData;
			std::vector<VkSpecializationMapEntry> domainSpecializationMapEntries;
			
			VDeleter<VkShaderModule> geomShaderModule;
			VkSpecializationInfo geomSpecializationInfo = {};
			std::vector<char> geomSpecializationData;
			std::vector<VkSpecializationMapEntry> geomSpecializationMapEntries;
			
			VDeleter<VkShaderModule> fragShaderModule;
			VkSpecializationInfo fragSpecializationInfo = {};
			std::vector<char> fragSpecializationData;
			std::vector<VkSpecializationMapEntry> fragSpecializationMapEntries;
			
			std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

			VkGraphicsPipelineCreateInfo pipelineInfo;

			GraphicsPipelineCreateInfo(const VDeleter<VkDevice> &device)
				:
				vertShaderModule{ device, vkDestroyShaderModule },
				hullShaderModule{ device, vkDestroyShaderModule },
				domainShaderModule{ device, vkDestroyShaderModule },
				geomShaderModule{ device, vkDestroyShaderModule },
				fragShaderModule{ device, vkDestroyShaderModule }
			{
				vertexInputInfo = {};
				inputAssemblyInfo = {};
				tessellationInfo = {};
				viewportStateInfo = {};
				rasterizerInfo = {};
				multisamplingInfo = {};
				depthStencilInfo = {};
				colorBlendAttachmentStates = {};
				colorBlendInfo = {};
				dynamicStateInfo = {};
				pipelineInfo = {};

				vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

				inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
				inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

				tessellationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

				viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
				viewportStateInfo.viewportCount = 1;
				viewportStateInfo.scissorCount = 1;

				rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
				rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
				rasterizerInfo.lineWidth = 1.0f;
				rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
				rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

				multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
				multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

				depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
				depthStencilInfo.depthTestEnable = VK_TRUE;
				depthStencilInfo.depthWriteEnable = VK_TRUE;
				depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

				colorBlendAttachmentStates.resize(1);
				colorBlendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachmentStates[0].blendEnable = VK_FALSE;

				colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
				colorBlendInfo.attachmentCount = 1;
				colorBlendInfo.pAttachments = colorBlendAttachmentStates.data();

				dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

				pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
				pipelineInfo.pVertexInputState = &vertexInputInfo;
				pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
				pipelineInfo.pViewportState = &viewportStateInfo;
				pipelineInfo.pRasterizationState = &rasterizerInfo;
				pipelineInfo.pMultisampleState = &multisamplingInfo;
				pipelineInfo.pDepthStencilState = &depthStencilInfo;
				pipelineInfo.pColorBlendState = &colorBlendInfo;
				pipelineInfo.pDynamicState = nullptr;
			}
		};

		struct DescriptorPoolCreateInfo
		{
			uint32_t maxSetCount;
			std::vector<VkDescriptorPoolSize> poolSizes;
		};

		struct DescriptorSetUpdateInfo
		{
			std::unordered_map<uint32_t, std::vector<VkDescriptorBufferInfo>> bufferInfos;
			std::unordered_map<uint32_t, std::vector<VkDescriptorImageInfo>> imageInfos;
			std::vector<VkWriteDescriptorSet> writeInfos;
		};
	}

	using namespace helper_functions;

	struct DescriptorSetUpdateBufferInfo
	{
		uint32_t bufferName;
		VkDeviceSize offset;
		VkDeviceSize sizeInBytes;
	};

	struct DescriptorSetUpdateImageInfo
	{
		uint32_t samplerName;
		uint32_t imageViewName;
		VkImageLayout layout;
	};

	class VManager
	{
	public:
		VManager(void *app,
			GLFWkeyfun keyfun = nullptr, GLFWmousebuttonfun mousebuttonfun = nullptr,
			GLFWcursorposfun cursorposfun = nullptr, GLFWscrollfun scrollfun = nullptr, GLFWwindowsizefun windowsizefun = nullptr,
			uint32_t winWidth = 1920, uint32_t winHeight = 1080, const std::string &winTitle = "",
			const VkPhysicalDeviceFeatures &enabledFeatures = {})
			:
			m_instance{ m_enableValidationLayers,{ "VK_LAYER_LUNARG_standard_validation" }, VWindow::getRequiredExtensions() },
			m_window{ m_instance, winWidth, winHeight, winTitle, app, keyfun, mousebuttonfun, cursorposfun, scrollfun, windowsizefun },
			m_device{ m_enableValidationLayers,{ "VK_LAYER_LUNARG_standard_validation" }, m_instance, m_window,{ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, enabledFeatures },
			m_swapChain{ m_device, m_window }
		{
			createPipelineCache();
			createSingleSubmitCommandPool();
		}

		virtual ~VManager() {}

		// --- Render pass creation ---
		void beginCreateRenderPass()
		{
			m_curRenderPassInfo = {};
			m_curRenderPassName = static_cast<uint32_t>(m_renderPasses.size());
			m_renderPasses[m_curRenderPassName] = VDeleter<VkRenderPass>{ m_device, vkDestroyRenderPass };
		}

		void renderPassAddAttachment(VkFormat format,
			VkImageLayout initLayout, VkImageLayout finalLayout, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
			VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE)
		{
			m_curRenderPassInfo.attachmentDescs.push_back({});
			VkAttachmentDescription &attachment = m_curRenderPassInfo.attachmentDescs.back();

			attachment.format = format;
			attachment.samples = samples;
			attachment.loadOp = loadOp;
			attachment.storeOp = storeOp;
			attachment.stencilLoadOp = stencilLoadOp;
			attachment.stencilStoreOp = stencilStoreOp;
			attachment.initialLayout = initLayout;
			attachment.finalLayout = finalLayout;
		}

		void beginDescribeSubpass()
		{
			m_curRenderPassInfo.subpassInfos.push_back({});
			m_pCurSubpassInfo = &m_curRenderPassInfo.subpassInfos.back();
		}

		void subpassAddColorAttachmentReference(uint32_t attachmentIdx, VkImageLayout layout)
		{
			m_pCurSubpassInfo->colorAttachmentRefs.push_back({});
			VkAttachmentReference &attachmentRef = m_pCurSubpassInfo->colorAttachmentRefs.back();

			attachmentRef.attachment = attachmentIdx;
			attachmentRef.layout = layout;
		}

		void subpassAddResolveAttachmentReference(uint32_t attachmentIdx, VkImageLayout layout)
		{
			m_pCurSubpassInfo->resolveAttachmentRefs.push_back({});
			VkAttachmentReference &attachmentRef = m_pCurSubpassInfo->resolveAttachmentRefs.back();

			attachmentRef.attachment = attachmentIdx;
			attachmentRef.layout = layout;
		}

		void subpassAddDepthAttachmentReference(uint32_t attachmentIdx, VkImageLayout layout)
		{
			if (m_pCurSubpassInfo->depthAttachmentRefs.size() == 1)
			{
				throw std::runtime_error("At most one depth attachment per subpass");
			}

			m_pCurSubpassInfo->depthAttachmentRefs.push_back({});
			VkAttachmentReference &attachmentRef = m_pCurSubpassInfo->depthAttachmentRefs.back();

			attachmentRef.attachment = attachmentIdx;
			attachmentRef.layout = layout;
		}

		void subpassAddInputAttachmentReference(uint32_t attachmentIdx, VkImageLayout layout)
		{
			m_pCurSubpassInfo->inputAttachmentRefs.push_back({});
			VkAttachmentReference &attachmentRef = m_pCurSubpassInfo->inputAttachmentRefs.back();

			attachmentRef.attachment = attachmentIdx;
			attachmentRef.layout = layout;
		}

		void subpassAddPreserveAttachmentReference(uint32_t attachmentIdx)
		{
			m_pCurSubpassInfo->preserveAttachmentRefs.push_back(attachmentIdx);
		}

		void endDescribeSubpass(VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS)
		{
			m_pCurSubpassInfo = nullptr;
			m_curRenderPassInfo.subpassDescs.push_back({});
			VkSubpassDescription &subpass = m_curRenderPassInfo.subpassDescs.back();

			subpass.pipelineBindPoint = bindPoint;
			if (m_pCurSubpassInfo->colorAttachmentRefs.size() > 0)
			{
				subpass.colorAttachmentCount = static_cast<uint32_t>(m_pCurSubpassInfo->colorAttachmentRefs.size());
				subpass.pColorAttachments = m_pCurSubpassInfo->colorAttachmentRefs.data();
			}

			if (m_pCurSubpassInfo->resolveAttachmentRefs.size() > 0)
			{
				if (m_pCurSubpassInfo->resolveAttachmentRefs.size() != m_pCurSubpassInfo->colorAttachmentRefs.size())
				{
					throw std::runtime_error("resolve attachment count must either be 0 or the same as color attachment count");
				}
				subpass.pResolveAttachments = m_pCurSubpassInfo->resolveAttachmentRefs.data();
			}

			if (m_pCurSubpassInfo->depthAttachmentRefs.size() > 0)
			{
				subpass.pDepthStencilAttachment = m_pCurSubpassInfo->depthAttachmentRefs.data();
			}

			if (m_pCurSubpassInfo->inputAttachmentRefs.size() > 0)
			{
				subpass.inputAttachmentCount = static_cast<uint32_t>(m_pCurSubpassInfo->inputAttachmentRefs.size());
				subpass.pInputAttachments = m_pCurSubpassInfo->inputAttachmentRefs.data();
			}

			if (m_pCurSubpassInfo->preserveAttachmentRefs.size() > 0)
			{
				subpass.preserveAttachmentCount = static_cast<uint32_t>(m_pCurSubpassInfo->preserveAttachmentRefs.size());
				subpass.pPreserveAttachments = m_pCurSubpassInfo->preserveAttachmentRefs.data();
			}
		}

		void renderPassAddSubpassDependency(uint32_t srcSubpass, uint32_t dstSubpass,
			VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages,
			VkAccessFlags srcAccessFlags, VkAccessFlags dstAccessFlags)
		{
			m_curRenderPassInfo.subpassDependencies.push_back({});
			VkSubpassDependency &dependency = m_curRenderPassInfo.subpassDependencies.back();

			dependency.srcSubpass = srcSubpass;
			dependency.dstSubpass = dstSubpass;
			dependency.srcStageMask = srcStages;
			dependency.srcAccessMask = srcAccessFlags;
			dependency.dstStageMask = dstStages;
			dependency.dstAccessMask = dstAccessFlags;
		}

		uint32_t endCreateRenderPass()
		{
			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32_t>(m_curRenderPassInfo.attachmentDescs.size());
			renderPassInfo.pAttachments = m_curRenderPassInfo.attachmentDescs.data();
			renderPassInfo.subpassCount = static_cast<uint32_t>(m_curRenderPassInfo.subpassDescs.size());
			renderPassInfo.pSubpasses = m_curRenderPassInfo.subpassDescs.data();
			renderPassInfo.dependencyCount = static_cast<uint32_t>(m_curRenderPassInfo.subpassDependencies.size());
			renderPassInfo.pDependencies = m_curRenderPassInfo.subpassDependencies.data();

			if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, m_renderPasses[m_curRenderPassName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create render pass!");
			}

			m_curRenderPassInfo = {};
			return m_curRenderPassName;
		}
		// --- Render pass creation ---

		// --- Descriptor set layout creation ---
		void beginCreateDescriptorSetLayout()
		{
			m_curSetLayoutInfo = {};
			m_curSetLayoutName = static_cast<uint32_t>(m_descriptorSetLayouts.size());
			m_descriptorSetLayouts[m_curSetLayoutName] = VDeleter<VkDescriptorSetLayout>{ m_device, vkDestroyDescriptorSetLayout };
		}

		void setLayoutAddBinding(uint32_t bindingPoint, VkDescriptorType type, VkShaderStageFlags shaderStages,
			uint32_t count = 1, const std::vector<uint32_t> &immutableSamplerNames = {})
		{
			m_curSetLayoutInfo.bindings.push_back({});
			VkDescriptorSetLayoutBinding &binding = m_curSetLayoutInfo.bindings.back();
			m_curSetLayoutInfo.immutableSamplers.push_back({});
			std::vector<VkSampler> &immutableSamplers = m_curSetLayoutInfo.immutableSamplers.back();

			binding.binding = bindingPoint;
			binding.descriptorType = type;
			binding.descriptorCount = count;
			binding.stageFlags = shaderStages;

			if (!immutableSamplerNames.empty())
			{
				immutableSamplers.reserve(immutableSamplerNames.size());
				for (auto name : immutableSamplerNames)
				{
					immutableSamplers.push_back(m_samplers.at(name));
				}
			}

			for (uint32_t i = 0; i < m_curSetLayoutInfo.bindings.size(); ++i)
			{
				auto &b = m_curSetLayoutInfo.bindings[i];
				auto &is = m_curSetLayoutInfo.immutableSamplers[i];

				b.pImmutableSamplers = nullptr;
				if (!is.empty())
				{
					b.pImmutableSamplers = is.data();
				}
			}
		}

		uint32_t endCreateDescriptorSetLayout()
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(m_curSetLayoutInfo.bindings.size());
			layoutInfo.pBindings = m_curSetLayoutInfo.bindings.data();

			if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_descriptorSetLayouts[m_curSetLayoutName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor set layout!");
			}

			m_curSetLayoutInfo = {};
			return m_curSetLayoutName;
		}
		// --- Descriptor set layout creation ---

		// --- Pipeline layouts creation ---
		void beginCreatePipelineLayout()
		{
			m_curPipelineLayoutInfo = {};
			m_curPipelineLayoutName = static_cast<uint32_t>(m_pipelineLayouts.size());
			m_pipelineLayouts[m_curPipelineLayoutName] = VDeleter<VkPipelineLayout>{ m_device, vkDestroyPipelineLayout };
		}

		void pipelineLayoutAddDescriptorSetLayouts(const std::vector<uint32_t> &setLayoutNames)
		{
			for (auto name : setLayoutNames)
			{
				auto found = m_descriptorSetLayouts.find(name);

				if (found == m_descriptorSetLayouts.end())
				{
					throw std::runtime_error("cannot find descriptor set layout");
				}
				else
				{
					m_curPipelineLayoutInfo.descriptorSetLayouts.insert(found->second);
				}
			}
		}

		void pipelineLayoutAddPushConstantRange(uint32_t offset, uint32_t size, VkShaderStageFlags shaderStages)
		{
			m_curPipelineLayoutInfo.pushConstantRanges.push_back({});
			VkPushConstantRange &psr = m_curPipelineLayoutInfo.pushConstantRanges.back();

			psr.offset = offset;
			psr.size = size;
			psr.stageFlags = shaderStages;
		}

		uint32_t endCreatePipelineLayout()
		{
			if (m_curPipelineLayoutInfo.descriptorSetLayouts.size() == 0)
			{
				throw std::runtime_error("pipeline layout contains at least one descriptor set layout");
			}

			VkPipelineLayoutCreateInfo info = {};

			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.setLayoutCount = static_cast<uint32_t>(m_curPipelineLayoutInfo.descriptorSetLayouts.size());
			std::vector<VkDescriptorSetLayout> setLayouts(
				m_curPipelineLayoutInfo.descriptorSetLayouts.begin(),
				m_curPipelineLayoutInfo.descriptorSetLayouts.end());
			info.pSetLayouts = setLayouts.data();
			info.pushConstantRangeCount = static_cast<uint32_t>(m_curPipelineLayoutInfo.pushConstantRanges.size());
			info.pPushConstantRanges = m_curPipelineLayoutInfo.pushConstantRanges.data();

			if (vkCreatePipelineLayout(m_device, &info, nullptr, m_pipelineLayouts[m_curPipelineLayoutName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create lighting pipeline layout!");
			}

			m_curPipelineLayoutInfo = {};
			return m_curPipelineLayoutName;
		}
		// --- Pipeline layouts creation ---

		// --- Graphics pipeline creation ---
		void beginCreateGraphicsPipeline(uint32_t layoutName, uint32_t renderPassName, uint32_t subpassIdx,
			uint32_t basePipelineName = std::numeric_limits<uint32_t>::max(), VkPipelineCreateFlags flags = 0)
		{
			m_curGraphicsPipelineInfo = std::make_shared<GraphicsPipelineCreateInfo>(m_device);
			m_curPipelineName = static_cast<uint32_t>(m_pipelines.size());
			m_pipelines[m_curPipelineName] = VDeleter<VkPipeline>{ m_device, vkDestroyPipeline };

			auto &pipelineInfo = m_curGraphicsPipelineInfo->pipelineInfo;
			pipelineInfo.flags = flags;

			auto layoutFound = m_pipelineLayouts.find(layoutName);
			if (layoutFound != m_pipelineLayouts.end())
			{
				pipelineInfo.layout = layoutFound->second;
			}
			else
			{
				throw std::runtime_error("Invalid pipeline layout");
			}

			auto renderPassFound = m_renderPasses.find(renderPassName);
			if (renderPassFound != m_renderPasses.end())
			{
				pipelineInfo.renderPass = renderPassFound->second;
			}
			else
			{
				throw std::runtime_error("Invalid render pass");
			}

			pipelineInfo.subpass = subpassIdx;

			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			if (flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
			{
				auto basePipelineFound = m_pipelines.find(basePipelineName);
				if (basePipelineFound != m_pipelines.end())
				{
					pipelineInfo.basePipelineHandle = basePipelineFound->second;
					pipelineInfo.basePipelineIndex = -1;
				}
				else
				{
					throw std::runtime_error("base pipeline not found");
				}
			}
		}

		void graphicsPipelineAddShaderStage(VkShaderStageFlagBits stage, const std::string &spvFileName, VkPipelineShaderStageCreateFlags flags = 0)
		{
			auto shaderByteCode = readFile(spvFileName);

			m_curGraphicsPipelineInfo->shaderStages.push_back({});
			VkPipelineShaderStageCreateInfo &shaderStageInfo = m_curGraphicsPipelineInfo->shaderStages.back();
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = stage;
			shaderStageInfo.pName = "main";
			shaderStageInfo.flags = flags;

			switch (stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				createShaderModule(m_curGraphicsPipelineInfo->vertShaderModule, m_device, shaderByteCode);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->vertShaderModule;
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				createShaderModule(m_curGraphicsPipelineInfo->hullShaderModule, m_device, shaderByteCode);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->hullShaderModule;
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				createShaderModule(m_curGraphicsPipelineInfo->domainShaderModule, m_device, shaderByteCode);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->domainShaderModule;
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				createShaderModule(m_curGraphicsPipelineInfo->geomShaderModule, m_device, shaderByteCode);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->geomShaderModule;
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				createShaderModule(m_curGraphicsPipelineInfo->fragShaderModule, m_device, shaderByteCode);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->fragShaderModule;
				break;
			default:
				throw std::runtime_error("unknown graphics shader stage");
			}

			m_curGraphicsPipelineInfo->pipelineInfo.stageCount = static_cast<uint32_t>(m_curGraphicsPipelineInfo->shaderStages.size());
			m_curGraphicsPipelineInfo->pipelineInfo.pStages = m_curGraphicsPipelineInfo->shaderStages.data();
		}

		void graphicsPipelineAddSpecializationConstant(VkShaderStageFlagBits stage,
			uint32_t constantID, uint32_t offset, uint32_t size, void *srcData)
		{
			auto checkShaderModule = [](const VDeleter<VkShaderModule> &module)
			{
				if (!module.isvalid())
				{
					throw std::runtime_error("try to add specialization constant to shader stage but the stage doesn't exist");
				}
			};

			if (!srcData) throw std::invalid_argument("data cannot be null");
			if (size == 0) throw std::invalid_argument("size need to be greater than 0");

			std::vector<VkSpecializationMapEntry> *mapEntries = nullptr;
			std::vector<char>* data = nullptr;
			VkSpecializationInfo *specializationInfo = nullptr;

			switch (stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				checkShaderModule(m_curGraphicsPipelineInfo->vertShaderModule);
				mapEntries = &m_curGraphicsPipelineInfo->vertSpecializationMapEntries;
				data = &m_curGraphicsPipelineInfo->vertSpecializationData;
				specializationInfo = &m_curGraphicsPipelineInfo->vertSpecializationInfo;
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				checkShaderModule(m_curGraphicsPipelineInfo->hullShaderModule);
				mapEntries = &m_curGraphicsPipelineInfo->hullSpecializationMapEntries;
				data = &m_curGraphicsPipelineInfo->hullSpecializationData;
				specializationInfo = &m_curGraphicsPipelineInfo->hullSpecializationInfo;
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				checkShaderModule(m_curGraphicsPipelineInfo->domainShaderModule);
				mapEntries = &m_curGraphicsPipelineInfo->domainSpecializationMapEntries;
				data = &m_curGraphicsPipelineInfo->domainSpecializationData;
				specializationInfo = &m_curGraphicsPipelineInfo->domainSpecializationInfo;
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				checkShaderModule(m_curGraphicsPipelineInfo->geomShaderModule);
				mapEntries = &m_curGraphicsPipelineInfo->geomSpecializationMapEntries;
				data = &m_curGraphicsPipelineInfo->geomSpecializationData;
				specializationInfo = &m_curGraphicsPipelineInfo->geomSpecializationInfo;
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				checkShaderModule(m_curGraphicsPipelineInfo->fragShaderModule);
				mapEntries = &m_curGraphicsPipelineInfo->fragSpecializationMapEntries;
				data = &m_curGraphicsPipelineInfo->fragSpecializationData;
				specializationInfo = &m_curGraphicsPipelineInfo->fragSpecializationInfo;
				break;
			default:
				throw std::runtime_error("unknown shader stage");
			}

			mapEntries->push_back({});
			auto &mapEntry = mapEntries->back();
			mapEntry.constantID = constantID;
			mapEntry.offset = offset;
			mapEntry.size = size;

			if (data->size() < offset + size)
			{
				data->resize(offset + size);
			}
			memcpy(&(*data)[offset], srcData, size);

			specializationInfo->mapEntryCount = static_cast<uint32_t>(mapEntries->size());
			specializationInfo->pMapEntries = mapEntries->data();
			specializationInfo->dataSize = data->size();
			specializationInfo->pData = data->data();

			VkPipelineShaderStageCreateInfo *shaderStageInfo = nullptr;
			for (auto &ss : m_curGraphicsPipelineInfo->shaderStages)
			{
				if (ss.stage == stage)
				{
					shaderStageInfo = &ss;
				}
			}
			shaderStageInfo->pSpecializationInfo = specializationInfo;
		}

		void graphicsPipelineAddBindingDescription(uint32_t binding, uint32_t stride, VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX)
		{
			m_curGraphicsPipelineInfo->viBindingDescs.push_back({});
			VkVertexInputBindingDescription &bindingDesc = m_curGraphicsPipelineInfo->viBindingDescs.back();

			bindingDesc.binding = binding;
			bindingDesc.stride = stride;
			bindingDesc.inputRate = inputRate;

			m_curGraphicsPipelineInfo->vertexInputInfo.vertexBindingDescriptionCount =
				static_cast<uint32_t>(m_curGraphicsPipelineInfo->viBindingDescs.size());
			m_curGraphicsPipelineInfo->vertexInputInfo.pVertexBindingDescriptions =
				m_curGraphicsPipelineInfo->viBindingDescs.data();
		}

		void graphicsPipelineAddAttributeDescription(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset)
		{
			m_curGraphicsPipelineInfo->viAttrDescs.push_back({});
			auto &attrDesc = m_curGraphicsPipelineInfo->viAttrDescs.back();

			attrDesc.location = location;
			attrDesc.binding = binding;
			attrDesc.format = format;
			attrDesc.offset = offset;

			m_curGraphicsPipelineInfo->vertexInputInfo.vertexAttributeDescriptionCount =
				static_cast<uint32_t>(m_curGraphicsPipelineInfo->viAttrDescs.size());
			m_curGraphicsPipelineInfo->vertexInputInfo.pVertexAttributeDescriptions =
				m_curGraphicsPipelineInfo->viAttrDescs.data();
		}

		void graphicsPipelineConfigureInputAssembly(VkPrimitiveTopology topology, VkBool32 enablePrimitiveRestart = VK_FALSE, VkPipelineInputAssemblyStateCreateFlags flags = 0)
		{
			auto &info = m_curGraphicsPipelineInfo->inputAssemblyInfo;

			info.topology = topology;
			info.primitiveRestartEnable = enablePrimitiveRestart;
			info.flags = flags;
		}

		void graphicsPipelineConfigureTessellationState(uint32_t numCPsPerPatch, VkPipelineTessellationStateCreateFlags flags = 0)
		{
			auto &info = m_curGraphicsPipelineInfo->tessellationInfo;

			info.patchControlPoints = numCPsPerPatch;
			info.flags = flags;
		}

		void graphicsPipelineAddViewportAndScissor(
			float viewportX, float viewportY, float viewportWidth, float viewportHeight, float minDepth = 0.f, float maxDepth = 1.f,
			int32_t scissorX = 0, int32_t scissorY = 0, uint32_t scissorWidth = 0, uint32_t scissorHeight = 0, bool coverEntireViewport = true)
		{
			if (viewportWidth < 0.f || viewportHeight < 0.f || scissorWidth < 0 || scissorHeight < 0)
			{
				throw std::invalid_argument("invalid arguments to graphicsPipelineAddViewportAndScissor");
			}

			m_curGraphicsPipelineInfo->viewports.push_back({});
			auto &viewport = m_curGraphicsPipelineInfo->viewports.back();

			viewport.x = viewportX;
			viewport.y = viewportY;
			viewport.width = viewportWidth;
			viewport.height = viewportHeight;
			viewport.minDepth = minDepth;
			viewport.maxDepth = maxDepth;

			m_curGraphicsPipelineInfo->scissors.push_back({});
			auto &scissor = m_curGraphicsPipelineInfo->scissors.back();

			if (coverEntireViewport)
			{
				scissor.offset.x = static_cast<int32_t>(viewportX);
				scissor.offset.y = static_cast<int32_t>(viewportY);
				scissor.extent.width = static_cast<int32_t>(viewportWidth);
				scissor.extent.height = static_cast<int32_t>(viewportHeight);
			}
			else
			{
				scissor.offset = { scissorX, scissorY };
				scissor.extent = { scissorWidth, scissorHeight };
			}

			auto &info = m_curGraphicsPipelineInfo->viewportStateInfo;
			info.viewportCount = static_cast<uint32_t>(m_curGraphicsPipelineInfo->viewports.size());
			info.pViewports = m_curGraphicsPipelineInfo->viewports.data();
			info.scissorCount = static_cast<uint32_t>(m_curGraphicsPipelineInfo->scissors.size());
			info.pScissors = m_curGraphicsPipelineInfo->scissors.data();
		}

		void graphicsPipelineConfigureRasterizer(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace, float lineWidth = 1.f,
			VkBool32 depthBiasEnable = VK_FALSE, float depthBiasConstantFactor = 0.f, float depthBiasSlopeFactor = 1.f,
			VkBool32 depthClampEnable = VK_FALSE, float depthBiasClamp = 1.f,
			VkBool32 rasterizerDiscardEnable = VK_FALSE, VkPipelineRasterizationStateCreateFlags flags = 0)
		{
			auto &info = m_curGraphicsPipelineInfo->rasterizerInfo;

			info.polygonMode = polygonMode;
			info.cullMode = cullMode;
			info.frontFace = frontFace;
			info.lineWidth = lineWidth;
			info.depthBiasEnable = depthBiasEnable;
			info.depthBiasConstantFactor = depthBiasConstantFactor;
			info.depthBiasSlopeFactor = depthBiasSlopeFactor;
			info.depthClampEnable = depthClampEnable;
			info.depthBiasClamp = depthBiasClamp;
			info.rasterizerDiscardEnable = rasterizerDiscardEnable;
			info.flags = flags;
		}

		void graphicsPipelineConfigureMultisampleState(VkSampleCountFlagBits sampleCount, VkBool32 perSampleShading = VK_FALSE, float minSampleShadingFraction = 1.f,
			const std::vector<VkSampleMask> &sampleMask = {}, VkBool32 alphaToCoverageEnable = VK_FALSE, VkBool32 alphaToOneEnable = VK_FALSE,
			VkPipelineMultisampleStateCreateFlags flags = 0)
		{
			auto &info = m_curGraphicsPipelineInfo->multisamplingInfo;

			info.rasterizationSamples = sampleCount;
			info.sampleShadingEnable = perSampleShading;
			info.minSampleShading = minSampleShadingFraction;
			m_curGraphicsPipelineInfo->sampleMask = sampleMask;
			info.pSampleMask = nullptr;
			if (!sampleMask.empty())
			{
				info.pSampleMask = m_curGraphicsPipelineInfo->sampleMask.data();
			}
			info.alphaToCoverageEnable = alphaToCoverageEnable;
			info.alphaToOneEnable = alphaToOneEnable;
		}

		void graphicsPipelineConfigureDepthState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp,
			VkBool32 depthBoundsTestEnable = VK_FALSE, float minDepthBounds = 0.f, float maxDepthBounds = 1.f)
		{
			auto &info = m_curGraphicsPipelineInfo->depthStencilInfo;

			info.depthTestEnable = depthTestEnable;
			info.depthWriteEnable = depthWriteEnable;
			info.depthCompareOp = depthCompareOp;
			info.depthBoundsTestEnable = depthBoundsTestEnable;
			info.minDepthBounds = minDepthBounds;
			info.maxDepthBounds = maxDepthBounds;
		}

		void graphicsPipelineConfigureStencilState(VkBool32 stencilTestEnable,
			VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp,
			uint32_t reference, uint32_t compareMask = 0xffffffff, uint32_t writeMask = 0xffffffff,
			bool frontOp = true)
		{
			auto &info = m_curGraphicsPipelineInfo->depthStencilInfo;
			auto &stencilOpState = frontOp ? info.front : info.back;

			info.stencilTestEnable = stencilTestEnable;
			stencilOpState.failOp = failOp;
			stencilOpState.passOp = passOp;
			stencilOpState.depthFailOp = depthFailOp;
			stencilOpState.compareOp = compareOp;
			stencilOpState.compareMask = compareMask;
			stencilOpState.writeMask = writeMask;
			stencilOpState.reference = reference;
		}

		// If logic op is enabled, blending is automatically disabled on all color attachments
		// Logic op can only be applied to sint, uint, snorm, or unorm attachments.
		// For floating point attachments, fragment outputs are pass through without modification.
		void graphicsPipelineConfigureLogicOp(VkBool32 logicOpEnable, VkLogicOp logicOp)
		{
			auto &info = m_curGraphicsPipelineInfo->colorBlendInfo;

			info.logicOpEnable = logicOpEnable;
			info.logicOp = logicOp;
		}

		void graphicsPipeLineAddColorBlendAttachment(VkBool32 blendEnable,
			VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp, bool alphaSameAsColor = true,
			VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD,
			VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
		{
			m_curGraphicsPipelineInfo->colorBlendAttachmentStates.push_back({});
			VkPipelineColorBlendAttachmentState &info = m_curGraphicsPipelineInfo->colorBlendAttachmentStates.back();

			info.blendEnable = blendEnable;
			info.srcColorBlendFactor = srcColorBlendFactor;
			info.dstColorBlendFactor = dstColorBlendFactor;
			info.colorBlendOp = colorBlendOp;
			info.srcAlphaBlendFactor = alphaSameAsColor ? srcColorBlendFactor : srcAlphaBlendFactor;
			info.dstAlphaBlendFactor = alphaSameAsColor ? dstColorBlendFactor : dstAlphaBlendFactor;
			info.alphaBlendOp = alphaSameAsColor ? colorBlendOp : alphaBlendOp;
			info.colorWriteMask = colorWriteMask;

			m_curGraphicsPipelineInfo->colorBlendInfo.attachmentCount =
				static_cast<uint32_t>(m_curGraphicsPipelineInfo->colorBlendAttachmentStates.size());
			m_curGraphicsPipelineInfo->colorBlendInfo.pAttachments =
				m_curGraphicsPipelineInfo->colorBlendAttachmentStates.data();
		}

		void graphicsPipelineSetBlendConstant(float R, float G, float B, float A)
		{
			float *bc = m_curGraphicsPipelineInfo->colorBlendInfo.blendConstants;
			bc[0] = R;
			bc[1] = G;
			bc[2] = B;
			bc[3] = A;
		}

		void graphicsPipelineAddDynamicState(VkDynamicState dynamicState)
		{
			m_curGraphicsPipelineInfo->dynamicStates.push_back(dynamicState);
			m_curGraphicsPipelineInfo->dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(m_curGraphicsPipelineInfo->dynamicStates.size());
			m_curGraphicsPipelineInfo->dynamicStateInfo.pDynamicStates = m_curGraphicsPipelineInfo->dynamicStates.data();
		}

		uint32_t endCreateGraphicsPipeline()
		{
			auto &info = m_curGraphicsPipelineInfo->pipelineInfo;

			info.stageCount = static_cast<uint32_t>(m_curGraphicsPipelineInfo->shaderStages.size());
			info.pStages = m_curGraphicsPipelineInfo->shaderStages.data();
			info.pVertexInputState = &m_curGraphicsPipelineInfo->vertexInputInfo;
			info.pInputAssemblyState = &m_curGraphicsPipelineInfo->inputAssemblyInfo;
			info.pTessellationState = nullptr;
			if (m_curGraphicsPipelineInfo->tessellationInfo.patchControlPoints > 0)
			{
				info.pTessellationState = &m_curGraphicsPipelineInfo->tessellationInfo;
			}
			info.pViewportState = &m_curGraphicsPipelineInfo->viewportStateInfo;
			info.pRasterizationState = &m_curGraphicsPipelineInfo->rasterizerInfo;
			info.pMultisampleState = &m_curGraphicsPipelineInfo->multisamplingInfo;
			info.pDepthStencilState = &m_curGraphicsPipelineInfo->depthStencilInfo;
			info.pColorBlendState = &m_curGraphicsPipelineInfo->colorBlendInfo;
			if (!m_curGraphicsPipelineInfo->dynamicStates.empty())
			{
				info.pDynamicState = &m_curGraphicsPipelineInfo->dynamicStateInfo;
			}

			if (vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1, &info, nullptr, m_pipelines[m_curPipelineName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create pipeline!");
			}

			m_curGraphicsPipelineInfo = nullptr;
			return m_curPipelineName;
		}
		// --- Graphics pipeline creation ---

		// --- Compute pipeline creation ---
		void beginCreateComputePipeline(uint32_t layoutName,
			uint32_t basePipelineName = std::numeric_limits<uint32_t>::max(), VkPipelineCreateFlags flags = 0)
		{
			m_curComputePipelineInfo = std::make_shared<ComputePipelineCreateInfo>(m_device);
			m_curPipelineName = static_cast<uint32_t>(m_pipelines.size());
			m_pipelines[m_curPipelineName] = VDeleter<VkPipeline>{ m_device, vkDestroyPipeline };

			auto &pipelineInfo = m_curComputePipelineInfo->pipelineInfo;
			pipelineInfo.flags = flags;

			auto layoutFound = m_pipelineLayouts.find(layoutName);
			if (layoutFound != m_pipelineLayouts.end())
			{
				pipelineInfo.layout = layoutFound->second;
			}
			else
			{
				throw std::runtime_error("Invalid pipeline layout");
			}

			if (flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT)
			{
				auto basePipelineFound = m_pipelines.find(basePipelineName);
				if (basePipelineFound != m_pipelines.end())
				{
					pipelineInfo.basePipelineHandle = basePipelineFound->second;
					pipelineInfo.basePipelineIndex = -1;
				}
				else
				{
					throw std::runtime_error("Invalid base pipeline");
				}
			}
		}

		void computePipelineAddShaderStage(const std::string &spvFileName, VkPipelineShaderStageCreateFlags flags = 0)
		{
			auto shaderByteCode = readFile(spvFileName);
			createShaderModule(m_curComputePipelineInfo->computeShaderModule, m_device, shaderByteCode);

			auto &info = m_curComputePipelineInfo->pipelineInfo.stage;
			info.module = m_curComputePipelineInfo->computeShaderModule;
			info.flags = flags;
		}

		void computePipelineAddSpecializationConstant(uint32_t constantID, uint32_t offset, uint32_t size, void *srcData)
		{
			if (!srcData) throw std::invalid_argument("data cannot be null");
			if (size == 0) throw std::invalid_argument("size need to be greater than 0");
			if (!m_curComputePipelineInfo->computeShaderModule.isvalid()) throw std::runtime_error("Cannot add specialization data to empty compute shader stage");

			auto &specializationInfo = m_curComputePipelineInfo->computeSpecializationInfo;
			auto &mapEntries = m_curComputePipelineInfo->computeSpecializationMapEntries;
			mapEntries.push_back({});
			auto &mapEntry = mapEntries.back();
			auto &specializationData = m_curComputePipelineInfo->computeSpecializationData;

			mapEntry.constantID = constantID;
			mapEntry.offset = offset;
			mapEntry.size = size;

			if (specializationData.size() < offset + size)
			{
				specializationData.resize(offset + size);
			}
			memcpy(&specializationData[offset], srcData, size);

			specializationInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
			specializationInfo.pMapEntries = mapEntries.data();
			specializationInfo.dataSize = specializationData.size();
			specializationInfo.pData = specializationData.data();

			m_curComputePipelineInfo->pipelineInfo.stage.pSpecializationInfo = &specializationInfo;
		}

		uint32_t endCreateComputePipeline()
		{
			if (vkCreateComputePipelines(m_device, m_pipelineCache, 1, &m_curComputePipelineInfo->pipelineInfo,
				nullptr, m_pipelines[m_curPipelineName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create compute pipeline!");
			}

			m_curComputePipelineInfo = nullptr;
			return m_curPipelineName;
		}
		// --- Compute pipeline creation ---

		// --- Image creation ---
		uint32_t createImage2D(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps,
			uint32_t mipLevels = 1, uint32_t arrayLayers = 1, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT,
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL)
		{
			uint32_t imageName = static_cast<uint32_t>(m_images.size());
			m_images.emplace(std::piecewise_construct, std::forward_as_tuple(imageName), std::forward_as_tuple(m_device));
			m_images.at(imageName).initAs2DImage(width, height, format, usage, memProps, mipLevels, arrayLayers, sampleCount, initialLayout, tiling);
			return imageName;
		}

		uint32_t createImageCube(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags memProps,
			uint32_t mipLevels = 1,
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL)
		{
			uint32_t imageName = static_cast<uint32_t>(m_images.size());
			m_images.emplace(std::piecewise_construct, std::forward_as_tuple(imageName), std::forward_as_tuple(m_device));
			m_images.at(imageName).initAsCubeImage(width, height, format, usage, memProps, mipLevels, initialLayout, tiling);
			return imageName;
		}
		// --- Image creation ---

		// --- Image view creation ---
		uint32_t createImageView(uint32_t imageName, VkImageViewType viewType, VkImageAspectFlags aspectMask,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0, uint32_t layerCount = 1,
			VkComponentMapping componentMapping = {}, VkImageViewCreateFlags flags = 0)
		{
			auto found = m_images.find(imageName);
			if (found == m_images.end())
			{
				throw std::invalid_argument("Invalid image name");
			}
			auto &image = found->second;

			uint32_t viewName = static_cast<uint32_t>(m_imageViews.size());
			m_imageViews.emplace(std::piecewise_construct, std::forward_as_tuple(viewName),
				std::forward_as_tuple(m_device, image));
			m_imageViews.at(viewName).init(viewType, aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount,
				componentMapping, flags);

			return viewName;
		}

		uint32_t createImageViewCube(uint32_t imageName, VkImageAspectFlags aspectMask,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0,
			VkComponentMapping componentMapping = {}, VkImageViewCreateFlags flags = 0)
		{
			return this->createImageView(imageName, VK_IMAGE_VIEW_TYPE_CUBE, aspectMask, baseMipLevel, levelCount, baseArrayLayer, 6,
				componentMapping, flags);
		}

		uint32_t createImageView2D(uint32_t imageName, VkImageAspectFlags aspectMask,
			uint32_t baseMipLevel = 0, uint32_t levelCount = 1, uint32_t baseArrayLayer = 0,
			VkComponentMapping componentMapping = {}, VkImageViewCreateFlags flags = 0)
		{
			return this->createImageView(imageName, VK_IMAGE_VIEW_TYPE_2D, aspectMask,
				baseMipLevel, levelCount, baseArrayLayer, 1, componentMapping, flags);
		}
		// --- Image view creation ---

		// --- Image utilities ---
		void transitionImageLayout(uint32_t imageName, VkImageLayout newLayout, bool tryPreserveContent = true)
		{
			auto found = m_images.find(imageName);
			if (found == m_images.end())
			{
				throw std::invalid_argument("Invalid image name");
			}
			auto &image = found->second;
			VkImageLayout oldLayout = tryPreserveContent ? image.layout() : VK_IMAGE_LAYOUT_UNDEFINED;

			beginSingleTimeCommands();
			recordImageLayoutTransitionCommands(m_singleTimeCommandBuffer, image, image.format(), 0, image.levels(),
				0, image.layers(), oldLayout, newLayout);
			endSingleTimeCommands();
		}

		// TODO: Add support for more formats
		void transferHostDataToImage(uint32_t imageName, VkDeviceSize sizeInBytes, const void *hostData,
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED)
		{
			if (!hostData) throw std::invalid_argument("hostData cannot be null");
			if (sizeInBytes == 0) throw std::invalid_argument("sizeInBytes cannot be 0");

			transitionImageLayout(imageName, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			auto &image = m_images.at(imageName);

			VBuffer stagingBuffer{ m_device };
			stagingBuffer.init(sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			void *mapped = stagingBuffer.mapBuffer();
			memcpy(mapped, hostData, sizeInBytes);
			stagingBuffer.unmapBuffer();
			mapped = nullptr;

			beginSingleTimeCommands();
			recordCopyBufferToImageCommands(m_singleTimeCommandBuffer, stagingBuffer, image, image.format(), aspectMask,
				image.extent().width, image.extent().height, image.extent().depth, image.levels(), image.layers());
			endSingleTimeCommands();

			if (finalLayout != VK_IMAGE_LAYOUT_UNDEFINED)
			{
				transitionImageLayout(imageName, finalLayout);
			}
		}
		// --- Image utilities ---

		// --- Buffer related ---
		uint32_t createBuffer(VkDeviceSize sizeInBytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps)
		{
			uint32_t bufferName;
			if (!m_availableBufferNames.empty())
			{
				bufferName = m_availableBufferNames.back();
				m_availableBufferNames.pop_back();
			}
			else
			{
				bufferName = static_cast<uint32_t>(m_buffers.size());
				m_buffers.emplace_back(m_device);
			}

			m_buffers.at(bufferName).init(sizeInBytes, usage, memProps);
			return bufferName;
		}

		void desctroyBuffer(uint32_t bufferName)
		{
			assert(std::find(m_availableBufferNames.begin(), m_availableBufferNames.end(), bufferName) == m_availableBufferNames.end());
			assert(bufferName < m_buffers.size());
			m_availableBufferNames.push_back(bufferName);
		}

		void transferHostDataToBuffer(uint32_t bufferName, VkDeviceSize sizeInBytes, const void *hostData, VkDeviceSize dstOffset = 0)
		{
			if (!hostData) throw std::invalid_argument("hostData cannot be null");
			if (sizeInBytes == 0) throw std::invalid_argument("sizeInBytes cannot be 0");

			auto &dstBuffer = m_buffers.at(bufferName);

			VBuffer stagingBuffer{ m_device };
			stagingBuffer.init(sizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			void *mapped = stagingBuffer.mapBuffer();
			memcpy(mapped, hostData, sizeInBytes);
			stagingBuffer.unmapBuffer();
			mapped = nullptr;

			beginSingleTimeCommands();
			recordCopyBufferToBufferCommands(m_singleTimeCommandBuffer, stagingBuffer, dstBuffer, sizeInBytes, 0, dstOffset);
			endSingleTimeCommands();
		}

		void *mapBuffer(uint32_t bufferName, VkDeviceSize offset = 0, VkDeviceSize sizeInBytes = 0)
		{
			auto &buffer = m_buffers.at(bufferName);
			return buffer.mapBuffer(offset, sizeInBytes);
		}

		void unmapBuffer(uint32_t bufferName)
		{
			auto &buffer = m_buffers.at(bufferName);
			buffer.unmapBuffer();
		}
		// --- Buffer related ---

		// --- Sampler creation ---
		uint32_t creationSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
			VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW,
			float minLod = 0.f, float maxLod = 0.f, float mipLodBias = 0.f, VkBool32 anisotropyEnable = VK_FALSE,
			float maxAnisotropy = 0.f, VkBool32 compareEnable = VK_FALSE, VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
			VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, VkBool32 unnormailzedCoords = VK_FALSE,
			VkSamplerCreateFlags flags = 0)
		{
			uint32_t samplerName = static_cast<uint32_t>(m_samplers.size());
			m_samplers.emplace(std::piecewise_construct, std::forward_as_tuple(samplerName), std::forward_as_tuple(m_device));
			m_samplers.at(samplerName).init(magFilter, minFilter, mipmapMode, addressModeU, addressModeV, addressModeW,
				minLod, maxLod, mipLodBias, anisotropyEnable, maxAnisotropy, compareEnable, compareOp,
				borderColor, unnormailzedCoords, flags);
			return samplerName;
		}
		// --- Sampler creation ---

		// --- Framebuffer related ---
		uint32_t createFramebuffer(uint32_t renderPassName, const std::vector<uint32_t> &attachmentViewNames)
		{
			VkRenderPass renderPass = m_renderPasses.at(renderPassName);
			
			bool first = true;
			uint32_t width, height, layers;
			std::vector<VkImageView> attachmentViews;
			for (auto name : attachmentViewNames)
			{
				auto &view = m_imageViews.at(name);
				VkExtent3D extent = view.image()->extent(view.baseLevel());

				if (!first)
				{
					if (extent.width != width || extent.height != height || extent.depth != 1 ||
						view.layers() != layers)
					{
						throw std::runtime_error("Image view cannot be used as framebuffer attachment");
					}
					first = false;
				}

				width = extent.width;
				height = extent.height;
				layers = view.layers();
				attachmentViews.push_back(view);
			}

			uint32_t fbName;
			if (!m_availableFramebufferNames.empty())
			{
				fbName = m_availableFramebufferNames.back();
				m_availableFramebufferNames.pop_back();
			}
			else
			{
				fbName = static_cast<uint32_t>(m_framebuffers.size());
				m_framebuffers.emplace_back(m_device);
			}

			m_framebuffers[fbName].init(renderPass, attachmentViews, width, height, layers);
			return fbName;
		}

		std::vector<uint32_t> createSwapChainFramebuffers(uint32_t renderPassName)
		{
			VkRenderPass renderPass = m_renderPasses.at(renderPassName);

			if (!m_swapChainFramebufferNames.empty())
			{
				m_availableFramebufferNames.insert(m_availableFramebufferNames.end(),
					m_swapChainFramebufferNames.begin(), m_swapChainFramebufferNames.end());
				m_swapChainFramebufferNames.clear();
			}

			const auto &views = m_swapChain.imageViews();
			VkExtent2D extent = m_swapChain.extent();

			for (const auto &view : views)
			{
				uint32_t fbName;
				if (!m_availableFramebufferNames.empty())
				{
					fbName = m_availableFramebufferNames.back();
					m_availableFramebufferNames.pop_back();
				}
				else
				{
					fbName = static_cast<uint32_t>(m_framebuffers.size());
					m_framebuffers.emplace_back(m_device);
				}

				m_framebuffers.at(fbName).init(renderPass, { view }, extent.width, extent.height, 1);
				m_swapChainFramebufferNames.push_back(fbName);
			}

			return m_swapChainFramebufferNames;
		}

		VkExtent2D getFramebufferExtent(uint32_t framebufferName)
		{
			auto &framebuffer = m_framebuffers.at(framebufferName);
			return { framebuffer.width(), framebuffer.height() };
		}
		// --- Framebuffer related ---

		// --- Descriptor pool creation ---
		void beginCreateDescriptorPool(uint32_t maxNumSets)
		{
			m_curDescriptorPoolInfo = {};
			m_curDescriptorPoolInfo.maxSetCount = maxNumSets;
			m_curDescriptorPoolName = static_cast<uint32_t>(m_descriptorPools.size());
			m_descriptorPools.emplace(std::piecewise_construct, std::forward_as_tuple(m_curDescriptorPoolName),
				std::forward_as_tuple(m_device));
		}

		void descriptorPoolAddDescriptors(VkDescriptorType type, uint32_t count)
		{
			auto &poolSizes = m_curDescriptorPoolInfo.poolSizes;
			
			VkDescriptorPoolSize ps = {};
			ps.type = type;
			ps.descriptorCount = count;
			poolSizes.push_back(ps);
		}

		uint32_t endCreateDescriptorPool()
		{
			uint32_t maxSet = m_curDescriptorPoolInfo.maxSetCount;
			auto &poolSizes = m_curDescriptorPoolInfo.poolSizes;
			m_descriptorPools.at(m_curDescriptorPoolName).init(maxSet, poolSizes);
			
			return m_curDescriptorPoolName;
		}
		// --- Descriptor pool creation ---

		// --- Descriptor sets ---
		std::vector<uint32_t> allocateDescriptorSets(uint32_t descriptorPoolName, const std::vector<uint32_t> &setLayoutNames)
		{
			auto &pool = m_descriptorPools.at(descriptorPoolName);
			
			std::vector<VkDescriptorSetLayout> layouts;
			layouts.reserve(setLayoutNames.size());
			for (auto name : setLayoutNames)
			{
				layouts.push_back(m_descriptorSetLayouts.at(name));
			}

			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = pool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
			allocInfo.pSetLayouts = layouts.data();

			std::vector<VkDescriptorSet> sets(layouts.size());
			if (vkAllocateDescriptorSets(m_device, &allocInfo, sets.data()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate descriptor set!");
			}

			std::vector<uint32_t> setNames;
			setNames.reserve(sets.size());
			uint32_t setName = static_cast<uint32_t>(m_descriptorSets.size());
			for (auto set : sets)
			{
				setNames.push_back(setName);
				m_descriptorSets.emplace(setName++, set);
			}

			return setNames;
		}

		void beginUpdateDescriptorSet(uint32_t setName)
		{
			assert(m_descriptorSets.find(setName) != m_descriptorSets.end());
			m_curDescriptorSetInfo = {};
			m_curDescriptorSetName = setName;
		}

		void descriptorSetAddBufferDescriptor(uint32_t binding, VkDescriptorType type, const std::vector<DescriptorSetUpdateBufferInfo> &updateInfos,
			uint32_t baseArrayElement = 0)
		{
			assert(!updateInfos.empty());

			auto &bufferInfoTable = m_curDescriptorSetInfo.bufferInfos;
			assert(bufferInfoTable.find(binding) == bufferInfoTable.end());
			bufferInfoTable[binding] = {};
			auto &bufferInfos = bufferInfoTable[binding];

			bufferInfos.reserve(updateInfos.size());
			for (const auto &updateInfo : updateInfos)
			{
				VkDescriptorBufferInfo info = {};
				info.buffer = m_buffers.at(updateInfo.bufferName);
				info.offset = updateInfo.offset;
				info.range = updateInfo.sizeInBytes;
				bufferInfos.push_back(info);
			}

			auto &writeInfos = m_curDescriptorSetInfo.writeInfos;
			writeInfos.push_back({});
			auto &writeInfo = writeInfos.back();

			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = m_descriptorSets[m_curDescriptorSetName];
			writeInfo.dstBinding = binding;
			writeInfo.dstArrayElement = baseArrayElement;
			writeInfo.descriptorType = type;
			writeInfo.descriptorCount = static_cast<uint32_t>(bufferInfos.size());
			writeInfo.pBufferInfo = bufferInfos.data();
		}

		void descriptorSetAddImageDescriptor(uint32_t binding, VkDescriptorType type, const std::vector<DescriptorSetUpdateImageInfo> &updateInfos,
			uint32_t baseArrayElement = 0)
		{
			assert(!updateInfos.empty());

			auto &imageInfoTable = m_curDescriptorSetInfo.imageInfos;
			assert(imageInfoTable.find(binding) == imageInfoTable.end());
			auto &imageInfos = imageInfoTable.at(binding);

			imageInfos.reserve(updateInfos.size());
			for (const auto &updateInfo : updateInfos)
			{
				VkDescriptorImageInfo info = {};
				info.sampler = updateInfo.samplerName == std::numeric_limits<uint32_t>::max() ? VK_NULL_HANDLE : m_samplers.at(updateInfo.samplerName);
				info.imageView = m_imageViews.at(updateInfo.imageViewName);
				info.imageLayout = updateInfo.layout;
				imageInfos.push_back(info);
			}

			auto &writeInfos = m_curDescriptorSetInfo.writeInfos;
			writeInfos.push_back({});
			auto &writeInfo = writeInfos.back();

			writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeInfo.dstSet = m_descriptorSets[m_curDescriptorSetName];
			writeInfo.dstBinding = binding;
			writeInfo.dstArrayElement = baseArrayElement;
			writeInfo.descriptorType = type;
			writeInfo.descriptorCount = static_cast<uint32_t>(imageInfos.size());
			writeInfo.pImageInfo = imageInfos.data();
		}

		void endUpdateDescriptorSet()
		{
			vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(m_curDescriptorSetInfo.writeInfos.size()),
				m_curDescriptorSetInfo.writeInfos.data(), 0, nullptr);

			m_curDescriptorSetName = std::numeric_limits<uint32_t>::max();
			m_curDescriptorSetInfo = {};
		}
		// --- Descriptor sets ---

		// --- Command pool related ---
		uint32_t createCommandPool(VkQueueFlagBits submitQueueType, VkCommandPoolCreateFlags flags = 0)
		{
			uint32_t queueFamilyIndex;
			const auto &queueFamilyIndices = m_device.getQueueFamilyIndices();
			switch (submitQueueType)
			{
			case VK_QUEUE_GRAPHICS_BIT:
				queueFamilyIndex = queueFamilyIndices.graphicsFamily;
				break;
			case VK_QUEUE_COMPUTE_BIT:
				queueFamilyIndex = queueFamilyIndices.computeFamily;
				break;
			case VK_QUEUE_TRANSFER_BIT:
				queueFamilyIndex = queueFamilyIndices.transferFamily;
			default:
				throw std::invalid_argument("unsupported queue type specified during command pool creation");
			}

			uint32_t newPoolName;
			newPoolName = static_cast<uint32_t>(m_commandPools.size());
			m_commandPools.emplace(std::piecewise_construct, std::forward_as_tuple(newPoolName),
				std::forward_as_tuple(m_device, vkDestroyCommandPool));

			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = queueFamilyIndex;
			poolInfo.flags = flags;

			auto &pool = m_commandPools[newPoolName];
			if (vkCreateCommandPool(m_device, &poolInfo, nullptr, pool.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create command pool!");
			}

			{
				std::lock_guard<std::shared_mutex> gaurd(g_commandBufferMutex);
				m_commandBufferTable[newPoolName] = {};
			}

			return newPoolName;
		}

		void resetCommandPool(uint32_t commandPoolName, VkCommandPoolResetFlags flags = 0)
		{
			std::lock_guard<std::shared_mutex> guard(g_commandBufferMutex); // no command buffer is in-use

			VkCommandPool pool = VK_NULL_HANDLE;
			pool = m_commandPools.at(commandPoolName);

			if (vkResetCommandPool(m_device, pool, flags) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to reset command buffer");
			}

			if (flags & VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT)
			{
				auto &cbNames = m_commandBufferTable[commandPoolName];
				for (auto cbName : cbNames)
				{
					m_commandBuffers.erase(cbName);
					m_commandBufferAvaiableNames.push_back(cbName);
				}
				cbNames.clear();
			}
		}
		// --- Command pool related ---

		// --- Command buffer related ---
		void beginSingleTimeCommands()
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = m_commandPools[m_singleSubmitCommandPoolName];
			allocInfo.commandBufferCount = 1;

			vkAllocateCommandBuffers(m_device, &allocInfo, &m_singleTimeCommandBuffer);

			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_singleTimeCommandBuffer, &beginInfo);
		}

		void endSingleTimeCommands()
		{
			vkEndCommandBuffer(m_singleTimeCommandBuffer);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_singleTimeCommandBuffer;

			vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(m_device.getGraphicsQueue());

			vkFreeCommandBuffers(m_device, m_commandPools[m_singleSubmitCommandPoolName], 1, &m_singleTimeCommandBuffer);

			m_singleTimeCommandBuffer = VK_NULL_HANDLE;
		}

		std::vector<uint32_t> allocateCommandBuffers(uint32_t poolName, uint32_t count,
			VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
		{
			VkCommandPool pool = VK_NULL_HANDLE;
			std::vector<VkCommandBuffer> cbs(count);
			
			pool = m_commandPools.at(poolName);

			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = level;
			allocInfo.commandPool = pool;
			allocInfo.commandBufferCount = count;

			if (vkAllocateCommandBuffers(m_device, &allocInfo, cbs.data()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to allocate command buffers!");
			}

			std::vector<uint32_t> commandBufferNames;
			commandBufferNames.reserve(count);
			{
				std::lock_guard<std::shared_mutex> guard(g_commandBufferMutex);

				auto &poolCbs = m_commandBufferTable.at(poolName);
				for (auto cb : cbs)
				{
					uint32_t cbName;

					if (!m_commandBufferAvaiableNames.empty())
					{
						cbName = m_commandBufferAvaiableNames.back();
						m_commandBufferAvaiableNames.pop_back();
					}
					else
					{
						cbName = static_cast<uint32_t>(m_commandBuffers.size());
					}

					m_commandBuffers.emplace(cbName, cb);
					commandBufferNames.push_back(cbName);
					poolCbs.push_back(cbName);
				}
			}

			return commandBufferNames;
		}

		// TODO: support secondary command buffer
		void beginCommandBuffer(uint32_t commandBufferName, VkCommandBufferUsageFlags flags = 0) const
		{
			g_commandBufferMutex.lock_shared(); // some command buffer(s) are in-use

			const auto &commandBuffer = m_commandBuffers.at(commandBufferName);

			VkCommandBufferBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags = flags;

			if (vkBeginCommandBuffer(commandBuffer, &info) != VK_SUCCESS)
			{
				throw std::runtime_error("Unable to begin command buffer");
			}
		}

		void endCommandBuffer(uint32_t commandBufferName) const
		{
			const auto &commandBuffer = m_commandBuffers.at(commandBufferName);

			if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			{
				throw std::runtime_error("Unable to end command buffer!");
			}

			// std::lock_guard<std::shared_mutex> will unblock for a writer if
			// all readers have called std::shared_mutex::unlock_shared()
			g_commandBufferMutex.unlock_shared();
		}

		void cmdBindVertexBuffer(uint32_t cmdBufferName, const std::vector<uint32_t> &bufferNames,
			const std::vector<VkDeviceSize> &offsets, uint32_t firstBinding = 0) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			uint32_t numVertBuffers = static_cast<uint32_t>(bufferNames.size());
			std::vector<VkBuffer> vertBuffers;
			vertBuffers.reserve(numVertBuffers);
			for (auto name : bufferNames)
			{
				vertBuffers.push_back(m_buffers.at(name));
			}

			vkCmdBindVertexBuffers(cmdBuffer, firstBinding, numVertBuffers, vertBuffers.data(), offsets.data());
		}

		void cmdBindIndexBuffer(uint32_t cmdBufferName, uint32_t indexBufferName, VkIndexType type, VkDeviceSize offset = 0) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &idxBuffer = m_buffers.at(indexBufferName);

			vkCmdBindIndexBuffer(cmdBuffer, idxBuffer, offset, type);
		}

		void cmdBeginRenderPass(uint32_t cmdBufferName, uint32_t renderPassName, uint32_t frameBufferName,
			std::vector<VkClearValue> &clearValues, VkRect2D renderArea = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &renderPass = m_renderPasses.at(renderPassName);
			const auto &framebuffer = m_framebuffers.at(frameBufferName);

			renderArea.extent.width = renderArea.extent.width == 0 ? framebuffer.width() : renderArea.extent.width;
			renderArea.extent.height = renderArea.extent.height == 0 ? framebuffer.height() : renderArea.extent.height;

			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = renderPass;
			info.framebuffer = framebuffer;
			info.renderArea = renderArea;
			info.clearValueCount = static_cast<uint32_t>(clearValues.size());
			info.pClearValues = clearValues.size() == 0 ? nullptr : clearValues.data();

			vkCmdBeginRenderPass(cmdBuffer, &info, subpassContents);
		}

		void cmdEndRenderPass(uint32_t cmdBufferName) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			vkCmdEndRenderPass(cmdBuffer);
		}

		void cmdNextSubpass(uint32_t cmdBufferName, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			vkCmdNextSubpass(cmdBuffer, subpassContents);
		}

		void cmdBindPipeline(uint32_t cmdBufferName, VkPipelineBindPoint pipelineBindPoint, uint32_t pipelineName) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &pipeline = m_pipelines.at(pipelineName);

			vkCmdBindPipeline(cmdBuffer, pipelineBindPoint, pipeline);
		}

		void cmdBindDescriptorSets(uint32_t cmdBufferName, VkPipelineBindPoint pipelineBindPoint, uint32_t pipelineLayoutName,
			const std::vector<uint32_t> &descriptorSetNames, uint32_t firstSet = 0, const std::vector<uint32_t> &dynamicOffsets = {}) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &pipelineLayout = m_pipelineLayouts.at(pipelineLayoutName);

			uint32_t numSets = static_cast<uint32_t>(descriptorSetNames.size());
			std::vector<VkDescriptorSet> sets;
			sets.reserve(numSets);
			for (auto name : descriptorSetNames)
			{
				sets.push_back(m_descriptorSets.at(name));
			}

			uint32_t numDynamicOffsets = static_cast<uint32_t>(dynamicOffsets.size());

			vkCmdBindDescriptorSets(cmdBuffer, pipelineBindPoint, pipelineLayout, firstSet, numSets, sets.data(),
				numDynamicOffsets, numDynamicOffsets == 0 ? nullptr : dynamicOffsets.data());
		}

		void cmdSetViewport(uint32_t cmdBufferName, uint32_t framebufferName, float topLeftU = 0.f, float topLeftV = 0.f,
			float width = 1.f, float height = 1.f, float minDepth = 0.f, float maxDepth = 1.f) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &framebuffer = m_framebuffers.at(framebufferName);

			float fbWidth = static_cast<float>(framebuffer.width());
			float fbHeight = static_cast<float>(framebuffer.height());

			VkViewport viewport = {};
			viewport.x = topLeftU * fbWidth;
			viewport.y = topLeftV * fbHeight;
			viewport.width = width * fbWidth;
			viewport.height = height * fbHeight;
			viewport.minDepth = minDepth;
			viewport.maxDepth = maxDepth;

			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		}

		void cmdSetViewport(uint32_t cmdBufferName, float x, float y, float width, float height,
			float minDepth = 0.f, float maxDepth = 1.f) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			VkViewport viewport = {};
			viewport.x = x;
			viewport.y = y;
			viewport.width = width;
			viewport.height = height;
			viewport.minDepth = minDepth;
			viewport.maxDepth = maxDepth;

			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		}

		void cmdSetScissor(uint32_t cmdBufferName, uint32_t framebufferName,
			float topLeftU = 0.f, float topLeftV = 0.f, float width = 1.f, float height = 1.f) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &framebuffer = m_framebuffers.at(framebufferName);

			float fbWidth = static_cast<float>(framebuffer.width());
			float fbHeight = static_cast<float>(framebuffer.height());

			int32_t x = static_cast<int32_t>(topLeftU * fbWidth);
			int32_t y = static_cast<int32_t>(topLeftV * fbHeight);
			uint32_t w = static_cast<uint32_t>(width * fbWidth);
			uint32_t h = static_cast<uint32_t>(height * fbHeight);

			VkRect2D scissor = {};
			scissor.offset = { x, y };
			scissor.extent = { w, h };

			vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		}

		void cmdSetScissor(uint32_t cmdBufferName, int32_t x, int32_t y, uint32_t width, uint32_t height) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			VkRect2D scissor = {};
			scissor.offset = { x, y };
			scissor.extent = { width, height };

			vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
		}

		void cmdDrawIndexed(uint32_t cmdBufferName, uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0,
			int32_t vertexOffset = 0, uint32_t firstInstance = 0) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		}

		void cmdDraw(uint32_t cmdBufferName, uint32_t vertexCount, uint32_t instanceCount = 1,
			uint32_t firstVertex = 0, uint32_t firstInstance = 0) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
		}

		void cmdPushConstants(uint32_t cmdBufferName, uint32_t pipelineLayoutName, VkShaderStageFlags shaderStages,
			uint32_t offset, uint32_t sizeInBytes, const void *pValues) const
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);
			const auto &pipelineLayout = m_pipelineLayouts.at(pipelineLayoutName);

			vkCmdPushConstants(cmdBuffer, pipelineLayout, shaderStages, offset, sizeInBytes, pValues);
		}

		void cmdDispatch(uint32_t cmdBufferName, uint32_t numBlocksX, uint32_t numBlocksY, uint32_t numBlocksZ)
		{
			const auto &cmdBuffer = m_commandBuffers.at(cmdBufferName);

			vkCmdDispatch(cmdBuffer, numBlocksX, numBlocksY, numBlocksZ);
		}

		void queueWaitIdle(VkQueueFlags queueType) const
		{
			VkQueue queue = VK_NULL_HANDLE;
			switch (queueType)
			{
			case VK_QUEUE_COMPUTE_BIT:
				queue = m_device.getComputeQueue();
				break;
			default:
				queue = m_device.getGraphicsQueue();
				break;
			}
			vkQueueWaitIdle(queue);
		}

		void deviceWaitIdle()
		{
			if (vkDeviceWaitIdle(m_device) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to wait for device idle");
			}
		}

		void queueSubmit(VkQueueFlags queueType, const std::vector<uint32_t> &cmdBufferNames,
			const std::vector<uint32_t> &waitSemaphoreNames = {},
			const std::vector<VkPipelineStageFlags> &waitStageMasks = {},
			const std::vector<uint32_t> &signalSemaphoreNames = {}) const
		{
			assert(!cmdBufferNames.empty());

			VkQueue queue = VK_NULL_HANDLE;
			switch (queueType)
			{
			case VK_QUEUE_COMPUTE_BIT:
				queue = m_device.getComputeQueue();
				break;
			default:
				queue = m_device.getGraphicsQueue();
				break;
			}

			std::vector<VkCommandBuffer> cmdBuffers;
			cmdBuffers.reserve(cmdBufferNames.size());
			for (auto name : cmdBufferNames)
			{
				cmdBuffers.push_back(m_commandBuffers.at(name));
			}

			std::vector<VkSemaphore> waitSemaphores;
			waitSemaphores.reserve(waitSemaphoreNames.size());
			for (auto name : waitSemaphoreNames)
			{
				waitSemaphores.push_back(m_semaphores[name]);
			}

			std::vector<VkSemaphore> signalSemaphores;
			signalSemaphores.reserve(signalSemaphoreNames.size());
			for (auto name : signalSemaphoreNames)
			{
				signalSemaphores.push_back(m_semaphores[name]);
			}

			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
			info.pCommandBuffers = cmdBuffers.data();
			info.pWaitDstStageMask = waitStageMasks.data();
			info.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
			info.pWaitSemaphores = waitSemaphores.data();
			info.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
			info.pSignalSemaphores = signalSemaphores.data();
		}
		// --- Command buffer related ---

		// --- Synchronization objects ---
		uint32_t createSemaphore(VkSemaphoreCreateFlags flags = 0)
		{
			uint32_t semaphoreName;
			if (!m_availableSemaphoreNames.empty())
			{
				semaphoreName = m_availableSemaphoreNames.back();
				m_availableSemaphoreNames.pop_back();
			}
			else
			{
				semaphoreName = static_cast<uint32_t>(m_semaphores.size());
				m_semaphores.emplace_back(m_device, vkDestroySemaphore);
			}

			VkSemaphoreCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			info.flags = flags;

			if (vkCreateSemaphore(m_device, &info, nullptr, m_semaphores[semaphoreName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("unable to create semaphore");
			}

			return semaphoreName;
		}

		uint32_t createFence(VkFenceCreateFlags flags = 0)
		{
			uint32_t fenceName;
			if (!m_availableFenceNames.empty())
			{
				fenceName = m_availableFenceNames.back();
				m_availableFenceNames.pop_back();
			}
			else
			{
				fenceName = static_cast<uint32_t>(m_fences.size());
				m_fences.emplace_back(m_device, vkDestroyFence);
			}

			VkFenceCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			info.flags = flags;

			if (vkCreateFence(m_device, &info, nullptr, m_fences[fenceName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create fence");
			}

			return fenceName;
		}

		void waitForFences(const std::vector<uint32_t> &fenceNames, VkBool32 waitAll = VK_TRUE,
			uint64_t timeout = std::numeric_limits<uint64_t>::max())
		{
			uint32_t fenceCount = static_cast<uint32_t>(fenceNames.size());
			std::vector<VkFence> fences;
			fences.reserve(fenceCount);
			for (auto name : fenceNames)
			{
				fences.push_back(m_fences.at(name));
			}

			if (vkWaitForFences(m_device, fenceCount, fences.data(), waitAll, timeout) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to wait for fences");
			}
		}

		void resetFences(const std::vector<uint32_t> &fenceNames)
		{
			uint32_t fenceCount = static_cast<uint32_t>(fenceNames.size());
			std::vector<VkFence> fences;
			fences.reserve(fenceCount);
			for (auto name : fenceNames)
			{
				fences.push_back(m_fences.at(name));
			}

			if (vkResetFences(m_device, fenceCount, fences.data()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to reset fences");
			}
		}
		// --- Synchronization objects ---

		// --- Window system ---
		void recreateSwapChain()
		{
			m_swapChain.recreateSwapChain();
		}

		VkExtent2D getSwapChainExtent() const
		{
			return m_swapChain.extent();
		}

		void swapChainNextImageIndex(uint32_t *pIdx, uint32_t signalSemaphoreName, uint32_t waitFenceName,
			uint64_t timeout = std::numeric_limits<uint64_t>::max())
		{
			VkSemaphore semaphore = signalSemaphoreName == std::numeric_limits<uint32_t>::max() ? VK_NULL_HANDLE : m_semaphores[signalSemaphoreName];
			VkFence fence = waitFenceName == std::numeric_limits<uint32_t>::max() ? VK_NULL_HANDLE : m_fences[waitFenceName];

			if (vkAcquireNextImageKHR(m_device, m_swapChain, timeout, semaphore, fence, pIdx) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to acquire next image index");
			}
		}

		void queuePresent(const std::vector<uint32_t> &waitSemaphoreNames, uint32_t imageIdx)
		{
			VkSwapchainKHR swapChain = m_swapChain;
			std::vector<VkSemaphore> waitSemaphores;
			waitSemaphores.reserve(waitSemaphoreNames.size());
			for (auto name : waitSemaphoreNames)
			{
				waitSemaphores.push_back(m_semaphores[name]);
			}

			VkPresentInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			info.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphoreNames.size());
			info.pWaitSemaphores = waitSemaphores.data();
			info.swapchainCount = 1;
			info.pSwapchains = &swapChain;
			info.pImageIndices = &imageIdx;

			if (vkQueuePresentKHR(m_device.getPresentQueue(), &info) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to present swap chain image");
			}
		}

		int windowShouldClose() const
		{
			return glfwWindowShouldClose(m_window.getWindow());
		}

		void windowPollEvents() const
		{
			glfwPollEvents();
		}
		// --- Window system ---

		// --- Device properties ---
		void getPhysicalDeviceProperties(VkPhysicalDeviceProperties *pProps) const
		{
			vkGetPhysicalDeviceProperties(m_device, pProps);
		}
		// --- Device properties ---

	protected:
		void createPipelineCache()
		{
			VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
			pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			if (vkCreatePipelineCache(m_device, &pipelineCacheCreateInfo, nullptr, m_pipelineCache.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create pipeline cache.");
			}
		}

		void createSingleSubmitCommandPool()
		{
			createCommandPool(VK_QUEUE_GRAPHICS_BIT, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
		}


#ifdef NDEBUG
		bool m_enableValidationLayers = false;
#else
		bool m_enableValidationLayers = true;
#endif

		VInstance m_instance;
		VWindow m_window;
		VDevice m_device;
		VSwapChain m_swapChain;
		VDeleter<VkPipelineCache> m_pipelineCache{ m_device, vkDestroyPipelineCache };

		RenderPassCreateInfo m_curRenderPassInfo;
		uint32_t m_curRenderPassName;
		SubpassCreateInfo *m_pCurSubpassInfo;
		std::unordered_map<uint32_t, VDeleter<VkRenderPass>> m_renderPasses;

		DescriptorSetLayoutCreateInfo m_curSetLayoutInfo;
		uint32_t m_curSetLayoutName;
		std::unordered_map<uint32_t, VDeleter<VkDescriptorSetLayout>> m_descriptorSetLayouts;

		PipelineLayoutCreateInfo m_curPipelineLayoutInfo;
		uint32_t m_curPipelineLayoutName;
		std::unordered_map<uint32_t, VDeleter<VkPipelineLayout>> m_pipelineLayouts;

		union
		{
			std::shared_ptr<GraphicsPipelineCreateInfo> m_curGraphicsPipelineInfo;
			std::shared_ptr<ComputePipelineCreateInfo> m_curComputePipelineInfo;
		};
		uint32_t m_curPipelineName;
		std::unordered_map<uint32_t, VDeleter<VkPipeline>> m_pipelines;

		const uint32_t m_singleSubmitCommandPoolName = 0;
		std::unordered_map<uint32_t, VDeleter<VkCommandPool>> m_commandPools;

		VkCommandBuffer m_singleTimeCommandBuffer;

		static std::shared_mutex g_commandBufferMutex;
		std::vector<uint32_t> m_commandBufferAvaiableNames;
		std::unordered_map<uint32_t, std::vector<uint32_t>> m_commandBufferTable; // command buffers of each pool
		std::unordered_map<uint32_t, VkCommandBuffer> m_commandBuffers;

		std::vector<uint32_t> m_availableBufferNames;
		std::vector<VBuffer> m_buffers;

		std::unordered_map<uint32_t, VImage> m_images;
		std::unordered_map<uint32_t, VImageView> m_imageViews;
		std::unordered_map<uint32_t, VSampler> m_samplers;

		std::vector<uint32_t> m_swapChainFramebufferNames;
		std::vector<uint32_t> m_availableFramebufferNames;
		std::vector<VFramebuffer> m_framebuffers;

		DescriptorPoolCreateInfo m_curDescriptorPoolInfo;
		uint32_t m_curDescriptorPoolName;
		std::unordered_map<uint32_t, VDescriptorPool> m_descriptorPools;

		DescriptorSetUpdateInfo m_curDescriptorSetInfo;
		uint32_t m_curDescriptorSetName;
		std::unordered_map<uint32_t, VkDescriptorSet> m_descriptorSets;

		std::vector<uint32_t> m_availableSemaphoreNames;
		std::vector<VDeleter<VkSemaphore>> m_semaphores;

		std::vector<uint32_t> m_availableFenceNames;
		std::vector<VDeleter<VkFence>> m_fences;
	};
}