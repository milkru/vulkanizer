#include "common.h"
#include "swapchain.h"
#include "query.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <meshoptimizer.h>
#include <easy/profiler.h>

#include <string.h>
#include <stdio.h>
#include <chrono>

#ifdef DEBUG_
const bool kbEnableValidationLayers = true;
#else
const bool kbEnableValidationLayers = false;
#endif

const uint32_t kWindowWidth = 1280;
const uint32_t kWindowHeight = 720;

const uint32_t kMaxFramesInFlightCount = 2;

char windowTitle[256] = "proto_vk";

struct FramePacing
{
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

struct Buffer
{
	VkBuffer bufferVk = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	size_t size = 0;
	void* data = nullptr;
};

struct Vertex
{
	uint16_t position[3];
	// TODO-MILKRU: Consider passing only two components of a normal vector and
	// reconstruct the third one using the cross product in order to further save memory.
	uint8_t normal[3];
	uint16_t texCoord[2];
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

static GLFWwindow* createWindow()
{
	if (!glfwInit())
	{
		assert(!"GLFW not initialized properly!");
	}

	if (!glfwVulkanSupported())
	{
		assert(!"Vulkan is not supported!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* pWindow = glfwCreateWindow(kWindowWidth, kWindowHeight, windowTitle, nullptr, nullptr);
	if (pWindow == nullptr)
	{
		glfwTerminate();
		assert(!"Window creation failed!");
	}

	return pWindow;
}

static VkVertexInputBindingDescription getVertexBindingDescription()
{
	VkVertexInputBindingDescription vertexBindingDescription{};
	vertexBindingDescription.binding = 0;
	vertexBindingDescription.stride = sizeof(Vertex);
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return vertexBindingDescription;
}

static std::array<VkVertexInputAttributeDescription, 3> getVertexAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions{};

	vertexAttributeDescriptions[0].binding = 0;
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].format = VK_FORMAT_R16G16B16_SFLOAT;
	vertexAttributeDescriptions[0].offset = offsetof(Vertex, position);

	vertexAttributeDescriptions[1].binding = 0;
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].format = VK_FORMAT_R8G8B8_SNORM;
	vertexAttributeDescriptions[1].offset = offsetof(Vertex, normal);

	vertexAttributeDescriptions[2].binding = 0;
	vertexAttributeDescriptions[2].location = 2;
	vertexAttributeDescriptions[2].format = VK_FORMAT_R16G16_SFLOAT;
	vertexAttributeDescriptions[2].offset = offsetof(Vertex, texCoord);

	return vertexAttributeDescriptions;
}

static VkInstance createInstance()
{
	const char* validationLayers[] =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	const char* instanceExtensions[] =
	{
		"VK_KHR_surface",
#ifdef _WIN32
		"VK_KHR_win32_surface",
#elif __linux__
		"VK_KHR_xcb_surface",
#endif
#ifdef DEBUG_
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
	};

	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "proto_vk";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "proto_vk";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.pApplicationInfo = &applicationInfo;

	if (kbEnableValidationLayers)
	{
		instanceCreateInfo.enabledLayerCount = ARRAY_SIZE(validationLayers);
		instanceCreateInfo.ppEnabledLayerNames = validationLayers;
	}

	instanceCreateInfo.enabledExtensionCount = ARRAY_SIZE(instanceExtensions);
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;

	VkInstance instance;
	VK_CALL(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

	volkLoadInstance(instance);

	return instance;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT _messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT _messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
	void* _pUserData)
{
	printf("%s\n", _pCallbackData->pMessage);
	assert(_messageSeverity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);

	return VK_FALSE;
}

static VkDebugUtilsMessengerEXT createDebugMessenger(
	VkInstance _instance)
{
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	debugMessengerCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMessengerCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMessengerCreateInfo.pfnUserCallback = debugMessageCallback;

	VkDebugUtilsMessengerEXT debugMessenger;
	VK_CALL(vkCreateDebugUtilsMessengerEXT(_instance, &debugMessengerCreateInfo, nullptr, &debugMessenger));

	return debugMessenger;
}

static VkSurfaceKHR createSurface(
	VkInstance _instance,
	GLFWwindow* _pWindow)
{
	VkSurfaceKHR surface;
	VK_CALL(glfwCreateWindowSurface(_instance, _pWindow, nullptr, &surface));

	return surface;
}

static uint32_t tryGetGraphicsQueueFamilyIndex(
	VkPhysicalDevice _physicalDevice)
{
	uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueCount, 0);

	std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueCount, queueProperties.data());

