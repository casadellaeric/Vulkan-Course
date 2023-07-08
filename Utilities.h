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

	inline VkCommandBuffer begin_command_buffer(VkDevice device, VkCommandPool commandPool)
	{
		// Command buffer to hold transfer commands
		VkCommandBuffer commandBuffer;

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkResult result{ vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate transfer command buffer!");
		}

		VkCommandBufferBeginInfo commandBufferBeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// Only using it once to transfer
		};

		// Begin recording transfer commands
		result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a command buffer!");
		}

		return commandBuffer;
	}

	inline void end_and_submit_command_buffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		// Queue submission information
		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
		};

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

		// We wait here to avoid submitting too many command buffers and crashing the program if we had many calls to copy_buffer()
		vkQueueWaitIdle(queue);

		// Free temporary command buffer back to command pool
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	inline void copy_buffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool, 
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
	{
		VkCommandBuffer transferCommandBuffer{ begin_command_buffer(device, transferCommandPool) };

		// Region of data to copy from and to
		VkBufferCopy bufferCopyRegion{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = bufferSize,
		};

		// Copy source buffer to destination buffer
		vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

		end_and_submit_command_buffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
	}

	inline void copy_image_buffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
		VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height)
	{
		VkCommandBuffer transferCommandBuffer{ begin_command_buffer(device, transferCommandPool) };

		VkBufferImageCopy bufferImageCopyRegion{
			.bufferOffset = 0,
			.bufferRowLength = 0,		// For data spacing calculation (if 0 --> tightly packed)
			.bufferImageHeight = 0,
			.imageSubresource{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageOffset{ 0, 0, 0 }, 
			.imageExtent{
				.width = width,
				.height = height,
				.depth = 1,
			},
		};

		vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, dstImage, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopyRegion);

		end_and_submit_command_buffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
	}

	inline void transition_image_layout(VkDevice device, VkQueue queue, VkCommandPool commandPool, 
		VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer{ begin_command_buffer(device, commandPool) };

		VkImageMemoryBarrier imageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,		// Queue family to transition from
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,		// Queue family to transition to
			.image = image,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};

		VkPipelineStageFlags srcStage{};
		VkPipelineStageFlags dstStage{};

		// If transitioning from new image to image ready to receive data
		if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = 0;								// Transition must happen after any stage
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		// Transition must happen before the transfer

			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		// If transitioning from transfer destination to shader readable
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
		{
			throw std::runtime_error("Unspecified layouts in transition_image_layout()!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			srcStage, dstStage,		// Pipeline stages (match to src and dst access masks previously specified)
			0,						// Dependency flags
			0, nullptr,				// Memory barrier + data
			0, nullptr,				// Buffer memory barrier + data
			1, &imageMemoryBarrier	// Image memory barrier + data
		);

		end_and_submit_command_buffer(device, commandPool, queue, commandBuffer);
	}
}