#pragma once

#include "Utilities.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace VkCourse {

	class Mesh
	{
	public:
		Mesh();
		Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices);
		
		~Mesh();

		void destroy_vertex_buffer();

		uint32_t get_vertex_count();
		VkBuffer get_vertex_buffer();

	private:
		uint32_t m_vertexCount{};
		VkBuffer m_vertexBuffer;
		VkDeviceMemory m_vertexBufferDeviceMemory;

		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;

		void create_vertex_buffer(std::vector<Vertex>* vertices);
		uint32_t find_memory_type_index(uint32_t allowedTypes, VkMemoryPropertyFlags memoryPropertyFlags);
	};
}