	for (uint32_t queueIndex = 0; queueIndex < queueCount; ++queueIndex)
	{
		if (queueProperties[queueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			return queueIndex;
		}
	}

	return ~0u;
}

static VkPhysicalDevice tryPickPhysicalDevice(
	VkInstance _instance,
	VkSurfaceKHR _surface)
{
	uint32_t physicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);

	if (physicalDeviceCount == 0)
	{
		assert(!"Not a single physical device was found!");
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices.data());

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	for (VkPhysicalDevice potentialPhysicalDevice : physicalDevices)
	{
		uint32_t graphicsQueueIndex = tryGetGraphicsQueueFamilyIndex(potentialPhysicalDevice);
		if (graphicsQueueIndex == ~0u)
		{
			continue;
		}

		VkBool32 bSurfaceSupported;
		vkGetPhysicalDeviceSurfaceSupportKHR(potentialPhysicalDevice, graphicsQueueIndex, _surface, &bSurfaceSupported);
		if (bSurfaceSupported == VK_FALSE)
		{
			continue;
		}

		physicalDevice = potentialPhysicalDevice;
		break;
	}

	return physicalDevice;
}

static VkDevice createDevice(
	VkPhysicalDevice _physicalDevice,
	uint32_t _queueFamilyIndex)
{
	const char* deviceExtensions[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueCreateInfo.queueFamilyIndex = _queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.pipelineStatisticsQuery = VK_TRUE;

	// TODO-MILKRU: Remember that SPV_KHR_8bit_storage is in VK_VERSION_1_2 core.
	VkPhysicalDeviceVulkan12Features physicalDeviceFeatures12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.pNext = &physicalDeviceFeatures12;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = ARRAY_SIZE(deviceExtensions);
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

	VkDevice device;
	VK_CALL(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &device));

	return device;
}

static VkRenderPass createRenderPass(
	VkDevice _device,
	VkFormat _colorFormat)
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = _colorFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkRenderPass renderPass;
	VK_CALL(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &renderPass));

	return renderPass;
}

static VkPipelineLayout createPipelineLayout(
	VkDevice _device)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;

	VkPipelineLayout pipelineLayout;
	VK_CALL(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	return pipelineLayout;
}

static VkPipeline createGraphicsPipeline(
	VkDevice _device,
	VkRenderPass _renderPass,
	VkPipelineLayout _pipelineLayout,
	VkShaderModule _vertexShader,
	VkShaderModule _fragmentShader)
{
	VkPipelineShaderStageCreateInfo vertexShaderStageICreatenfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertexShaderStageICreatenfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageICreatenfo.module = _vertexShader;
	vertexShaderStageICreatenfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = _fragmentShader;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = { vertexShaderStageICreatenfo, fragmentShaderStageCreateInfo };

	VkVertexInputBindingDescription bindingDescription = getVertexBindingDescription();
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = getVertexAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = uint32_t(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.0f;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
	colorBlendCreateInfo.blendConstants[0] = 0.0f;
	colorBlendCreateInfo.blendConstants[1] = 0.0f;
	colorBlendCreateInfo.blendConstants[2] = 0.0f;
	colorBlendCreateInfo.blendConstants[3] = 0.0f;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStates);
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = _pipelineLayout;
	pipelineCreateInfo.renderPass = _renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline graphicsPipeline;
	VK_CALL(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline));

	return graphicsPipeline;
}

static VkFramebuffer createFramebuffer(
	VkDevice _device,
	VkRenderPass _renderPass,
	VkExtent2D _extent,
	const std::vector<VkImageView>& _rAttachments)
{
	VkFramebufferCreateInfo framebufferICreatenfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferICreatenfo.renderPass = _renderPass;
	framebufferICreatenfo.attachmentCount = _rAttachments.size();
	framebufferICreatenfo.pAttachments = _rAttachments.data();
	framebufferICreatenfo.width = _extent.width;
	framebufferICreatenfo.height = _extent.height;
	framebufferICreatenfo.layers = 1;

	VkFramebuffer framebuffer;
	VK_CALL(vkCreateFramebuffer(_device, &framebufferICreatenfo, nullptr, &framebuffer));

	return framebuffer;
}

