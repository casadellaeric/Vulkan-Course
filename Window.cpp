#include "Window.h"

#include <iostream>

namespace VkCourse
{
	Window::Window()
	{
		std::cout << "Window created" << std::endl;
	}

	Window::~Window()
	{
		std::cout << "Window destroyed" << std::endl;
		if (m_window != nullptr)
		{
			glfwDestroyWindow(m_window);
		}
		glfwTerminate();
	}

	GLFWwindow* Window::get_window_handle() const
	{
		return m_window;
	}

	int Window::init(int width, int height, const std::string& windowName)
	{
		if (glfwInit() == GLFW_FALSE)
		{
			return EXIT_FAILURE;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Not OpenGL neither OpenGL ES
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Window not resizable for now
		m_window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
		if (m_window == nullptr)
		{
			glfwTerminate();
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	bool Window::should_close() const
	{
		return glfwWindowShouldClose(m_window);
	}

	void Window::process_pending_events()
	{
		glfwPollEvents();
	}

	const std::vector<const char*> Window::get_required_extension_names() const
	{
		unsigned int extensionCount{};
		const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
		return std::vector<const char*>(extensionNames, extensionNames + extensionCount);
	}
}