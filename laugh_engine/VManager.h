#pragma once

#include <unordered_map>
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
		void beginCreateRenderPass(const std::string &name)
		{
			m_curRenderPassInfo = {};
			m_curRenderPassName = name;
			m_renderPasses[name] = VDeleter<VkRenderPass>{ m_device, vkDestroyRenderPass };
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

		void endCreateRenderPass()
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
		}
		// --- Render pass creation ---

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
		std::string m_curRenderPassName;
		SubpassCreateInfo *m_pCurSubpassInfo;
		std::unordered_map<std::string, VDeleter<VkRenderPass>> m_renderPasses;
	};
}