static VkCommandPool createCommandPool(
	VkDevice _device,
	uint32_t _queueFamilyIndex)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	commandPoolCreateInfo.queueFamilyIndex = _queueFamilyIndex;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	VK_CALL(vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &commandPool));

	return commandPool;
}

static uint32_t tryFindMemoryType(
	VkPhysicalDevice _physicalDevice,
	uint32_t _typeFilter,
	VkMemoryPropertyFlags _memoryFlags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memoryProperties);

	for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryProperties.memoryTypeCount; ++memoryTypeIndex)
	{
		if ((_typeFilter & (1 << memoryTypeIndex)) && (memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & _memoryFlags) == _memoryFlags)
		{
			return memoryTypeIndex;
		}
	}

	return -1;
}

static Buffer createBuffer(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkBufferUsageFlags _usageFlags,
	VkMemoryPropertyFlags _memoryFlags,
	VkDeviceSize _size)
{
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.size = _size;
	bufferCreateInfo.usage = _usageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Buffer buffer{};
	VK_CALL(vkCreateBuffer(_device, &bufferCreateInfo, nullptr, &buffer.bufferVk));

	buffer.size = _size;

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(_device, buffer.bufferVk, &memoryRequirements);

	uint32_t memoryTypeIndex = tryFindMemoryType(_physicalDevice, memoryRequirements.memoryTypeBits, _memoryFlags);
	assert(memoryTypeIndex != -1);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CALL(vkAllocateMemory(_device, &allocateInfo, nullptr, &buffer.memory));

	vkBindBufferMemory(_device, buffer.bufferVk, buffer.memory, 0);

	if (_memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		// Persistently mapped memory, which should be faster on NVidia.
		vkMapMemory(_device, buffer.memory, 0, _size, 0, &buffer.data);
	}

	return buffer;
}

static VkCommandBuffer createCommandBuffer(
	VkDevice _device,
	VkCommandPool _commandPool)
{
	VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandPool = _commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CALL(vkAllocateCommandBuffers(_device, &allocateInfo, &commandBuffer));

	return commandBuffer;
}

