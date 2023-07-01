#pragma once

#include "Window.h"
#include "Utilities.h"

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
		void free();

	private:
		const Window& m_window;

		// Main Vulkan components
		VkInstance m_instance;
		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;
		VkQueue m_graphicsQueue;
		VkQueue m_presentationQueue;
		VkSurfaceKHR m_surface;
		VkSwapchainKHR m_swapchain;
		std::vector<SwapchainImage> m_swapchainImages{};

		// Secondary Vulkan components
		VkFormat m_swapchainImageFormat;
		VkExtent2D m_swapchainExtent;

		// Vulkan functions
		// - Create functions
		void create_instance();
		void create_logical_device();
		void create_surface();
		void create_swapchain();

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
	};
}