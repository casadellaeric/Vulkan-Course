#pragma once

#include "Window.h"
#include "Utilities.h"
#include "Mesh.h"
#include "MeshModel.h"

#include "stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Specific to Vulkan, as opposed to -1 to 1 (OpenGL)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
		void draw();
		void destroy();

		size_t create_mesh_model(const std::string& modelFileName);
		void update_model_matrix(size_t modelId, glm::mat4 modelMatrix);

	private:
		const Window& m_window;

		// Keeps track of which frame between 0 and MAX_FRAMES - 1, is being rendered to inside draw()
		unsigned int m_currentFrame{ 0 };

		// Scene objects
		std::vector<MeshModel> m_meshModels{};

		// Scene settings
		struct UboViewProjection {
			glm::mat4 view;
			glm::mat4 projection;
		} m_uboViewProjection;

		// Main Vulkan components
		VkInstance m_instance;
		struct {
			VkPhysicalDevice physicalDevice;
			VkDevice logicalDevice;
		} m_device;
		QueueFamilyIndices m_queueFamilyIndices;
		VkQueue m_graphicsQueue;
		VkQueue m_presentationQueue;
		VkSurfaceKHR m_surface;
		VkSwapchainKHR m_swapchain;
		std::vector<SwapchainImage> m_swapchainImages{};
		std::vector<VkFramebuffer> m_swapchainFramebuffers{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		std::vector<VkImage> m_colorBufferImages;
		std::vector<VkDeviceMemory> m_colorBufferImageMemories;
		std::vector<VkImageView> m_colorBufferImageViews;
		VkFormat m_colorBufferFormat;

		std::vector<VkImage> m_depthBufferImages;
		std::vector<VkDeviceMemory> m_depthBufferImageMemories;
		std::vector<VkImageView> m_depthBufferImageViews;
		VkFormat m_depthBufferFormat;

		VkSampler m_textureSampler;

		// Descriptors
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkDescriptorSetLayout m_samplerSetLayout;
		VkDescriptorSetLayout m_inputAttachmentSetLayout;
		VkPushConstantRange m_pushConstantRange;

		VkDescriptorPool m_descriptorPool;
		VkDescriptorPool m_samplerDescriptorPool;
		VkDescriptorPool m_inputAttachmentDescriptorPool;
		std::vector<VkDescriptorSet> m_descriptorSets{};
		std::vector<VkDescriptorSet> m_samplerDescriptorSets{};
		std::vector<VkDescriptorSet> m_inputAttachmentDescriptorSets{};

		std::vector<VkBuffer> m_vpUniformBuffers{};
		std::vector<VkDeviceMemory> m_vpUniformBufferMemories{};

		// std::vector<VkBuffer> m_modelDynamicUniformBuffers{};
		// std::vector<VkDeviceMemory> m_modelDynamicUniformBufferMemories{};
		// VkDeviceSize m_minUniformBufferOffset;
		// size_t m_modelUniformAlignment{};
		// Model* m_modelTransferSpace{};

		// Assets
		std::vector<VkImage> m_textureImages{};
		std::vector<VkDeviceMemory> m_textureImageMemories{};		// An optimal layout would have only one memory accessed with offsets
		std::vector<VkImageView> m_textureImageViews{};

		// Pipeline
		VkPipeline m_graphicsPipeline;
		VkPipelineLayout m_pipelineLayout;

		VkPipeline m_secondPipeline;
		VkPipelineLayout m_secondPipelineLayout;

		VkRenderPass m_renderPass;

		// Pools
		VkCommandPool m_graphicsCommandPool;

		// Secondary Vulkan components
		VkFormat m_swapchainImageFormat;
		VkExtent2D m_swapchainExtent;

		// Synchronization
		std::vector<VkSemaphore> m_semaphoresImageAvailable{};
		std::vector<VkSemaphore> m_semaphoresRenderFinished{};
		std::vector<VkFence> m_fencesDraw{};

		// Vulkan functions
		// - Create functions
		void create_instance();
		void create_logical_device();
		void create_surface();
		void create_swapchain();
		void create_render_pass();
		void create_descriptor_set_layout();
		void create_push_constant_range();
		void create_graphics_pipeline();
		void create_color_buffer_image();
		void create_depth_buffer_image();
		void create_framebuffers();
		void create_command_pool();
		void create_command_buffers();
		void create_synchronization();
		void create_texture_sampler();

		void create_uniform_buffers();
		void create_descriptor_pool();
		void create_descriptor_sets();
		void create_input_descriptor_sets();

		void update_uniform_buffers(uint32_t imageIndex);

		// - Record functions
		void record_commands(uint32_t imageIndex);

		// - Get/Obtain functions
		// Not a getter, obtains the physical device to initialize m_device.physicalDevice
		void obtain_physical_device();
		QueueFamilyIndices get_queue_family_indices(const VkPhysicalDevice& device) const;
		SwapchainDetails get_swap_chain_details(const VkPhysicalDevice& device) const;

		// Allocate functions
		void allocate_dynamic_buffer_transfer_space();

		// - Support functions
		// -- Check functions
		bool check_instance_extension_support(const std::vector<const char*>& requestedExtensionNames) const;
		bool check_device_extension_support(const VkPhysicalDevice& device, const std::vector<const char*>& requestedExtensionNames) const;
		bool check_validation_layer_support(const std::vector<const char*>& requestedValidationLayerNames) const;
		bool device_supports_requirements(const VkPhysicalDevice& device) const;

		// -- Choose functions
		VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& surfaceFormatList) const;
		VkPresentModeKHR choose_presentation_mode(const std::vector<VkPresentModeKHR>& presentationModeList) const;
		VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& surfaceCapabilities) const;
		VkFormat choose_supported_format(const std::vector<VkFormat>& formatList, VkImageTiling imageTiling, 
			VkFormatFeatureFlags featureFlags) const;

		// -- Create functions (reusable)
		VkImage create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags,
			VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceMemory* imageMemory);
		VkImageView create_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
		VkShaderModule create_shader_module(const std::vector<char>& code);

		size_t create_texture_image(const std::string& fileName);
		size_t create_texture(const std::string& fileName);
		size_t create_texture_descriptor(VkImageView textureImage);

		// -- Loader functions
		stbi_uc* load_texture_file(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize);
	};
}