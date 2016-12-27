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

		// --- Command pool creation ---
		uint32_t createCommandPool(VkQueueFlagBits submitQueueType, VkCommandPoolCreateFlags flags = 0)
		{
			uint32_t queueFamilyIndex;
			const auto &queueFamilyIndices = m_device.getQueueFamilyIndices();
			switch(submitQueueType)
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

			uint32_t newPoolName = static_cast<uint32_t>(m_commandPools.size());
			m_commandPools[newPoolName] = VDeleter<VkCommandPool>{ m_device, vkDestroyCommandPool };

			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = queueFamilyIndex;
			poolInfo.flags = flags;

			if (vkCreateCommandPool(m_device, &poolInfo, nullptr, m_commandPools[newPoolName].replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create command pool!");
			}
		}
		// --- Command pool creation ---

		// --- Image creation ---

		// --- Image creation ---

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

		std::unordered_map<uint32_t, VDeleter<VkCommandPool>> m_commandPools;



		std::unordered_map<uint32_t, VDeleter<VkSampler>> m_samplers;
	};
}