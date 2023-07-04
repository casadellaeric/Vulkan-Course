#include "Mesh.h"

VkCourse::Mesh::Mesh()
{
}

VkCourse::Mesh::Mesh(VkPhysicalDevice physicalDevice, VkDevice device, std::vector<Vertex>* vertices)
{
	m_vertexCount = static_cast<uint32_t>(vertices->size());
	m_device.physicalDevice = physicalDevice;
	m_device.logicalDevice = device;
	create_vertex_buffer(vertices);
}

VkCourse::Mesh::~Mesh()
{
	//destroy_vertex_buffer();
}

void VkCourse::Mesh::destroy_vertex_buffer()
{
	vkDestroyBuffer(m_device.logicalDevice, m_vertexBuffer, nullptr);
	vkFreeMemory(m_device.logicalDevice, m_vertexBufferDeviceMemory, nullptr);
}

uint32_t VkCourse::Mesh::get_vertex_count()
{
	return m_vertexCount;
}

VkBuffer VkCourse::Mesh::get_vertex_buffer()
{
	return m_vertexBuffer;
}

void VkCourse::Mesh::create_vertex_buffer(std::vector<Vertex>* vertices)
{
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex) * vertices->size(),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,		// Should not be used by multiple queues
	};

	VkResult result{ vkCreateBuffer(m_device.logicalDevice, &bufferCreateInfo, nullptr, &m_vertexBuffer) };
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a vertex buffer!");
	}

	// Get buffer memory requirements
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_device.logicalDevice, m_vertexBuffer, &memoryRequirements);

	// Allocate memory to buffer
	VkMemoryAllocateInfo memoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = find_memory_type_index(memoryRequirements.memoryTypeBits,	// Memory type on physical device with required bit flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),// CPU can interact with it + data straight into buffer after mapping
	};

	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(m_device.logicalDevice, &memoryAllocateInfo, nullptr, &m_vertexBufferDeviceMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory!");
	}

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(m_device.logicalDevice, m_vertexBuffer, m_vertexBufferDeviceMemory, 0);

	// Map memory to vertex buffer
	void* data;																								// Pointer to a point in normal memory
	vkMapMemory(m_device.logicalDevice, m_vertexBufferDeviceMemory, 0, bufferCreateInfo.size, 0, &data);	// Map vertex buffer memory to data
	memcpy(data, vertices->data(), static_cast<size_t>(bufferCreateInfo.size));								// Copy memory from vertices to data
	vkUnmapMemory(m_device.logicalDevice, m_vertexBufferDeviceMemory);
}

uint32_t VkCourse::Mesh::find_memory_type_index(uint32_t allowedTypes, VkMemoryPropertyFlags memoryPropertyFlags)
{
	// Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_device.physicalDevice, &physicalDeviceMemoryProperties);

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
