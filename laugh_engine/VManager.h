#pragma once

#include <set>
#include <unordered_map>
#include <memory>
#include "VInstance.h"
#include "VWindow.h"
#include "VDevice.h"
#include "VQueueFamilyIndices.h"


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
			VkPipelineVertexInputStateCreateInfo vertexInputInfo;
			VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
			VkPipelineViewportStateCreateInfo viewportStateInfo;
			VkPipelineRasterizationStateCreateInfo rasterizerInfo;
			VkPipelineMultisampleStateCreateInfo multisamplingInfo;
			VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
			std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
			VkPipelineColorBlendStateCreateInfo colorBlendInfo;
			std::vector<VkDynamicState> dynamicStates;
			VkPipelineDynamicStateCreateInfo dynamicStateInfo;

			VDeleter<VkShaderModule> vertShaderModule;
			VkSpecializationInfo vertSpecializationInfo = {};
			std::vector<VkSpecializationMapEntry> vertSpecializationMapEntries;

			VDeleter<VkShaderModule> hullShaderModule;
			VkSpecializationInfo hullSpecializationInfo = {};
			std::vector<VkSpecializationMapEntry> hullSpecializationMapEntries;

			VDeleter<VkShaderModule> domainShaderModule;
			VkSpecializationInfo domainSpecializationInfo = {};
			std::vector<VkSpecializationMapEntry> domainSpecializationMapEntries;
			
			VDeleter<VkShaderModule> geomShaderModule;
			VkSpecializationInfo geomSpecializationInfo = {};
			std::vector<VkSpecializationMapEntry> geomSpecializationMapEntries;
			
			VDeleter<VkShaderModule> fragShaderModule;
			VkSpecializationInfo fragSpecializationInfo = {};
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
	}

	using namespace helper_functions;

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
		}

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
			m_curDescriptorSetInfo = {};
			m_curSetLayoutName = static_cast<uint32_t>(m_descriptorSetLayouts.size());
			m_descriptorSetLayouts[m_curSetLayoutName] = VDeleter<VkDescriptorSetLayout>{ m_device, vkDestroyDescriptorSetLayout };
		}

		void setLayoutAddBinding(uint32_t bindingPoint, VkDescriptorType type, VkShaderStageFlags shaderStages,
			uint32_t count = 1, uint32_t immutableSamplerName = std::numeric_limits<uint32_t>::max())
		{
			m_curDescriptorSetInfo.bindings.push_back({});
			VkDescriptorSetLayoutBinding &binding = m_curDescriptorSetInfo.bindings.back();

			binding.binding = bindingPoint;
			binding.descriptorType = type;
			binding.descriptorCount = count;
			binding.stageFlags = shaderStages;
			binding.pImmutableSamplers = &m_samplers[immutableSamplerName];
		}

		uint32_t endCreateDescriptorSetLayout()
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(m_curDescriptorSetInfo.bindings.size());
			layoutInfo.pBindings = m_curDescriptorSetInfo.bindings.data();

			if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, m_descriptorSetLayouts[m_curSetLayoutName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor set layout!");
			}

			m_curDescriptorSetInfo = {};
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

		// --- Pipeline creation ---
		void beginCreateGraphicsPipeline()
		{
			m_curGraphicsPipelineInfo = std::make_shared<GraphicsPipelineCreateInfo>(m_device);
			m_curPipelineName = static_cast<uint32_t>(m_pipelines.size());
			m_pipelines[m_curPipelineName] = VDeleter<VkPipeline>{ m_device, vkDestroyPipeline };
		}

		void graphicsPipelineAddShaderStage(VkShaderStageFlagBits stage, const std::string &spvFileName)
		{
			auto shaderByteCode = readFile(spvFileName);

			m_curGraphicsPipelineInfo->shaderStages.push_back({});
			VkPipelineShaderStageCreateInfo &shaderStageInfo = m_curGraphicsPipelineInfo->shaderStages.back();
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = stage;
			shaderStageInfo.pName = "main";

			switch (stage)
			{
			case VK_SHADER_STAGE_VERTEX_BIT:
				createShaderModule(m_device, shaderByteCode, m_curGraphicsPipelineInfo->vertShaderModule);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->vertShaderModule;
				break;
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				createShaderModule(m_device, shaderByteCode, m_curGraphicsPipelineInfo->hullShaderModule);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->hullShaderModule;
				break;
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				createShaderModule(m_device, shaderByteCode, m_curGraphicsPipelineInfo->domainShaderModule);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->domainShaderModule;
				break;
			case VK_SHADER_STAGE_GEOMETRY_BIT:
				createShaderModule(m_device, shaderByteCode, m_curGraphicsPipelineInfo->geomShaderModule);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->geomShaderModule;
				break;
			case VK_SHADER_STAGE_FRAGMENT_BIT:
				createShaderModule(m_device, shaderByteCode, m_curGraphicsPipelineInfo->fragShaderModule);
				shaderStageInfo.module = m_curGraphicsPipelineInfo->fragShaderModule;
				break;
			default:
				throw std::runtime_error("unknown graphics shader stage");
			}
		}
		// --- Pipeline creation ---

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


#ifndef NDEBUG
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

		DescriptorSetLayoutCreateInfo m_curDescriptorSetInfo;
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

		std::unordered_map<uint32_t, VDeleter<VkSampler>> m_samplers;
	};
}