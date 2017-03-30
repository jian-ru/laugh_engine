#pragma once

#include "VDevice.h"


namespace rj
{
	class VQueryPool
	{
	public:
		VQueryPool(const VDevice &device)
			:
			m_pDevice(&device),
			m_queryPool{ device, vkDestroyQueryPool }
		{}

		void init(VkQueryType queryType, uint32_t queryCount, VkQueryPipelineStatisticFlags pipelineStatistics = 0)
		{
			assert(pipelineStatistics == 0 || queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS);

			VkQueryPoolCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			info.queryType = queryType;
			info.queryCount = queryCount;
			info.pipelineStatistics = pipelineStatistics;

			if (vkCreateQueryPool(*m_pDevice, &info, nullptr, m_queryPool.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("Error: failed to create query pool");
			}

			m_queryType = queryType;
			m_queryCount = queryCount;
			m_pipelineStatistics = pipelineStatistics;
		}

		VkQueryType getQueryType() const { return m_queryType; }
		uint32_t getQueryCount() const { return m_queryCount; }
		VkQueryPipelineStatisticFlags getQueryPipelineStatistics() const { return m_pipelineStatistics; }

		operator VkQueryPool() const { return m_queryPool; }

	protected:
		const VDevice *m_pDevice;

		VDeleter<VkQueryPool> m_queryPool;
		VkQueryType m_queryType;
		uint32_t m_queryCount;
		VkQueryPipelineStatisticFlags m_pipelineStatistics;
	};
}
