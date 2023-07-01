#pragma once

namespace VkCourse
{
	const std::vector<const char*> requestedDeviceExtensionNames{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	// Indices of queue families (-1 if invalid)
	struct QueueFamilyIndices {
		int graphicsFamily = -1;
		int presentationFamily = -1;

		bool are_all_valid()
		{
			return (
				graphicsFamily >= 0 && 
				presentationFamily >= 0);
		}
	};

	struct SwapchainDetails {
		VkSurfaceCapabilitiesKHR surfaceCapabilities;				// e.g. image size/extent
		std::vector<VkSurfaceFormatKHR> surfaceSupportedFormats;	// e.g. R8G8B8
		std::vector<VkPresentModeKHR> presentationModes;			// e.g. Immediate, mailbox, FIFO
	};

	struct SwapchainImage {
		VkImage image;				
		VkImageView imageView;
	};
}