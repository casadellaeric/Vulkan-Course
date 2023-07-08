#include "VulkanRenderer.h"
#include "Utilities.h"
#include "Mesh.h"

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <array>


namespace VkCourse
{
	VulkanRenderer::VulkanRenderer(const Window& window)
		: m_window(window)
	{
	}

	VulkanRenderer::~VulkanRenderer()
	{
		destroy();
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
			create_depth_buffer_image();
			create_render_pass();
			create_descriptor_set_layout();
			create_push_constant_range();
			create_graphics_pipeline();
			create_framebuffers();
			create_command_pool();

			size_t firstTexture{ create_texture("wiseTree.png") };
			
			// Fill mvp
			m_uboViewProjection.projection = glm::perspective(glm::radians(45.f),
				static_cast<float>(m_swapchainExtent.width) / static_cast<float>(m_swapchainExtent.height), 0.1f, 100.f);

			m_uboViewProjection.view = glm::lookAt(glm::vec3(2.f, 1.f, 2.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
			
			m_uboViewProjection.projection[1][1] *= -1.f; // Invert the Y axis to fit Vulkan

			// Create a mesh
			std::vector<Vertex> meshVertices{
				{{ -0.2f,  0.4f,  0.f }, {  1.f,  0.f,  0.f }},	// position, color
				{{  0.2f,  0.4f,  0.f }, {  1.f,  0.f,  0.f }},
				{{  0.2f, -0.4f,  0.f }, {  1.f,  0.f,  0.f }},
				{{ -0.2f, -0.4f,  0.f }, {  1.f,  0.f,  0.f }},
			};

			std::vector<Vertex> meshVertices2{
				{{ -0.2f,  0.4f,  0.f }, {  0.f,  0.f,  1.f }},	// position, color
				{{  0.2f,  0.4f,  0.f }, {  0.f,  0.f,  1.f }},
				{{  0.2f, -0.4f,  0.f }, {  0.f,  0.f,  1.f }},
				{{ -0.2f, -0.4f,  0.f }, {  0.f,  0.f,  1.f }},
			};
			
			std::vector<uint32_t> meshIndices{
				0, 3, 2,
				2, 1, 0,
			};

			m_meshes.reserve(2);

			// Mesh constructor
			m_meshes.emplace_back(m_device.physicalDevice, m_device.logicalDevice,
				m_graphicsQueue, m_graphicsCommandPool, &meshVertices, &meshIndices);
			
			m_meshes.emplace_back(m_device.physicalDevice, m_device.logicalDevice,
				m_graphicsQueue, m_graphicsCommandPool, &meshVertices2, &meshIndices);

			create_command_buffers();
			//allocate_dynamic_buffer_transfer_space();
			create_uniform_buffers();
			create_descriptor_pool();
			create_descriptor_sets();
			create_synchronization();
		}
		catch (const std::runtime_error& error)
		{
			std::cout << error.what() << std::endl;
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	void VulkanRenderer::draw()
	{
		// Wait before the previous render to the current frame has finished to start, and close it (unsignal fence)
		vkWaitForFences(m_device.logicalDevice, 1, &m_fencesDraw[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkResetFences(m_device.logicalDevice, 1, &m_fencesDraw[m_currentFrame]);

		// Get a new image to render to, get a semaphore to know when the image is available
		uint32_t imageIndex;
		vkAcquireNextImageKHR(m_device.logicalDevice, m_swapchain, std::numeric_limits<uint64_t>::max(), 
			m_semaphoresImageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

		record_commands(imageIndex);
		update_uniform_buffers(imageIndex);

		// Submit a command buffer to queue, wait for semaphore and signal after
		VkPipelineStageFlags pipelineWaitStages[]{	// Stages to wait at for the given semaphores
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_semaphoresImageAvailable[m_currentFrame],
			.pWaitDstStageMask = pipelineWaitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &m_commandBuffers[imageIndex],
			.signalSemaphoreCount = 1,		// This will be signaled when the command buffer is finished
			.pSignalSemaphores = &m_semaphoresRenderFinished[m_currentFrame],
		};

		// Submit commands to the queue, signal the fence so that the next access to the same frame can start rendering
		VkResult result{ vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fencesDraw[m_currentFrame])};
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to subit command buffer to graphics queue!");
		}

		// Present rendered image to the screen
		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_semaphoresRenderFinished[m_currentFrame],
			.swapchainCount = 1,
			.pSwapchains = &m_swapchain,
			.pImageIndices = &imageIndex,
		};

		result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present image!");
		}

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAME_DRAWS;
	}

	void VulkanRenderer::destroy()
	{
		// Destroy in inverse order of creation
		
		// Wait for the device to be idle before destroying semaphores, command pools...
		vkDeviceWaitIdle(m_device.logicalDevice);

		//_aligned_free(m_modelTransferSpace);

		for (size_t i = 0; i < m_textureImages.size(); ++i) 
		{
			vkDestroyImage(m_device.logicalDevice, m_textureImages[i], nullptr);
			vkFreeMemory(m_device.logicalDevice, m_textureImageMemories[i], nullptr);
		}

		vkDestroyImageView(m_device.logicalDevice, m_depthBufferImageView, nullptr);
		vkDestroyImage(m_device.logicalDevice, m_depthBufferImage, nullptr);
		vkFreeMemory(m_device.logicalDevice, m_depthBufferImageMemory, nullptr);

		vkDestroyDescriptorPool(m_device.logicalDevice, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(m_device.logicalDevice, m_descriptorSetLayout, nullptr);
		for (size_t i = 0; i < m_swapchainImages.size(); ++i)
		{
			vkDestroyBuffer(m_device.logicalDevice, m_vpUniformBuffers[i], nullptr);
			vkFreeMemory(m_device.logicalDevice, m_vpUniformBufferMemories[i], nullptr);
			//vkDestroyBuffer(m_device.logicalDevice, m_modelDynamicUniformBuffers[i], nullptr);
			//vkFreeMemory(m_device.logicalDevice, m_modelDynamicUniformBufferMemories[i], nullptr);
		}
		for (auto& mesh : m_meshes)
		{
			mesh.destroy_buffers();
		}
		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i) {
			vkDestroySemaphore(m_device.logicalDevice, m_semaphoresRenderFinished[i], nullptr);
			vkDestroySemaphore(m_device.logicalDevice, m_semaphoresImageAvailable[i], nullptr);
			vkDestroyFence(m_device.logicalDevice, m_fencesDraw[i], nullptr);
		}
		vkDestroyCommandPool(m_device.logicalDevice, m_graphicsCommandPool, nullptr);
		for (const auto& framebuffer : m_swapchainFramebuffers)
		{
			vkDestroyFramebuffer(m_device.logicalDevice, framebuffer, nullptr);
		}
		vkDestroyPipeline(m_device.logicalDevice, m_graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(m_device.logicalDevice, m_pipelineLayout, nullptr);
		vkDestroyRenderPass(m_device.logicalDevice, m_renderPass, nullptr);
		for (const auto& image : m_swapchainImages)
		{
			vkDestroyImageView(m_device.logicalDevice, image.imageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device.logicalDevice, m_swapchain, nullptr);
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyDevice(m_device.logicalDevice, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	void VulkanRenderer::update_model_matrix(unsigned int modelId, glm::mat4 modelMatrix)
	{
		if (modelId >= m_meshes.size()) return;
		m_meshes[modelId].set_model(modelMatrix);
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
		m_queueFamilyIndices = get_queue_family_indices(m_device.physicalDevice);

		// We use a set to avoid creating duplicate queues (e.g. if graphics queue is the same as presentation queue)
		std::set<int> queueFamilyUniqueIndices{
			m_queueFamilyIndices.graphicsFamily,
			m_queueFamilyIndices.presentationFamily
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
		vkGetDeviceQueue(m_device.logicalDevice, static_cast<uint32_t>(m_queueFamilyIndices.graphicsFamily), 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_device.logicalDevice, static_cast<uint32_t>(m_queueFamilyIndices.presentationFamily), 0, &m_presentationQueue);
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

	void VulkanRenderer::create_render_pass()
	{
		// Attachments
		// Color attachment of render pass
		VkAttachmentDescription colorAttachmentDescription{
			.format = m_swapchainImageFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,					// Number of samples to write for multisampling
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,				// What to do before rendering
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,			// What to do after rendering
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,		// Format to present to surface
		};

		// Depth attachment of render pass
		VkAttachmentDescription depthAttachmentDescription{
			.format = m_depthBufferFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_NONE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		std::array<VkAttachmentDescription, 2> renderPassAttachmentDescriptions{
			colorAttachmentDescription, 
			depthAttachmentDescription 
		};

		// Attachment reference uses an index to refer to the attachment passed to the renderPassCreateInfo
		VkAttachmentReference colorAttachmentReference{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depthAttachmentReference{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpassDescription{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentReference,
			.pDepthStencilAttachment = &depthAttachmentReference,
		};

		// Need to determine when layout transitions occur using subpass dependencies
		std::array<VkSubpassDependency, 2> subpassDependencies{};
		// Transition must happen after...
		subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;		// This stage has to happen before the transition
		subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;				// It has to be read from, before the conversion
		// Transition must happen before...
		subpassDependencies[0].dstSubpass = 0;	//Id of 1st subpass
		subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependencies[0].dependencyFlags = 0;

		// Transition must happen after...
		subpassDependencies[1].srcSubpass = 0;
		subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		// Transition must happen before...
		subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpassDependencies[1].dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassCreateInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = static_cast<uint32_t>(renderPassAttachmentDescriptions.size()),
			.pAttachments = renderPassAttachmentDescriptions.data(),
			.subpassCount = 1,
			.pSubpasses = &subpassDescription,
			.dependencyCount = static_cast<uint32_t>(subpassDependencies.size()),
			.pDependencies = subpassDependencies.data(),
		};

		VkResult result{ vkCreateRenderPass(m_device.logicalDevice, &renderPassCreateInfo, nullptr, &m_renderPass) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a render pass!");
		}
	}

	void VulkanRenderer::create_descriptor_set_layout()
	{
		// MVP binding information
		VkDescriptorSetLayoutBinding vpLayoutBinding{
			.binding = 0,											// Corresponds to the binding in the shader
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	// Type (uniform, dynamic uniform, image sampler...)
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr,							// For textures, can make sampler immutable
		};

		/*VkDescriptorSetLayoutBinding modelLayoutBinding{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr,
		};*/

		std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{ 
			vpLayoutBinding, 
			// modelLayoutBinding 
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size()),
			.pBindings = descriptorSetLayoutBindings.data(),
		};

		VkResult result{ vkCreateDescriptorSetLayout(m_device.logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a descriptor set layout!");
		}
	}

	void VulkanRenderer::create_push_constant_range()
	{
		// Push constant values (in this case for the model matrix)
		m_pushConstantRange = {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(Model),
		};
	}

	void VulkanRenderer::create_graphics_pipeline()
	{
		// Read already compiled SPIR-V shaders
		std::vector<char> vertexShaderCode{ read_file("Shaders/vert.spv") };
		std::vector<char> fragmentShaderCode{ read_file("Shaders/frag.spv") };

		// Build shader modules to link to graphics pipeline
		VkShaderModule vertexShaderModule{ create_shader_module(vertexShaderCode) };
		VkShaderModule fragmentShaderModule{ create_shader_module(fragmentShaderCode) };

		// Configure stages of the pipeline with create infos
		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShaderModule,
			.pName = "main"
		};

		VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShaderModule,
			.pName = "main"
		};

		VkPipelineShaderStageCreateInfo shaderStages[]{ 
			vertexShaderStageCreateInfo, 
			fragmentShaderStageCreateInfo 
		};

		// How the data for a single vertex (position, color, etc.) is as a whole
		VkVertexInputBindingDescription vertexInputBindingDescription{
			.binding = 0,								// Can bind multiple streams of data, this defines which one
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,	// How to move between data after each vertex (in this case, next vertex)
		};

		// How the data for an attribute is defined within a vertex
		std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributeDescriptions{};
		// Position
		vertexInputAttributeDescriptions[0] = {
			.location = 0,							// Should match in the vertex shader
			.binding = 0,							// Should be same as above (unless multiple streams of data)
			.format = VK_FORMAT_R32G32B32_SFLOAT,	// Format the data will take + size
			.offset = offsetof(Vertex, position),
		};
		// Color
		vertexInputAttributeDescriptions[1] = {
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color),
		};

		// -- Vertex input --
		VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertexInputBindingDescription,				// e.g. data spacing, stride...
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size()),
			.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data(),	// data format, where to bind to/from
		};

		// -- Input Assembly --
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE		// Used for strip topology to start new primitives
		};

		// -- Viewport and scissor --
		VkViewport viewport{
			.x = 0.f,
			.y = 0.f,
			.width = static_cast<float>(m_swapchainExtent.width),
			.height = static_cast<float>(m_swapchainExtent.height),
			.minDepth = 0.f,
			.maxDepth = 1.f
		};

		VkRect2D scissor{
			.offset = { 0, 0 },
			.extent = m_swapchainExtent,
		};

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = &viewport,
			.scissorCount = 1,
			.pScissors = &scissor,
		};

		// -- Dynamic state -- (don't need to resize now)
		//std::vector<VkDynamicState> dynamicStateEnables{};
		//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);	// Dynamic viewport (can resize in command buffer with vkCmdSetViewport)
		//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);	// Dynamic scissor	(can resize in command buffer with vkCmdSetScissor)

		//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
		//	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		//	.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size()),
		//	.pDynamicStates = dynamicStateEnables.data(),
		//};

		// -- Rasterizer --
		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,			// Change if fragments beyond near/far planes are clipped or clamped to plane
			.rasterizerDiscardEnable = VK_FALSE,	// Discard primitives before fragment shader
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,	// Due to Vulkan inverted coordinate system
			.depthBiasEnable = VK_FALSE,
			.lineWidth = 1.f,
		};

		// -- Multisampling --
		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};

		// -- Blending --
		// ---- Blending eq: (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
							| VK_COLOR_COMPONENT_G_BIT
							| VK_COLOR_COMPONENT_B_BIT
							| VK_COLOR_COMPONENT_A_BIT,
		};

		VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,		// Alternative to calculations
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachmentState,
		};

