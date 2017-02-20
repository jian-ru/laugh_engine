#pragma once

#include <vector>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "VDeleter.h"


namespace rj
{
	class VWindow
	{
	public:
		VWindow(
			const VDeleter<VkInstance> &inst,
			uint32_t width, uint32_t height,
			const std::string &title = "",
			void *app = nullptr,
			GLFWkeyfun onKeyPressed = nullptr,
			GLFWmousebuttonfun onMouseClicked = nullptr,
			GLFWcursorposfun onCursorMoved = nullptr,
			GLFWscrollfun onScroll = nullptr,
			GLFWwindowsizefun onWindowResized = nullptr)
			:
			m_instance(inst),
			m_width(width), m_height(height),
			m_windowTitle(title),
			m_app(app),
			m_onWindowResized(onWindowResized),
			m_onMouseClicked(onMouseClicked),
			m_onCursorMoved(onCursorMoved),
			m_onKeyPressed(onKeyPressed),
			m_onScroll(onScroll),
			m_surface{ m_instance, vkDestroySurfaceKHR }
		{
			initWindow();
			createSurface();
		}

		operator const VDeleter<VkSurfaceKHR> &() const
		{
			return m_surface;
		}

		operator VkSurfaceKHR() const
		{
			return static_cast<VkSurfaceKHR>(m_surface);
		}

		GLFWwindow *getWindow() const
		{
			assert(m_window);
			return m_window;
		}

		void getExtent(uint32_t *pWidth, uint32_t *pHeight) const
		{
			*pWidth = m_width;
			*pHeight = m_height;
		}

		void setWindowTitle(const std::string &title)
		{
			m_windowTitle = title;
			glfwSetWindowTitle(m_window, m_windowTitle.c_str());
		}

		static std::vector<const char *> getRequiredExtensions()
		{
			std::vector<const char *> extensions;

			unsigned int glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwInit();
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			for (unsigned int i = 0; i < glfwExtensionCount; i++)
			{
				extensions.push_back(glfwExtensions[i]);
			}

			return extensions;
		}

	protected:
		void initWindow()
		{
			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // not to create OpenGL context
			m_window = glfwCreateWindow(m_width, m_height, m_windowTitle.c_str(), nullptr, nullptr);
			if (m_app) glfwSetWindowUserPointer(m_window, m_app);
			if (m_onWindowResized) glfwSetWindowSizeCallback(m_window, m_onWindowResized);
			if (m_onMouseClicked) glfwSetMouseButtonCallback(m_window, m_onMouseClicked);
			if (m_onCursorMoved) glfwSetCursorPosCallback(m_window, m_onCursorMoved);
			if (m_onKeyPressed) glfwSetKeyCallback(m_window, m_onKeyPressed);
			if (m_onScroll) glfwSetScrollCallback(m_window, m_onScroll);
		}

		void createSurface()
		{
			if (glfwCreateWindowSurface(m_instance, m_window, nullptr, m_surface.replace()) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create window surface!");
			}
		}

		const VDeleter<VkInstance> &m_instance;

		uint32_t m_width, m_height;
		std::string m_windowTitle;
		GLFWwindow *m_window = nullptr;

		// Application provide implementations for these
		void *m_app;
		GLFWwindowsizefun m_onWindowResized;
		GLFWmousebuttonfun m_onMouseClicked;
		GLFWcursorposfun m_onCursorMoved;
		GLFWkeyfun m_onKeyPressed;
		GLFWscrollfun m_onScroll;

		VDeleter<VkSurfaceKHR> m_surface;
	};
}