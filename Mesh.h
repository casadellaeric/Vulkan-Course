#pragma once

#include "Utilities.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace VkCourse {

	struct UboModel {
		glm::mat4 model;
	};

	class Mesh
	{
	public:
		Mesh();
		Mesh(VkPhysicalDevice physicalDevice, VkDevice device, 
			VkQueue transferQueue, VkCommandPool transferCommandPool, 
			std::vector<Vertex>* vertices, std::vector<uint32_t>* indices);
		
		~Mesh();
		
		void destroy_buffers();

		uint32_t get_vertex_count();
		uint32_t get_index_count();

		VkBuffer get_vertex_buffer();
		VkBuffer get_index_buffer();

		void set_model(glm::mat4 modelMatrix);
		UboModel get_model_matrix();

	private:
		UboModel m_uboModel;

		uint32_t m_vertexCount{};
		VkBuffer m_vertexBuffer;
		VkDeviceMemory m_vertexBufferMemory;

		uint32_t m_indexCount{};
		VkBuffer m_indexBuffer;
		VkDeviceMemory m_indexBufferMemory;

		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;

		void create_vertex_buffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
			std::vector<Vertex>* vertices);
		void create_index_buffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
			std::vector<uint32_t>* indices);
	};
}

