#include "VulkanRenderer.h"
#include "Utilities.h"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>


namespace VkCourse
{
	VulkanRenderer::VulkanRenderer(const Window& window)
		: m_window(window)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		free();
	}

	int VulkanRenderer::init()
	{
		try
		{
			create_instance();
			create_surface();
			obtain_physical_device();
			create_logical_device();
			create_swapchain();
		}
		catch (const std::runtime_error& error)
		{
			std::cout << error.what() << std::endl;
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	void VulkanRenderer::free()
	{
		// Destroy in inverse order of creation
		for (const auto& image : m_swapchainImages)
		{
			vkDestroyImageView(m_device.logicalDevice, image.imageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device.logicalDevice, m_swapchain, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyDevice(m_device.logicalDevice, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	void VulkanRenderer::create_instance()
	{
		// Mostly doesn't affect the application, can provide useful information to the driver/developer
		VkApplicationInfo appInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Vulkan Course Application",	// Custom application name
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),		// Custom application version, default
			.pEngineName = "No Engine",							// Custom engine name (not using one now)
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),			// Custom engine version, default
			.apiVersion = VK_API_VERSION_1_3,					// Highest Vulkan version supported
		};

		// We can store here all the extensions we need for the instance
		std::vector<const char*> instanceRequiredExtensions{};

		// Query for required extensions from the window, should be a list of names
		const std::vector<const char*> windowRequiredExtensions{ m_window.get_required_extension_names() };
		instanceRequiredExtensions.insert(instanceRequiredExtensions.end(),
			windowRequiredExtensions.begin(),
			windowRequiredExtensions.end());

		if (!check_instance_extension_support(instanceRequiredExtensions))
		{
			throw std::runtime_error("Instance does not support required extensions!");
		}

		if (validationLayersEnabled && !check_validation_layer_support(requestedValidationLayerNames))
		{
			throw std::runtime_error("Requested validation layers could not be loaded!");
		}

		// Creation information for a VkInstance. This pattern is ubiquitous throughout any Vulkan program
		VkInstanceCreateInfo instanceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = nullptr,
			.enabledExtensionCount = static_cast<uint32_t>(instanceRequiredExtensions.size()),
			.ppEnabledExtensionNames = instanceRequiredExtensions.data(),
		};

		if (validationLayersEnabled)
		{
			instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requestedValidationLayerNames.size());
			instanceCreateInfo.ppEnabledLayerNames = requestedValidationLayerNames.data();
		}

		VkResult result{ vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Vulkan instance!");
		}
	}

	void VulkanRenderer::create_logical_device()
	{
		// Get the indices of the queue families supported by the physical device, to create the logical device
		QueueFamilyIndices queueFamilyIndices{ get_queue_family_indices(m_device.physicalDevice) };

		// We use a set to avoid creating duplicate queues (e.g. if graphics queue is the same as presentation queue)
		std::set<int> queueFamilyUniqueIndices{
			queueFamilyIndices.graphicsFamily,
			queueFamilyIndices.presentationFamily
		};

		// Store all the create infos of the queues to later pass to logical device create info
		std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos{};
		deviceQueueCreateInfos.reserve(queueFamilyUniqueIndices.size());
		for (auto queueFamilyIndex : queueFamilyUniqueIndices)
		{
			// Create information for all queues required to create the logical device
			float priorities{ 1.0f }; // Used to assign priorities to queues, for now just one priority
			VkDeviceQueueCreateInfo deviceQueueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndex),
				.queueCount = 1,
				.pQueuePriorities = &priorities
			};
			deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
		}

		// To set required features (for future use)
		VkPhysicalDeviceFeatures requiredFeatures{};

		// Logical device (often called just "device" as opposed to "physical device")
		VkDeviceCreateInfo deviceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size()),
			.pQueueCreateInfos = deviceQueueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(requestedDeviceExtensionNames.size()),
			.ppEnabledExtensionNames = requestedDeviceExtensionNames.data(),
			.pEnabledFeatures = &requiredFeatures
		};

		VkResult result{ vkCreateDevice(m_device.physicalDevice, &deviceCreateInfo, nullptr, &m_device.logicalDevice) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a logical device!");
		}

		// Queues have now been created, we can retrieve necessary queues for the application
		// - graphicsFamily is really a queue family that supports graphics, not exclusive of other types
		// - queueIndex is 0 since we have only created one queue
		vkGetDeviceQueue(m_device.logicalDevice, queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_device.logicalDevice, queueFamilyIndices.presentationFamily, 0, &m_presentationQueue);
	}

	void VulkanRenderer::create_surface()
	{
		// Create a surface automatically with GLFW, runs the create surface function and returns the result
		VkResult result{ glfwCreateWindowSurface(m_instance, m_window.get_window_handle(), nullptr, &m_surface) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a surface!");
		}

	}

	void VulkanRenderer::create_swapchain()
	{
		// Get the swap chain details of the selected physical device
		SwapchainDetails swapchainDetails{ get_swap_chain_details(m_device.physicalDevice) };

		// Pick the best values to be used in the swapchain creation
		VkSurfaceFormatKHR selectedSurfaceFormat{ choose_surface_format(swapchainDetails.surfaceSupportedFormats) };
		VkPresentModeKHR selectedPresentationMode{ choose_presentation_mode(swapchainDetails.presentationModes) };
		VkExtent2D selectedExtent{ choose_swapchain_extent(swapchainDetails.surfaceCapabilities) };

		// Select a swapchain minimum image count. We should get one more than the minimum to allow triple buffering
		uint32_t imageCount{ swapchainDetails.surfaceCapabilities.minImageCount + 1 };
		if (swapchainDetails.surfaceCapabilities.maxImageCount > 0 // If it's 0, it has no limit
			&& swapchainDetails.surfaceCapabilities.maxImageCount < imageCount)
		{
			imageCount = swapchainDetails.surfaceCapabilities.maxImageCount;
		}		

		VkSwapchainCreateInfoKHR swapchainCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_surface,
			.minImageCount = imageCount,
			.imageFormat = selectedSurfaceFormat.format,
			.imageColorSpace = selectedSurfaceFormat.colorSpace,
			.imageExtent = selectedExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.preTransform = swapchainDetails.surfaceCapabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = selectedPresentationMode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE	// If this swapchain replaces another one, this may aid in the resource reuse
		};

		// We need to check if the swapchain can work exclusively with one queue queue family or not
		QueueFamilyIndices deviceQueueFamilyIndices{ get_queue_family_indices(m_device.physicalDevice) };
		uint32_t queueFamilyIndices[]{
			static_cast<uint32_t>(deviceQueueFamilyIndices.graphicsFamily),
			static_cast<uint32_t>(deviceQueueFamilyIndices.presentationFamily)
		};
		if (deviceQueueFamilyIndices.graphicsFamily != deviceQueueFamilyIndices.presentationFamily)
		{
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapchainCreateInfo.queueFamilyIndexCount = 2;
			swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		
		VkResult result{ vkCreateSwapchainKHR(m_device.logicalDevice, &swapchainCreateInfo, nullptr, &m_swapchain) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a swapchain!");
		}

		// We save these values now that we know they are compatible ans we will use them in the future
		m_swapchainImageFormat = selectedSurfaceFormat.format;
		m_swapchainExtent = selectedExtent;

		// Now we can fill the list of images that we can use from the swapchain
		uint32_t createdSwapchainImageCount{};
		vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &createdSwapchainImageCount, nullptr);
		std::vector<VkImage> createdSwapchainImages(createdSwapchainImageCount);
		vkGetSwapchainImagesKHR(m_device.logicalDevice, m_swapchain, &createdSwapchainImageCount, createdSwapchainImages.data());

		for (const auto& createdImage : createdSwapchainImages)
		{
			// Create corresponding image view to interface with the obtained swapchain image
			SwapchainImage swapChainImage{
				.image = createdImage,
				.imageView = create_image_view(createdImage, m_swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			};

			m_swapchainImages.push_back(swapChainImage);
		}

	}

	void VulkanRenderer::obtain_physical_device()
	{
		// Enumerate all physical devices available to our instance
		uint32_t physicalDeviceCount{};
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
		if (physicalDeviceCount == 0)
		{
			throw std::runtime_error("No devices available in the current instance!");
		}
		std::vector<VkPhysicalDevice> availablePhysicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, availablePhysicalDevices.data());

		// Select the first device that supports all requirements
		for (const auto& physicalDevice : availablePhysicalDevices)
		{
			if(device_supports_requirements(physicalDevice))
			{
				m_device.physicalDevice = physicalDevice;
				break;
			}
		}
	}

	QueueFamilyIndices VulkanRenderer::get_queue_family_indices(const VkPhysicalDevice& device) const
	{
		QueueFamilyIndices queueFamilyIndices{};

		// Get the number of queue families supported by the device to later query and obtain their indices,
		// returned list will be ordered by index
		uint32_t queueFamilyCount{};
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

		// Find the indices of queue families that have the required queues
		for (size_t i = 0; i < queueFamilyProperties.size(); ++i)
		{
			if (queueFamilyProperties[i].queueCount == 0)
			{
				continue;
			}

			// Found a queue family that supports graphics
			if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				queueFamilyIndices.graphicsFamily = static_cast<int>(i);
			}

			// Check if device supports the surface at the current queue family
			VkBool32 queueFamilySupportsPresentation{ false };
			vkGetPhysicalDeviceSurfaceSupportKHR(device, static_cast<uint32_t>(i), m_surface, &queueFamilySupportsPresentation);
			if (queueFamilySupportsPresentation)
			{
				queueFamilyIndices.presentationFamily = static_cast<int>(i);
			}

			if (queueFamilyIndices.are_all_valid())
			{
				// We can stop searching, all the queue families have been found
				break;
			}
		}
		return queueFamilyIndices;
	}

	SwapchainDetails VulkanRenderer::get_swap_chain_details(const VkPhysicalDevice& device) const
	{
		SwapchainDetails swapChainDetails{};

		// Get surface capabilities for a given surface on a given physical device
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapChainDetails.surfaceCapabilities);

		// Get the available formats
		uint32_t supportedFormatCount{};
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &supportedFormatCount, nullptr);
		if (supportedFormatCount > 0)
		{
			swapChainDetails.surfaceSupportedFormats.resize(supportedFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &supportedFormatCount, swapChainDetails.surfaceSupportedFormats.data());
		}

		// Get the supported presentation modes
		uint32_t presentationModeCount{};
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, nullptr);

		if (presentationModeCount > 0)
		{
			swapChainDetails.presentationModes.resize(presentationModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationModeCount, swapChainDetails.presentationModes.data());
		}

		return swapChainDetails;
	}

	bool VulkanRenderer::check_instance_extension_support(const std::vector<const char*>& requiredExtensionNames) const
	{
		// First we get the number of supported extensions, then we query again with a vector big enough
		uint32_t supportedExtensionCount{};
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
		std::vector<VkExtensionProperties> supportedExtensionProperties(supportedExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensionProperties.data());

		// Check that the extensions are in fact supported
		for (const auto& requiredExtensionName : requiredExtensionNames)
		{
			bool isSupported{ false };
			for (const auto& supportedExtension : supportedExtensionProperties)
			{
				if (strcmp(requiredExtensionName, supportedExtension.extensionName) == 0)
				{
					isSupported = true;
					break;
				}
			}
			if (!isSupported) return false;
		}
		return true;
	}

	// Check that the physical device passed by parameter supports all the requested extensions (defined in Utilities.h)
	bool VulkanRenderer::check_device_extension_support(const VkPhysicalDevice& device, 
														const std::vector<const char*>& requestedExtensionNames) const
	{
		uint32_t supportedExtensionCount{};
		vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, nullptr);
		if (supportedExtensionCount == 0)
		{
			return false;
		}
		std::vector<VkExtensionProperties> supportedExtensionProperties(supportedExtensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &supportedExtensionCount, supportedExtensionProperties.data());

		for (const auto& requestedDeviceExtensionName : requestedExtensionNames)
		{
			bool isSupported{ false };
			for (const auto& supportedExtension : supportedExtensionProperties)
			{
				if (strcmp(requestedDeviceExtensionName, supportedExtension.extensionName) == 0)
				{
					isSupported = true;
					break;
				}
			}
			if (!isSupported)
			{
				return false;
			}
		}
		return true;
	}

	bool VulkanRenderer::device_supports_requirements(const VkPhysicalDevice& device) const
	{
		//// General physical device properties and limits
		//VkPhysicalDeviceProperties physicalDeviceProperties;
		//vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

		//// Physical device features: geometry shader, tessellation shader, depth clamp, multi viewport...
		//VkPhysicalDeviceFeatures physicalDeviceFeatures;
		//vkGetPhysicalDeviceFeatures(device, &physicalDeviceFeatures);

		if (!check_device_extension_support(device, requestedDeviceExtensionNames))
		{
			return false;
		}

		// Get device swap chain details to check that there it supports some formats and/or presentation modes
		SwapchainDetails deviceSwapChainDetails{ get_swap_chain_details(device) };
		if (deviceSwapChainDetails.surfaceSupportedFormats.empty()
			|| deviceSwapChainDetails.presentationModes.empty())
		{
			return false;
		}

		// Get queue families to check if all the ones needed are supported and return true if that is the case
		QueueFamilyIndices deviceQueueFamilyIndices{ get_queue_family_indices(device) };

		return deviceQueueFamilyIndices.are_all_valid();
	}

	// For this particular program, the function tries to select:
	// - Format: VK_FORMAT_R8G8B8A8_UNORM
	// - Color space: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	VkSurfaceFormatKHR VulkanRenderer::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& surfaceFormatList) const
	{
		// In this case, all the formats are available
		if (surfaceFormatList.size() == 1 && surfaceFormatList[0].format == VK_FORMAT_UNDEFINED)
		{
			return {
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 
			};
		}

		// If not all are available, we look for the one we want
		for (const auto& surfaceFormat : surfaceFormatList)
		{
			if ((surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM || surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
				&& surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return surfaceFormat;
			}
		}

		// Otherwise just return the first one and hope it works (generally shouldn't happen)
		return surfaceFormatList[0];
	}

	// Try to select mailbox presentation mode, otherwise select FIFO mode
	VkPresentModeKHR VulkanRenderer::choose_presentation_mode(const std::vector<VkPresentModeKHR>& presentationModeList) const
	{
		for (const auto& presentationMode : presentationModeList)
		{
			if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return presentationMode;
			}
		}

		// We use this otherwise, since it should always be available according to specification
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanRenderer::choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const
	{
		// If this isn't set to the maximum value, the current extent is already set to a correct value
		if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>().max())
		{
			return surfaceCapabilities.currentExtent;
		}

		// Otherwise we need to set the extent manually
		int width, height;
		glfwGetFramebufferSize(m_window.get_window_handle(), &width, &height);

		VkExtent2D newExtent{
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		// We need to check that the new extent is inside the boundaries of the surface maximum and minimum extents
		newExtent.width = std::clamp(newExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		newExtent.height = std::clamp(newExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

		return newExtent;
	}

	VkImageView VulkanRenderer::create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo imageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY, // r = r
				.g = VK_COMPONENT_SWIZZLE_IDENTITY, // g = g
				.b = VK_COMPONENT_SWIZZLE_IDENTITY, // b = b
				.a = VK_COMPONENT_SWIZZLE_IDENTITY  // a = a
			},
			.subresourceRange = {
				.aspectMask = aspectFlags,	// Which aspect of the image to view (e.g. COLOR_BIT, DEPTH_BIT...)
				.baseMipLevel = 0,			// Start mipmap level to view from
				.levelCount = 1,			// Number of mipmap levels to view
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		VkImageView imageView;
		VkResult result{ vkCreateImageView(m_device.logicalDevice, &imageViewCreateInfo, nullptr, &imageView) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view!");
		}

		return imageView;
	}

	bool VulkanRenderer::check_validation_layer_support(const std::vector<const char*>& requestedValidationLayerNames) const
	{
		uint32_t enabledLayerCount{};
		vkEnumerateInstanceLayerProperties(&enabledLayerCount, nullptr);
		std::vector<VkLayerProperties> supportedLayerProperties(enabledLayerCount);
		vkEnumerateInstanceLayerProperties(&enabledLayerCount, supportedLayerProperties.data());

		for (const auto& requestedLayer : requestedValidationLayerNames)
		{
			bool isSupported{ false };
			for (const auto& supportedLayer : supportedLayerProperties)
			{
				if (strcmp(requestedLayer, supportedLayer.layerName) == 0)
				{
					isSupported = true;
					break;
				}
			}
			if (!isSupported) return false;
		}
		return true;
	}
}