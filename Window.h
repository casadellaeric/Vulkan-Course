#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <iostream>

namespace VkCourse
{
	// Represents a window. Controls GLFW initialization and termination. No support for multiple windows.
	class Window
	{
	public:

		Window();
		~Window();

		GLFWwindow* get_window_handle() const;

		int init(int width, int height, const std::string& windowName);
		bool should_close() const;
		void process_pending_events();

		const std::vector<const char*> get_required_extension_names() const;

	private:

		GLFWwindow* m_window;
	};
}
