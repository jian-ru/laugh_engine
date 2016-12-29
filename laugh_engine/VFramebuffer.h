#pragma once

#include "VImage.h"


namespace rj
{
	class VFramebuffer
	{
	public:
		VFramebuffer(const VDevice &device)
			:
			m_pDevice(&device),
			m_framebuffer{ device, vkDestroyFramebuffer }
		{}

		// Image views used in a framebuffer must be 2D (or equivalent to 2D with layers such as cube view)
		// and have only one mip level
		void init(VkRenderPass renderPass, const std::vector<VkImageView> &attachmentViews,
			uint32_t width, uint32_t height, uint32_t layers = 1, VkFramebufferCreateFlags flags = 0)
		{
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = renderPass;
			info.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
			info.pAttachments = attachmentViews.data();
			info.width = width;
			info.height = height;
			info.layers = layers;
			info.flags = flags;

			if (vkCreateFramebuffer(*m_pDevice, &info, nullptr, m_framebuffer.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}

			m_renderPass = renderPass;
			m_attchmentViews = attachmentViews;
			m_attachmentCount = info.attachmentCount;
			m_width = width;
			m_height = height;
			m_layers = layers;
		}

	protected:
		const VDevice *m_pDevice;

		VDeleter<VkFramebuffer> m_framebuffer;

		VkRenderPass m_renderPass = VK_NULL_HANDLE;
		std::vector<VkImageView> m_attchmentViews;
		uint32_t m_attachmentCount = 0;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_layers;
	};
}