#pragma once

#include "VDevice.h"


namespace rj
{
	class VSampler
	{
	public:
		VSampler(const VDevice &device)
			:
			m_device(device),
			m_sampler{ m_device, vkDestroySampler }
		{}

		void init(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
			VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV, VkSamplerAddressMode addressModeW,
			float minLod = 0.f, float maxLod = 0.f, float mipLodBias = 0.f, VkBool32 anisotropyEnable = VK_FALSE,
			float maxAnisotropy = 0.f, VkBool32 compareEnable = VK_FALSE, VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
			VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, VkBool32 unnormailzedCoords = VK_FALSE,
			VkSamplerCreateFlags flags = 0)
		{
			assert(!unnormailzedCoords || (minFilter == magFilter && mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST &&
				minLod == 0.f && maxLod == 0.f &&
				(addressModeU & (VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE | VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)) &&
				(addressModeV & (VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE | VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)) &&
				!anisotropyEnable && !compareEnable));

			m_info = {};
			m_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			m_info.magFilter = magFilter;
			m_info.minFilter = minFilter;
			m_info.mipmapMode = mipmapMode;
			m_info.addressModeU = addressModeU;
			m_info.addressModeV = addressModeV;
			m_info.addressModeW = addressModeW;
			m_info.minLod = minLod;
			m_info.maxLod = maxLod;
			m_info.mipLodBias = mipLodBias;
			m_info.anisotropyEnable = anisotropyEnable;
			m_info.maxAnisotropy = maxAnisotropy;
			m_info.compareEnable = compareEnable;
			m_info.compareOp = compareOp;
			m_info.borderColor = borderColor;
			m_info.unnormalizedCoordinates = unnormailzedCoords;
			m_info.flags = flags;

			if (vkCreateSampler(m_device, &m_info, nullptr, m_sampler.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create sampler!");
			}
		}

		operator VkSampler() const { return m_sampler; }

	protected:
		const VDevice &m_device;

		VDeleter<VkSampler> m_sampler;
		VkSamplerCreateInfo m_info;
	};
}