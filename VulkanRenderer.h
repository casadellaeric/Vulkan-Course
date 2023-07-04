#pragma once

#include "Window.h"
#include "Utilities.h"
#include "Mesh.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Specific to Vulkan, as opposed to -1 to 1 (OpenGL)
#include <glm/glm.hpp>

namespace VkCourse
{
	const std::vector<const char*> requestedValidationLayerNames{
		"VK_LAYER_KHRONOS_validation"
	};

// We don't want to perform validation at runtime
#ifdef NDEBUG
	constexpr bool validationLayersEnabled{ false };
#else
	constexpr bool validationLayersEnabled{ true };
#endif

	class VulkanRenderer
	{
	public:
		VulkanRenderer(const Window& window);
		~VulkanRenderer();

		int init();
		void draw();
		void destroy();

	private:
		const Window& m_window;

		// Keeps track of which frame between 0 and MAX_FRAMES - 1, is being rendered to inside draw()
		unsigned int m_currentFrame{ 0 };

		// Scene objects
		Mesh m_firstMesh;

		// Main Vulkan components
		VkInstance m_instance;
		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;
		QueueFamilyIndices m_queueFamilyIndices;
		VkQueue m_graphicsQueue;
		VkQueue m_presentationQueue;
		VkSurfaceKHR m_surface;
		VkSwapchainKHR m_swapchain;
		std::vector<SwapchainImage> m_swapchainImages{};
		std::vector<VkFramebuffer> m_swapchainFramebuffers{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		// Pipeline
		VkPipeline m_graphicsPipeline;
		VkPipelineLayout m_pipelineLayout;
		VkRenderPass m_renderPass;

		// Pools
		VkCommandPool m_graphicsCommandPool;

		// Secondary Vulkan components
		VkFormat m_swapchainImageFormat;
		VkExtent2D m_swapchainExtent;
		
		// Synchronization
		std::vector<VkSemaphore> m_semaphoresImageAvailable{};
		std::vector<VkSemaphore> m_semaphoresRenderFinished{};
		std::vector<VkFence> m_fencesDraw{};

		// Vulkan functions
		// - Create functions
		void create_instance();
		void create_logical_device();
		void create_surface();
		void create_swapchain();
		void create_render_pass();
		void create_graphics_pipeline();
		void create_framebuffers();
		void create_command_pool();
		void create_command_buffers();
		void create_synchronization();

		// - Record functions
		void record_commands();

		// - Get/Obtain functions
		// Not a getter, obtains the physical device to initialize m_device.physicalDevice
		void obtain_physical_device();
		QueueFamilyIndices get_queue_family_indices(const VkPhysicalDevice& device) const;
		SwapchainDetails get_swap_chain_details(const VkPhysicalDevice& device) const;

		// - Support functions
		// -- Check functions
		bool check_instance_extension_support(const std::vector<const char*>& requestedExtensionNames) const;
		bool check_device_extension_support(const VkPhysicalDevice& device, const std::vector<const char*>& requestedExtensionNames) const;
		bool check_validation_layer_support(const std::vector<const char*>& requestedValidationLayerNames) const;
		bool device_supports_requirements(const VkPhysicalDevice& device) const;

		// -- Choose functions
		VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& surfaceFormatList) const;
		VkPresentModeKHR choose_presentation_mode(const std::vector<VkPresentModeKHR>& presentationModeList) const;
		VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const;

		// -- Create functions (reusable)
		VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		VkShaderModule create_shader_module(const std::vector<char>& code);
	};
}