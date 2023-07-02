#pragma once

#include <fstream>
#include <string_view>

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

	inline std::vector<char> readFile(std::string_view fileName)
	{
		// Binary (we are reading SPIR-V) + start reading from the end of file (to know the size)
		std::ifstream file(fileName.data(), std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file" + std::string(fileName) + "!");
		}

		size_t fileSize{ static_cast<size_t>(file.tellg()) };
		std::vector<char> fileBuffer(fileSize);

		// Go back to position 0 to start reading file
		file.seekg(0);
		file.read(fileBuffer.data(), fileSize);
		file.close();

		return fileBuffer;
	}
}