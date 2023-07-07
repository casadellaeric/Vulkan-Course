#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <fstream>
#include <string_view>
#include <glm/glm.hpp>

namespace VkCourse
{
	// Number of simultaneous frames that can be in use
	constexpr unsigned int MAX_FRAME_DRAWS{ 2 };

	// Used to allocate memory for enough dynamic uniform buffers
	constexpr unsigned int MAX_OBJECTS{ 2 };

	const std::vector<const char*> requestedDeviceExtensionNames{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	struct Vertex {
		glm::vec3 position;
		glm::vec3 color;		// RGB
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

	inline std::vector<char> read_file(std::string_view fileName)
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

	inline uint32_t find_memory_type_index(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags memoryPropertyFlags)
	{
		// Get properties of physical device memory
		VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

		for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; ++i)
		{
			// Each memory type corresponds to one bit in the bit field allowedTypes
			if ((allowedTypes & (1 << i))
				// memoryTypes[i].properties is equal to the parameter memoryPropertyFlags
				&& ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags))
			{
				return i;
			}
		}

		throw std::runtime_error("Failed to find memory type with requested properties!");
	}

	inline void create_buffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, 
		VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer* buffer, 
		VkDeviceMemory* bufferMemory)
	{
		VkBufferCreateInfo bufferCreateInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = bufferSize,
			.usage = bufferUsageFlags,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,		// Should not be used by multiple queues
		};

		VkResult result{ vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a vertex buffer!");
		}

		// Get buffer memory requirements
		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

		// Allocate memory to buffer
		VkMemoryAllocateInfo memoryAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = find_memory_type_index(physicalDevice, memoryRequirements.memoryTypeBits, memoryPropertyFlags),
		};

		// Allocate memory to VkDeviceMemory
		result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate vertex buffer memory!");
		}

		// Allocate memory to given vertex buffer
		vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
	}

	inline void copy_buffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
	{
		// Command buffer to hold transfer commands
		VkCommandBuffer transferCommandBuffer;

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = transferCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkResult result{ vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &transferCommandBuffer) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate transfer command buffer!");
		}

		VkCommandBufferBeginInfo commandBufferBeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// Only using it once to transfer
		};

		// Begin recording transfer commands
		result = vkBeginCommandBuffer(transferCommandBuffer, &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a command buffer!");
		}

		// Region of data to copy from and to
		VkBufferCopy bufferCopyRegion{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = bufferSize,
		};

		// Copy source buffer to destination buffer
		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);
		vkEndCommandBuffer(transferCommandBuffer);

		// Queue submission information
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &transferCommandBuffer,
		};

		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

		// We wait here to avoid submitting too many command buffers and crashing the program if we had many calls to copy_buffer()
		vkQueueWaitIdle(transferQueue);

		// Free temporary command buffer back to command pool
		vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
	}

}