static void copyBuffer(
	VkDevice _device,
	VkQueue _queue,
	VkCommandPool _commandPool,
	VkBuffer _srcBuffer,
	VkBuffer _dstBuffer,
	VkDeviceSize _size)
{
	VkCommandBuffer commandBuffer = createCommandBuffer(_device, _commandPool);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	VkBufferCopy copyRegion{};
	copyRegion.size = _size;
	vkCmdCopyBuffer(commandBuffer, _srcBuffer, _dstBuffer, 1, &copyRegion);

	VK_CALL(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CALL(vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL(vkQueueWaitIdle(_queue));

	vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
}

static void destroyBuffer(
	VkDevice _device,
	Buffer& _rBuffer)
{
	if (_rBuffer.data)
	{
		vkUnmapMemory(_device, _rBuffer.memory);
	}

	vkDestroyBuffer(_device, _rBuffer.bufferVk, nullptr);
	vkFreeMemory(_device, _rBuffer.memory, nullptr);

	_rBuffer = {};
}

// NOTE-MILKRU: Use one big buffer for sub allocation.
static Buffer createBuffer(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkQueue _queue,
	VkCommandPool _commandPool,
	VkBufferUsageFlags _usageFlags,
	VkDeviceSize _size,
	void* _pContents = nullptr)
{
	if (_pContents)
	{
		_usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	Buffer buffer = createBuffer(_physicalDevice, _device, _usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _size);

	if (_pContents)
	{
		Buffer stagingBuffer = createBuffer(_physicalDevice, _device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _size);

		memcpy(stagingBuffer.data, _pContents, stagingBuffer.size);
		copyBuffer(_device, _queue, _commandPool, stagingBuffer.bufferVk, buffer.bufferVk, _size);
		destroyBuffer(_device, stagingBuffer);
	}

	return buffer;
}

static VkSemaphore createSemaphore(
	VkDevice _device)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore;
	VK_CALL(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &semaphore));

	return semaphore;
}

static VkFence createFence(
	VkDevice _device)
{
	VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	VK_CALL(vkCreateFence(_device, &fenceCreateInfo, nullptr, &fence));

	return fence;
}

static FramePacing createFramePacing(
	VkDevice _device)
{
	FramePacing framePacing{};
	framePacing.imageAvailableSemaphore = createSemaphore(_device);
	framePacing.renderFinishedSemaphore = createSemaphore(_device);
	framePacing.inFlightFence = createFence(_device);
	return framePacing;
}

static void destroyFramePacing(
	VkDevice _device,
	FramePacing _framePacing)
{
	vkDestroySemaphore(_device, _framePacing.renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(_device, _framePacing.imageAvailableSemaphore, nullptr);
	vkDestroyFence(_device, _framePacing.inFlightFence, nullptr);
}

static VkShaderModule createShaderModule(
	VkDevice _device,
	size_t _spvCodeSize,
	uint32_t* _pSpvCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = _spvCodeSize;
	shaderModuleCreateInfo.pCode = _pSpvCode;

	VkShaderModule shaderModule;
	VK_CALL(vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule));

	return shaderModule;
}

static std::vector<uint8_t> readFile(
	const char* _pFilePath)
{
	FILE* file = fopen(_pFilePath, "rb");
	assert(file != nullptr);

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	std::vector<uint8_t> fileContents(fileSize);
	size_t bytesToRead = fread(fileContents.data(), sizeof(uint8_t), fileSize, file);
	fclose(file);

	return fileContents;
}

Mesh loadMesh(
	const char* _pFilePath)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warning, error;

	{
		bool bObjLoaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, _pFilePath);
		assert(bObjLoaded);
	}

	// TODO-MILKRU: Meshes with multiple sections (shapes) are not supported yet.
	assert(shapes.size() == 1);

	tinyobj::mesh_t& shapeMesh = shapes[0].mesh;
	size_t indexCount = shapeMesh.indices.size();

	Mesh mesh;
	mesh.vertices.reserve(indexCount);

	// Using various mesh optimizations from meshoptimizer.
	// https://github.com/zeux/meshoptimizer
	for (const auto& index : shapeMesh.indices)
	{
		Vertex vertex{};

		vertex.position[0] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 0]);
		vertex.position[1] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 1]);
		vertex.position[2] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 2]);

		vertex.normal[0] = meshopt_quantizeSnorm(index.normal_index < 0 ? 0.0f :
			attrib.normals[3 * index.normal_index + 0], 8);
		vertex.normal[1] = meshopt_quantizeSnorm(index.normal_index < 0 ? 1.0f :
			attrib.normals[3 * index.normal_index + 1], 8);
		vertex.normal[2] = meshopt_quantizeSnorm(index.normal_index < 0 ? 0.0f :
			attrib.normals[3 * index.normal_index + 2], 8);

		vertex.texCoord[0] = meshopt_quantizeHalf(index.texcoord_index < 0 ? 0.0f :
			attrib.texcoords[2 * index.texcoord_index + 0]);
		vertex.texCoord[1] = meshopt_quantizeHalf(index.texcoord_index < 0 ? 0.0f :
			1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);

		mesh.vertices.push_back(vertex);
	}

	std::vector<uint32_t> remapTable(indexCount);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, indexCount,
		mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));

	mesh.vertices.resize(vertexCount);
	mesh.indices.resize(indexCount);

	meshopt_remapVertexBuffer(mesh.vertices.data(), mesh.vertices.data(), indexCount, sizeof(Vertex), remapTable.data());
	meshopt_remapIndexBuffer(mesh.indices.data(), nullptr, indexCount, remapTable.data());

	meshopt_optimizeVertexCache(mesh.indices.data(), mesh.indices.data(), indexCount, vertexCount);
	// TODO-MILKRU: meshopt_optimizeOverdraw does not support 16 bit vertex positions.
	meshopt_optimizeVertexFetch(mesh.vertices.data(), mesh.indices.data(), indexCount, mesh.vertices.data(), vertexCount, sizeof(Vertex));

	return mesh;
}

