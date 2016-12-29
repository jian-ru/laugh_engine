#pragma once

#include "VDevice.h"


namespace rj
{
	class VDescriptorPool
	{
	public:
		VDescriptorPool(const VDevice &device)
			:
			m_pDevice(&device),
			m_descriptorPool{ device, vkDestroyDescriptorPool }
		{}

		void init(uint32_t maxNumSets, const std::vector<VkDescriptorPoolSize> &poolSizes)
		{
			assert(!poolSizes.empty());

			VkDescriptorPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = maxNumSets;

			if (vkCreateDescriptorPool(*m_pDevice, &poolInfo, nullptr, m_descriptorPool.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor pool!");
			}

			m_poolSizes = poolSizes;
			m_maxSetCount = maxNumSets;
		}

		operator VkDescriptorPool() const { return m_descriptorPool; }

	protected:
		const VDevice *m_pDevice;

		VDeleter<VkDescriptorPool> m_descriptorPool;
		std::vector<VkDescriptorPoolSize> m_poolSizes;
		uint32_t m_maxSetCount;
	};
}