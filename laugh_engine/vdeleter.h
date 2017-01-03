#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cassert>


namespace rj
{
	template <typename T>
	class VDeleter
	{
	public:
		VDeleter() : VDeleter([](T, VkAllocationCallbacks *) {}) {}

		VDeleter(std::function<void(T, VkAllocationCallbacks *)> deletef)
		{
			this->deleter = [=](T obj) { deletef(obj, nullptr); };
		}

		VDeleter(const VDeleter<VkInstance> &instance,
			std::function<void(VkInstance, T, VkAllocationCallbacks *)> deletef)
		{
			this->deleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
		}

		VDeleter(const VDeleter<VkDevice> &device, std::function<void(VkDevice, T, VkAllocationCallbacks *)> deletef)
		{
			this->deleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
		}

		// TODO: make copy ctor deleted
		VDeleter(const VDeleter<T> &other)
		{
			VDeleter<T> &_other = const_cast<VDeleter<T> &>(other);
			object = _other.object;
			deleter = _other.deleter;
			_other.object = VK_NULL_HANDLE;
		}

		// TODO: make copy assignment operator deleted
		VDeleter<T> &operator=(const VDeleter<T> &other)
		{
			VDeleter<T> &_other = const_cast<VDeleter<T> &>(other);
			object = _other.object;
			deleter = _other.deleter;
			_other.object = VK_NULL_HANDLE;
			return *this;
		}

		VDeleter(VDeleter<T> &&other)
		{
			object = other.object;
			deleter = other.deleter;
			other.object = VK_NULL_HANDLE;
		}

		VDeleter<T> &operator=(VDeleter<T> &&other)
		{
			object = other.object;
			deleter = other.deleter;
			other.object = VK_NULL_HANDLE;
			return *this;
		}

		virtual ~VDeleter()
		{
			cleanup();
		}

		const T *operator &() const
		{
			return &object;
		}

		T *replace()
		{
			cleanup();
			return &object;
		}

		operator T() const
		{
			return object;
		}

		void operator=(T rhs)
		{
			if (rhs != object)
			{
				cleanup();
				object = rhs;
			}
		}

		template<typename V>
		bool operator==(V rhs)
		{
			return object == T(rhs);
		}

		bool isvalid() const
		{
			return object != VK_NULL_HANDLE;
		}

	private:
		T object{ VK_NULL_HANDLE };
		std::function<void(T)> deleter;

		void cleanup()
		{
			if (object != VK_NULL_HANDLE)
			{
				deleter(object);
			}
			object = VK_NULL_HANDLE;
		}
	};
}
