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

			float angle = 0.f;
			float deltaTime = 0.f;
			float lastTime = 0.f;

			float addedFrameTime{};
			uint16_t frameCount{}; // nº frames since last fps check

			// Main loop
			while (!window.should_close())
			{
				// Check for inputs
				window.process_pending_events();

				float now{ static_cast<float>(glfwGetTime()) };
				deltaTime = now - lastTime;
				lastTime = now;
				addedFrameTime += deltaTime;

				if (addedFrameTime > 1.f)
				{
					std::cout << frameCount / addedFrameTime << std::endl;
					addedFrameTime -= 1.f;
					frameCount = 0;
				}

				angle += 30.f * deltaTime;

				if (angle > 360.f)
				{
					angle -= 360.f;
				}
				
				glm::mat4 firstModel(1.f);
				glm::mat4 secondModel(1.f);

				firstModel = glm::translate(firstModel, glm::vec3(0.f, 0.f, 1.f));
				firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.f, 0.f, 1.f));

				secondModel = glm::translate(secondModel, glm::vec3(1.f, 0.f, 0.f));
				secondModel = glm::rotate(secondModel, glm::radians(-angle * 10), glm::vec3(0.f, 0.f, 1.f));

				vulkanRenderer.update_model_matrix(0, firstModel);
				vulkanRenderer.update_model_matrix(1, secondModel);

				vulkanRenderer.draw();
				frameCount++;
			}
		} 
	}
}