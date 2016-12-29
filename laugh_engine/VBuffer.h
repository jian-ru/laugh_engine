#pragma once

#include "VDevice.h"


namespace rj
{
	class VBuffer
	{
	public:
		VBuffer(const VDevice &device)
			:
			m_device(device),
			m_buffer{ m_device, vkDestroyBuffer },
			m_bufferMemory{ m_device, vkFreeMemory }
		{}

		void init(VkDeviceSize sizeInBytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps)
		{
			createBuffer(m_buffer, m_bufferMemory, m_device, m_device, sizeInBytes, usage, memProps);
		}

		void *mapBuffer(VkDeviceSize offset = 0, VkDeviceSize sizeInBytes = 0) const
		{
			assert((m_memoryProperties & (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
				== (VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
			assert(offset < m_sizeInBytes && offset + sizeInBytes <= m_sizeInBytes);

			sizeInBytes = sizeInBytes == 0 ? m_sizeInBytes : sizeInBytes;
			void *mapped = nullptr;
			vkMapMemory(m_device, m_bufferMemory, offset, sizeInBytes, 0, &mapped);
			return mapped;
		}

		void unmapBuffer() const
		{
			vkUnmapMemory(m_device, m_bufferMemory);
		}

		operator VkBuffer() const { return m_buffer; }

		VkDeviceSize size() const { return m_sizeInBytes; }
		VkBufferUsageFlags usage() const { return m_usage; }
		VkMemoryPropertyFlags memoryProperties() const { return m_memoryProperties; }

	protected:
		const VDevice &m_device;

		VDeleter<VkBuffer> m_buffer;
		VDeleter<VkDeviceMemory> m_bufferMemory;

		VkDeviceSize m_sizeInBytes;
		VkBufferUsageFlags m_usage;
		VkMemoryPropertyFlags m_memoryProperties;
	};
}