		// -- Layout --
		VkPipelineLayoutCreateInfo layoutCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &m_descriptorSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &m_pushConstantRange,
		};

		VkResult result{ vkCreatePipelineLayout(m_device.logicalDevice, &layoutCreateInfo, nullptr, &m_pipelineLayout) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout!");
		}

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,			// Check depth value is between bounds
			.stencilTestEnable = VK_FALSE,
		};
		
		// Final graphics pipeline
		VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputStateCreateInfo,
			.pInputAssemblyState = &inputAssemblyStateCreateInfo,
			.pViewportState = &viewportStateCreateInfo,
			.pRasterizationState = &rasterizationStateCreateInfo,
			.pMultisampleState = &multisampleStateCreateInfo,
			.pDepthStencilState = &depthStencilStateCreateInfo,
			.pColorBlendState = &colorBlendStateCreateInfo,
			.pDynamicState = nullptr,
			.layout = m_pipelineLayout,
			.renderPass = m_renderPass,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,	// Used to create pipelines based on other pipelines
			.basePipelineIndex = -1,
		};

		result = vkCreateGraphicsPipelines(m_device.logicalDevice, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a pipeline!");
		}

		// Destroy shader modules (no longer needed after pipeline has been created)
		vkDestroyShaderModule(m_device.logicalDevice, fragmentShaderModule, nullptr);
		vkDestroyShaderModule(m_device.logicalDevice, vertexShaderModule, nullptr);
	}

	void VulkanRenderer::create_depth_buffer_image()
	{
		// Supported format for depth buffer
		m_depthBufferFormat = choose_supported_format(
			{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);

		m_depthBufferImage = create_image(m_swapchainExtent.width, m_swapchainExtent.height,
			m_depthBufferFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_depthBufferImageMemory);

		m_depthBufferImageView = create_image_view(m_depthBufferImage, m_depthBufferFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void VulkanRenderer::create_framebuffers()
	{
		// We want to create one framebuffer for each of the images of the swapchain
		m_swapchainFramebuffers.resize(m_swapchainImages.size());
		for (size_t i = 0; i < m_swapchainFramebuffers.size(); ++i)
		{
			std::array<VkImageView, 2> attachments{
				m_swapchainImages[i].imageView,
				m_depthBufferImageView
			};

			VkFramebufferCreateInfo framebufferCreateInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = m_renderPass,										// Render pass the framebuffer will be used with
				.attachmentCount = static_cast<uint32_t>(attachments.size()),
				.pAttachments = attachments.data(),								// List of attachments (1:1 with render pass)
				.width = m_swapchainExtent.width,
				.height = m_swapchainExtent.height,
				.layers = 1,
			};

			VkResult result{ vkCreateFramebuffer(m_device.logicalDevice, &framebufferCreateInfo, nullptr, &m_swapchainFramebuffers[i]) };
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create a framebuffer");
			}
		}
	}

	void VulkanRenderer::create_command_pool()
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,						// Command buffers can be reset to re-record commands (for push constants)
			.queueFamilyIndex = static_cast<uint32_t>(m_queueFamilyIndices.graphicsFamily),	// Buffers from this command pool will use this type of family
		};

		VkResult result{ vkCreateCommandPool(m_device.logicalDevice, &commandPoolCreateInfo, nullptr, &m_graphicsCommandPool) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a command pool!");
		}
	}

	void VulkanRenderer::create_command_buffers()
	{
		m_commandBuffers.resize(m_swapchainFramebuffers.size());

		VkCommandBufferAllocateInfo commandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_graphicsCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,	// Can't be called from other command buffers, only directly from the queue
			.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size()),
		};

		VkResult result{ vkAllocateCommandBuffers(m_device.logicalDevice, &commandBufferAllocateInfo, m_commandBuffers.data()) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers!");
		}
	}

	void VulkanRenderer::create_synchronization()
	{
		m_semaphoresImageAvailable.resize(MAX_FRAME_DRAWS);
		m_semaphoresRenderFinished.resize(MAX_FRAME_DRAWS);
		m_fencesDraw.resize(MAX_FRAME_DRAWS);

		// GPU-GPU synchronization
		VkSemaphoreCreateInfo semaphoreCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
		{
			if (vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_semaphoresImageAvailable[i]) != VK_SUCCESS
				|| vkCreateSemaphore(m_device.logicalDevice, &semaphoreCreateInfo, nullptr, &m_semaphoresRenderFinished[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create a semaphore");
			}
		}

		// GPU-CPU synchronization
		VkFenceCreateInfo fenceCreateInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
		{
			if (vkCreateFence(m_device.logicalDevice, &fenceCreateInfo, nullptr, &m_fencesDraw[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create a fence!");
			}
		}
	}

	void VulkanRenderer::create_uniform_buffers()
	{
		// Buffer size to store ViewProjection and model
		VkDeviceSize vpBufferSize{ sizeof(UboViewProjection) };
		// VkDeviceSize modelBufferSize{ m_modelUniformAlignment * MAX_OBJECTS };

		// One uniform buffer for each swapchain image/command buffer
		m_vpUniformBuffers.resize(m_swapchainImages.size());
		m_vpUniformBufferMemories.resize(m_swapchainImages.size());
		//m_modelDynamicUniformBuffers.resize(m_swapchainImages.size());
		//m_modelDynamicUniformBufferMemories.resize(m_swapchainImages.size());

		for (size_t i = 0; i < m_vpUniformBuffers.size(); ++i)
		{
			create_buffer(m_device.physicalDevice, m_device.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
				&m_vpUniformBuffers[i], &m_vpUniformBufferMemories[i]);

			//create_buffer(m_device.physicalDevice, m_device.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			//	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			//	&m_modelDynamicUniformBuffers[i], &m_modelDynamicUniformBufferMemories[i]);
		}
	}

	void VulkanRenderer::create_descriptor_pool()
	{
		// Type of descriptors + how many descriptors
		VkDescriptorPoolSize vpDescriptorPoolSize{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = static_cast<uint32_t>(m_vpUniformBuffers.size()),
		};

		/*VkDescriptorPoolSize modelDescriptorPoolsize{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = static_cast<uint32_t>(m_modelDynamicUniformBuffers.size())
		};*/

		std::vector<VkDescriptorPoolSize> descriptorPoolSizes{ 
			vpDescriptorPoolSize, 
			//modelDescriptorPoolsize 
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = static_cast<uint32_t>(m_swapchainImages.size()),			// Maximum number of descriptor sets that can be created from pool
			.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()),
			.pPoolSizes = descriptorPoolSizes.data(),
		};

		VkResult result{ vkCreateDescriptorPool(m_device.logicalDevice, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a descriptor pool!");
		}
	}

	void VulkanRenderer::create_descriptor_sets()
	{	
		// One descriptor set for each swapchain image
		m_descriptorSets.resize(m_swapchainImages.size());

		// For now, all sets use the same layout
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts(m_swapchainImages.size(), m_descriptorSetLayout);

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = m_descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(m_descriptorSets.size()),
			.pSetLayouts = descriptorSetLayouts.data(),			// Layouts to use to allocate data (1:1 relationship)
		};

		VkResult result{ vkAllocateDescriptorSets(m_device.logicalDevice, &descriptorSetAllocateInfo, m_descriptorSets.data()) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate descriptor sets!");
		}

		// Update all the descriptor set bindings
		for (size_t i = 0; i < m_descriptorSets.size(); ++i)
		{
			VkDescriptorBufferInfo vpBufferInfo{
				.buffer = m_vpUniformBuffers[i],
				.offset = 0,
				.range = sizeof(UboViewProjection),			// Size of data
			};

			// Data about the connection between the binding and the buffer
			VkWriteDescriptorSet vpWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = m_descriptorSets[i],
				.dstBinding = 0,		// Matches with binding on layout/shader
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &vpBufferInfo,
			};

			// Model descriptor
			/*VkDescriptorBufferInfo modelBufferInfo{
				.buffer = m_modelDynamicUniformBuffers[i],
				.offset = 0,
				.range = m_modelUniformAlignment,
			};

			VkWriteDescriptorSet modelWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = m_descriptorSets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.pBufferInfo = &modelBufferInfo,
			};*/

			std::vector<VkWriteDescriptorSet> writeDescriptorSets{ 
				vpWriteDescriptorSet, 
				//modelWriteDescriptorSet 
			};

			// Update the descriptor sets with new buffer/binding info
			vkUpdateDescriptorSets(m_device.logicalDevice,
				static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		}
	}

	void VulkanRenderer::update_uniform_buffers(uint32_t imageIndex)
	{
		// Copy ViewProjection data
		void* data;
		vkMapMemory(m_device.logicalDevice, m_vpUniformBufferMemories[imageIndex], 0, 
			sizeof(UboViewProjection), 0, &data);
		memcpy(data, &m_uboViewProjection, sizeof(UboViewProjection));
		vkUnmapMemory(m_device.logicalDevice, m_vpUniformBufferMemories[imageIndex]);

		// Copy Model data (For dynamic uniform buffers)
		/*for (size_t i = 0; i < m_meshes.size(); ++i)
		{
			Model* currentModel{ reinterpret_cast<Model*>
				(reinterpret_cast<uint64_t>(m_modelTransferSpace) + (i * m_modelUniformAlignment)) };
			*currentModel = m_meshes[i].get_model_matrix();
		}

		vkMapMemory(m_device.logicalDevice, m_modelDynamicUniformBufferMemories[imageIndex], 0, 
			m_modelUniformAlignment * m_meshes.size(), 0, &data);
		memcpy(data, m_modelTransferSpace, m_modelUniformAlignment * m_meshes.size());
		vkUnmapMemory(m_device.logicalDevice, m_modelDynamicUniformBufferMemories[imageIndex]);*/
	}

	void VulkanRenderer::record_commands(uint32_t imageIndex)
	{
		// Information about how to begin each command buffer
		VkCommandBufferBeginInfo commandBufferBeginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.6f, 0.65f, 0.4f, 1.f };	// 1st attachment (color)
		clearValues[1].depthStencil.depth = 1.f;			// 2nd attachment (depth)

		// Information about how to begin a render pass (only for grapics applications)
		VkRenderPassBeginInfo renderPassBeginInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = m_renderPass,
			.renderArea{
				.offset = { 0, 0 },
				.extent = m_swapchainExtent,
			},
			.clearValueCount = static_cast<uint32_t>(clearValues.size()),
			.pClearValues = clearValues.data(),
		};

		VkResult result{ vkBeginCommandBuffer(m_commandBuffers[imageIndex], &commandBufferBeginInfo) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to start recording a command buffer!");
		}

		{ // Indented block to symbolize render pass
			renderPassBeginInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
			vkCmdBeginRenderPass(m_commandBuffers[imageIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // INLINE: All commands are primary
			{
				vkCmdBindPipeline(m_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				for (size_t j = 0; j < m_meshes.size(); ++j)
				{
					VkBuffer vertexBuffers[]{ m_meshes[j].get_vertex_buffer()};	// Buffers to bind
					VkDeviceSize offsets[]{ 0 };
					vkCmdBindVertexBuffers(m_commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(m_commandBuffers[imageIndex], m_meshes[j].get_index_buffer(), 0, VK_INDEX_TYPE_UINT32);
						
					// Offset for the j-th dynamic uniform buffer
					//uint32_t dynamicUniformOffset{ static_cast<uint32_t>(m_modelUniformAlignment * j) };

					vkCmdPushConstants(m_commandBuffers[imageIndex], m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
						0, sizeof(Model), &m_meshes[j].get_model_matrix());

					vkCmdBindDescriptorSets(m_commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
						m_pipelineLayout, 0, 1, &m_descriptorSets[imageIndex], 0, nullptr);

					vkCmdDrawIndexed(m_commandBuffers[imageIndex], m_meshes[j].get_index_count(), 1, 0, 0, 0);
				}
			}
			vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
		}

		result = vkEndCommandBuffer(m_commandBuffers[imageIndex]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to stop recording a command buffer!");
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

		// Get properties of physical device to find uniform buffer alignment
		//VkPhysicalDeviceProperties physicalDeviceProperties;
		//vkGetPhysicalDeviceProperties(m_device.physicalDevice, &physicalDeviceProperties);

		//m_minUniformBufferOffset = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
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

	void VulkanRenderer::allocate_dynamic_buffer_transfer_space()
	{
		//// Minimum block alignment based on the size of the uniform values (in this case, UboModel)
		//m_modelUniformAlignment = (sizeof(Model) + m_minUniformBufferOffset - 1) & ~(m_minUniformBufferOffset - 1);

		//// Create space in memory to hold alined dynamic buffer that can contain up to MAX_OBJECTS
		//m_modelTransferSpace = static_cast<Model*>(_aligned_malloc(m_modelUniformAlignment * MAX_OBJECTS, m_modelUniformAlignment));
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

	VkFormat VulkanRenderer::choose_supported_format(const std::vector<VkFormat>& formatList, VkImageTiling imageTiling, VkFormatFeatureFlags featureFlags) const
	{
		for (VkFormat format : formatList)
		{
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(m_device.physicalDevice, format, &formatProperties);

			// Depending on tiling choice, need to check for different feature flags
			if ((imageTiling == VK_IMAGE_TILING_LINEAR && ((formatProperties.linearTilingFeatures & featureFlags) == featureFlags))
				|| (imageTiling == VK_IMAGE_TILING_OPTIMAL && ((formatProperties.optimalTilingFeatures & featureFlags) == featureFlags)))
			{
				return format;
			}
		}

		throw std::runtime_error("Failed to find a matching format!");
	}

	VkImage VulkanRenderer::create_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, 
		VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceMemory* imageMemory)
	{
		VkImageCreateInfo imageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = format,
			.extent{
				.width = width,
				.height = height,
				.depth = 1,
			},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,				// N� samples for multisampling
			.tiling = tiling,
			.usage = usageFlags,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,		// Whether image can be shared between queues
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,		// Layout of image data on creation
		};

		VkImage image;
		VkResult result{ vkCreateImage(m_device.logicalDevice, &imageCreateInfo, nullptr, &image) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image!");
		}

		// Create device memory for created image
		// Get memory requirements for the created image
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(m_device.logicalDevice, image, &memoryRequirements);

		VkMemoryAllocateInfo memoryAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = find_memory_type_index(m_device.physicalDevice, memoryRequirements.memoryTypeBits, memoryPropertyFlags),
		};
		result = vkAllocateMemory(m_device.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate image memory!");
		}

		// Connect memory to image
		vkBindImageMemory(m_device.logicalDevice, image, *imageMemory, 0);

		return image;
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

	VkShaderModule VulkanRenderer::create_shader_module(const std::vector<char>& code)
	{
		if ((sizeof(char) * code.size()) % sizeof(uint32_t) != 0)
		{
			throw std::runtime_error("Shader code is not able to be pointed at by uint32_t*.");
		}

		VkShaderModuleCreateInfo shaderModuleCreateInfo{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())
		};

		VkShaderModule shaderModule;
		VkResult result{ vkCreateShaderModule(m_device.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule) };
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module!");
		}
		return shaderModule;
	}

	size_t VulkanRenderer::create_texture(const std::string& fileName)
	{
		// Load image file
		int width, height;
		VkDeviceSize imageSize;
		stbi_uc* imageData{ load_texture_file(fileName, &width, &height, &imageSize) };

		// We don't need host visible texture data, so we create staging buffer first
		VkBuffer imageStagingBuffer;
		VkDeviceMemory imageStagingBufferMemory;
		create_buffer(m_device.physicalDevice, m_device.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	&imageStagingBuffer, &imageStagingBufferMemory);

		void* data;
		vkMapMemory(m_device.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, imageData, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_device.logicalDevice, imageStagingBufferMemory);

		// Free original image data now not in use
		stbi_image_free(imageData);

		VkDeviceMemory textureImageMemory;
		VkImage textureImage{ create_image(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			&textureImageMemory) };

		// Force transition before transfer
		transition_image_layout(m_device.logicalDevice, m_graphicsQueue, m_graphicsCommandPool, 
			textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		copy_image_buffer(m_device.logicalDevice, m_graphicsQueue, m_graphicsCommandPool, 
			imageStagingBuffer, textureImage, width, height);

		transition_image_layout(m_device.logicalDevice, m_graphicsQueue, m_graphicsCommandPool,
			textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(m_device.logicalDevice, imageStagingBuffer, nullptr);
		vkFreeMemory(m_device.logicalDevice, imageStagingBufferMemory, nullptr);

		m_textureImages.push_back(textureImage);
		m_textureImageMemories.push_back(textureImageMemory);

		return (m_textureImages.size() - 1);
	}

	stbi_uc* VulkanRenderer::load_texture_file(const std::string& fileName, int* width, int* height, VkDeviceSize* imageSize)
	{
		// Number of channels image uses
		int channels;
		const auto desiredChannels{ STBI_rgb_alpha };

		const std::string fileLoc{ "Textures/" + fileName };
		stbi_uc* image{ stbi_load(fileLoc.c_str(), width, height, &channels, desiredChannels) };

		if (!image)
		{
			throw std::runtime_error("Failed to load texture file " + fileName + "!");
		}

		*imageSize = static_cast<VkDeviceSize>(*width * *height * desiredChannels);

		return image;
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