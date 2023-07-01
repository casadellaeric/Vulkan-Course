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

		// Vulkan components
		VkInstance m_instance;
		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;
		VkQueue m_graphicsQueue;
		VkQueue m_presentationQueue;
		VkSurfaceKHR m_surface;

		// Vulkan functions
		// - Create functions
		void create_instance();
		void create_logical_device();
		void create_surface();

		// - Get/Obtain functions
		// Not a getter, obtains the physical device to initialize m_device.physicalDevice
		void obtain_physical_device();
		QueueFamilyIndices get_queue_family_indices(const VkPhysicalDevice& device) const;
		SwapChainDetails get_swap_chain_details(const VkPhysicalDevice& device) const;

		// - Support functions
		bool check_instance_extension_support(const std::vector<const char*>& requestedExtensionNames) const;
		bool check_device_extension_support(const VkPhysicalDevice& device) const;
		bool check_validation_layer_support(const std::vector<const char*>& supportedValidationLayerNames) const;
		// Check support for properties, features, queue families...
		bool device_supports_requirements(const VkPhysicalDevice& device) const;
	};
}