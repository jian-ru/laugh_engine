#pragma once

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cassert>


namespace rj
{
	// Only movable (cannot be copied/duplicated)
	// This is required to avoid freeing the same Vulkan object multiple times
	// It also allows VDeleter<T> to be used with STL containers which
	// copy/move construct elements and then destruct elements in the old memory
	// when reallocate. If we allow copy/duplicate, whenever the STL container
	// reallocate its internal memory, VDeleter<T>::~VDeleter will be called on
	// old elements and the copy/move constructed elements will refer to destroyed
	// Vulkan objects!
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
			if (object != other.object)
			{
				cleanup();
			}

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
			if (object != other.object)
			{
				cleanup();
			}

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
