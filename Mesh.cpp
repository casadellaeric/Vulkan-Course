#include "Mesh.h"

VkCourse::Mesh::Mesh()
{
}

VkCourse::Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device,
	VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<Vertex>* vertices, std::vector<uint32_t>* indices)
{
	m_vertexCount = static_cast<uint32_t>(vertices->size());
	m_indexCount = static_cast<uint32_t>(indices->size());
	m_device.physicalDevice = physicalDevice;
	m_device.logicalDevice = device;
	create_vertex_buffer(transferQueue, transferCommandPool, vertices);
	create_index_buffer(transferQueue, transferCommandPool, indices);
	m_uboModel = { .model = glm::mat4(1.f) };
}

VkCourse::Mesh::~Mesh()
{
}

void VkCourse::Mesh::destroy_buffers()
{
	vkDestroyBuffer(m_device.logicalDevice, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device.logicalDevice, m_vertexBufferMemory, nullptr);

	vkDestroyBuffer(m_device.logicalDevice, m_indexBuffer, nullptr);
	vkFreeMemory(m_device.logicalDevice, m_indexBufferMemory, nullptr);
}

uint32_t VkCourse::Mesh::get_vertex_count()
{
	return m_vertexCount;
}

uint32_t VkCourse::Mesh::get_index_count()
{
	return m_indexCount;
}

VkBuffer VkCourse::Mesh::get_vertex_buffer()
{
	return m_vertexBuffer;
}

VkBuffer VkCourse::Mesh::get_index_buffer()
{
	return m_indexBuffer;
}

void VkCourse::Mesh::set_model(glm::mat4 modelMatrix)
{
	m_uboModel.model = modelMatrix;
}

VkCourse::UboModel VkCourse::Mesh::get_model_matrix()
{
	return m_uboModel;
}

void VkCourse::Mesh::create_vertex_buffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<Vertex>* vertices)
{
	VkDeviceSize bufferSize{ sizeof(Vertex) * vertices->size() };

	// Create staging buffer (temporary buffer to store vertex data before transferring to GPU)
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	// Create host visible staging buffer and allocate memory to it
	create_buffer(m_device.physicalDevice, m_device.logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	// Map memory to staging buffer
	void* data;																			// Pointer to a point in normal memory
	vkMapMemory(m_device.logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);	// Map vertex buffer memory to data
	memcpy(data, vertices->data(), static_cast<size_t>(bufferSize));					// Copy memory from vertices to data
	vkUnmapMemory(m_device.logicalDevice, stagingBufferMemory);

	// Create GPU local vertex buffer with TRANSFER_DST_BIT to mark as recipient of transfer data
	create_buffer(m_device.physicalDevice, m_device.logicalDevice, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_vertexBuffer, &m_vertexBufferMemory);

	copy_buffer(m_device.logicalDevice, transferQueue, transferCommandPool, stagingBuffer, m_vertexBuffer, bufferSize);

	// Destroy temporary buffer
	vkDestroyBuffer(m_device.logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_device.logicalDevice, stagingBufferMemory, nullptr);
}

void VkCourse::Mesh::create_index_buffer(VkQueue transferQueue, VkCommandPool transferCommandPool,
	std::vector<uint32_t>* indices)
{
	// Very similar to create_vertex_buffer() above

	VkDeviceSize bufferSize{ sizeof(uint32_t) * indices->size() };

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	create_buffer(m_device.physicalDevice, m_device.logicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

	void* data;
	vkMapMemory(m_device.logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices->data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_device.logicalDevice, stagingBufferMemory);

	// Now the destination is for an index buffer
	create_buffer(m_device.physicalDevice, m_device.logicalDevice, bufferSize, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_indexBuffer, &m_indexBufferMemory);

	copy_buffer(m_device.logicalDevice, transferQueue, transferCommandPool, stagingBuffer, m_indexBuffer, bufferSize);

	vkDestroyBuffer(m_device.logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_device.logicalDevice, stagingBufferMemory, nullptr);
}