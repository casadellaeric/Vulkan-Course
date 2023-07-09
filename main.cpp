#define STB_IMAGE_IMPLEMENTATION

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
			if (vulkanRenderer.init() == EXIT_FAILURE)
			{
				return EXIT_FAILURE;
			}

			float angle = 0.f;
			float deltaTime = 0.f;
			float lastTime = 0.f;

			float addedFrameTime{};
			uint16_t frameCount{}; // nº frames since last fps check

			size_t testModel{ vulkanRenderer.create_mesh_model("Models/Seahawk.obj") };

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
				
				glm::mat4 testMat{ glm::rotate(glm::mat4(1.f), glm::radians(angle), {0.f, 0.f, 1.f}) };
				testMat = glm::scale(testMat, { 0.1f, 0.1f, 0.1f });
				vulkanRenderer.update_model_matrix(testModel, testMat);

				vulkanRenderer.draw();
				frameCount++;
			}
		} 
	}
}