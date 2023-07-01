#include "VulkanRenderer.h"
#include "Window.h"

#include <iostream>

constexpr int WINDOW_WIDTH{ 1200 };
constexpr int WINDOW_HEIGHT{ 675 };

int main()
{
	{ 
		VkCourse::Window window;
		if (window.init(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Course") == EXIT_FAILURE) return EXIT_FAILURE;

		{
			VkCourse::VulkanRenderer vulkanRenderer(window);
			if (vulkanRenderer.init() == EXIT_FAILURE) return EXIT_FAILURE;

			// Main loop
			while (!window.should_close())
			{
				// Check for inputs
				window.process_pending_events();
			}
		} 
	}
}