int main(int argc, const char** argv)
{
	EASY_MAIN_THREAD;
	EASY_PROFILER_ENABLE;

	if (argc != 2)
	{
		printf("Mesh path is required as a command line argument.\n");
		return 1;
	}

	GLFWwindow* pWindow = createWindow();

	VK_CALL(volkInitialize());

	VkInstance instance = createInstance();

	VkDebugUtilsMessengerEXT debugMessenger = kbEnableValidationLayers ? createDebugMessenger(instance) : VK_NULL_HANDLE;

	VkSurfaceKHR surface = createSurface(instance, pWindow);

	VkPhysicalDevice physicalDevice = tryPickPhysicalDevice(instance, surface);
	assert(physicalDevice != VK_NULL_HANDLE);

	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	printf("%s device will be used.\n", physicalDeviceProperties.deviceName);

	uint32_t graphicsQueueIndex = tryGetGraphicsQueueFamilyIndex(physicalDevice);
	assert(graphicsQueueIndex != ~0u);

	VkDevice device = createDevice(physicalDevice, graphicsQueueIndex);

	VkQueue graphicsQueue;
	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &graphicsQueue);

	Swapchain swapchain = createSwapchain(pWindow, surface, physicalDevice, device);

	VkRenderPass renderPass = createRenderPass(device, swapchain.imageFormat);

	VkPipelineLayout pipelineLayout = createPipelineLayout(device);

	VkPipeline graphicsPipeline;
	{
		std::vector<uint8_t> vertexShaderCode = readFile("shaders/shader.vert.spv");
		VkShaderModule vertexShader = createShaderModule(device, vertexShaderCode.size(), (uint32_t*)vertexShaderCode.data());

		std::vector<uint8_t> fragmentShaderCode = readFile("shaders/shader.frag.spv");
		VkShaderModule fragmentShader = createShaderModule(device, fragmentShaderCode.size(), (uint32_t*)fragmentShaderCode.data());

		graphicsPipeline = createGraphicsPipeline(device, renderPass, pipelineLayout, vertexShader, fragmentShader);

		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
	}

	std::vector<VkFramebuffer> framebuffers(swapchain.imageViews.size());
	for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
	{
		framebuffers[frameBufferIndex] = createFramebuffer(device, renderPass, swapchain.extent, { swapchain.imageViews[frameBufferIndex] });
	}

	VkCommandPool commandPool = createCommandPool(device, graphicsQueueIndex);

	const char* meshPath = argv[1];
	Mesh mesh = loadMesh(meshPath);

	Buffer vertexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(Vertex) * mesh.vertices.size(), mesh.vertices.data());

	Buffer indexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data());

	std::vector<VkCommandBuffer> commandBuffers(kMaxFramesInFlightCount);
	std::vector<FramePacing> framePacings(kMaxFramesInFlightCount);

	for (uint32_t frameIndex = 0; frameIndex < kMaxFramesInFlightCount; ++frameIndex)
	{
		commandBuffers[frameIndex] = createCommandBuffer(device, commandPool);
		framePacings[frameIndex] = createFramePacing(device);
	}

	QueryPool timestampsQueryPool = createQueryPool(device, VK_QUERY_TYPE_TIMESTAMP, /*_queryCount*/ 2);
	QueryPool statisticsQueryPool = createQueryPool(device, VK_QUERY_TYPE_PIPELINE_STATISTICS, /*_queryCount*/ 1);

	uint32_t currentFrame = 0;

	while (!glfwWindowShouldClose(pWindow))
	{
		EASY_BLOCK("Frame");

		auto beginFrameTimestamp = std::chrono::high_resolution_clock::now();

		glfwPollEvents();

		VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
		FramePacing framePacing = framePacings[currentFrame];

		{
			EASY_BLOCK("WaitForFences");
			VK_CALL(vkWaitForFences(device, 1, &framePacing.inFlightFence, VK_TRUE, UINT64_MAX));
		}

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

		VkExtent2D currentExtent = surfaceCapabilities.currentExtent;

		if (currentExtent.width == 0 || currentExtent.height == 0)
		{
			continue;
		}

		if (swapchain.extent.width != currentExtent.width || swapchain.extent.height != currentExtent.height)
		{
			EASY_BLOCK("RecreateSwapchain");

			VK_CALL(vkDeviceWaitIdle(device));

			for (VkFramebuffer framebuffer : framebuffers)
			{
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			}

			Swapchain newSwapchain = createSwapchain(pWindow, surface, physicalDevice, device, swapchain.swapchainVk);
			destroySwapchain(device, swapchain);
			swapchain = newSwapchain;

			framebuffers.resize(swapchain.imageViews.size());
			for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
			{
				framebuffers[frameBufferIndex] = createFramebuffer(device, renderPass, swapchain.extent, { swapchain.imageViews[frameBufferIndex] });
			}

			continue;
		}

		VK_CALL(vkResetFences(device, 1, &framePacing.inFlightFence));

		uint32_t imageIndex;
		VK_CALL(vkAcquireNextImageKHR(device, swapchain.swapchainVk, UINT64_MAX, framePacing.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));

		{
			EASY_BLOCK("Draw");

			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			resetQueryPool(commandBuffer, timestampsQueryPool);
			resetQueryPool(commandBuffer, statisticsQueryPool);

			VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = swapchain.extent;

			VkClearValue clearColor = { {{ 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f }} };
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearColor;

			{
				GPU_BLOCK(commandBuffer, timestampsQueryPool, Main);
				GPU_STATS(commandBuffer, statisticsQueryPool, Main);

				vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Flipped viewport.
				// https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
				VkViewport viewport{};
				viewport.x = 0.0f;
				viewport.y = float(swapchain.extent.height);
				viewport.width = float(swapchain.extent.width);
				viewport.height = -float(swapchain.extent.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissorRect{};
				scissorRect.offset = { 0, 0 };
				scissorRect.extent = swapchain.extent;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				VkBuffer vertexBuffers[] = { vertexBuffer.bufferVk };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindIndexBuffer(commandBuffer, indexBuffer.bufferVk, 0, VK_INDEX_TYPE_UINT32);

				for (uint32_t drawIndex = 0; drawIndex < 4086; ++drawIndex)
				{
					vkCmdDrawIndexed(commandBuffer, uint32_t(mesh.indices.size()), 1, 0, 0, 0);
				}

				vkCmdEndRenderPass(commandBuffer);
			}

			VK_CALL(vkEndCommandBuffer(commandBuffer));
		}

		VkSemaphore waitSemaphores[] = { framePacing.imageAvailableSemaphore };
		VkSemaphore signalSemaphores[] = { framePacing.renderFinishedSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = ARRAY_SIZE(waitSemaphores);
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = ARRAY_SIZE(signalSemaphores);
		submitInfo.pSignalSemaphores = signalSemaphores;

		VK_CALL(vkQueueSubmit(graphicsQueue, 1, &submitInfo, framePacing.inFlightFence));

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain.swapchainVk;
		presentInfo.pImageIndices = &imageIndex;

		VK_CALL(vkQueuePresentKHR(graphicsQueue, &presentInfo));

		updateQueryPoolResults(device, timestampsQueryPool);
		updateQueryPoolResults(device, statisticsQueryPool);

		{
			static double mainGpuTime = 0.0f;
			GPU_BLOCK_RESULT(timestampsQueryPool, physicalDeviceProperties.limits, Main, mainGpuTime);

			static uint64_t mainGpuPrimitives = 0ull;
			GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingPrimitives, mainGpuPrimitives);

			auto endFrameTimestamp = std::chrono::high_resolution_clock::now();
			double mainCpuTime = std::chrono::duration<double, std::chrono::milliseconds::period>(endFrameTimestamp - beginFrameTimestamp).count();

			// TODO-MILKRU: Temporary until Imgui gets integrated.
			sprintf(windowTitle, "proto_vk [GPU time: %.2f ms, CPU time: %.2f ms, clipping primitives: %lld]", mainGpuTime, mainCpuTime, mainGpuPrimitives);
			glfwSetWindowTitle(pWindow, windowTitle);
		}

		currentFrame = (currentFrame + 1) % kMaxFramesInFlightCount;
	}

	{
		EASY_BLOCK("DeviceWaitIdle");
		VK_CALL(vkDeviceWaitIdle(device));
	}

	{
		EASY_BLOCK("Cleanup");

		for (VkFramebuffer framebuffer : framebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		destroySwapchain(device, swapchain);

		destroyBuffer(device, vertexBuffer);
		destroyBuffer(device, indexBuffer);

		for (size_t frameIndex = 0; frameIndex < kMaxFramesInFlightCount; ++frameIndex)
		{
			destroyFramePacing(device, framePacings[frameIndex]);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		destroyQueryPool(device, timestampsQueryPool);
		destroyQueryPool(device, statisticsQueryPool);

		vkDestroyDevice(device, nullptr);

		if (debugMessenger != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(pWindow);

		glfwTerminate();
	}

	{
		const char* profileCaptureFileName = "cpu_profile_capture.prof";
		profiler::dumpBlocksToFile(profileCaptureFileName);
		printf("CPU profile capture saved to %s file.\n", profileCaptureFileName);
	}

	return 0;
}
