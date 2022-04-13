#include <volk.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>
#include <array>
#include <string.h>
#include <stdio.h>

#define VK_CALL(_call) \
	do { \
		VkResult result_ = _call; \
		assert(result_ == VK_SUCCESS); \
	} \
	while (0)

#ifndef MIN
#define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef MAX
#define MAX(_a, _b) (((_b) < (_a)) ? (_a) : (_b))
#endif

#ifndef CLAMP
#define CLAMP(_val, _min, _max) ((_val) < (_min) ? (_min) : (_val) > (_max) ? (_max) : (_val))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) ((int)(sizeof(_arr) / sizeof(*(_arr))))
#endif

#ifdef DEBUG_
const bool kEnableValidationLayers = true;
#else
const bool kEnableValidationLayers = false;
#endif

const bool kEnableVSync = true;

const uint32_t kWindowWidth = 1280;
const uint32_t kWindowHeight = 720;

const uint32_t kPreferredSwapchainImageCount = 2;
const uint32_t kMaxFramesInFlightCount = 2;

struct Swapchain
{
	VkSwapchainKHR swapchain;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	VkFormat imageFormat;
	VkExtent2D extent;
};

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
};

struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;
};

struct FramePacing
{
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;
};

static VkVertexInputBindingDescription getVertexBindingDescription()
{
	VkVertexInputBindingDescription vertexBindingDescription{};
	vertexBindingDescription.binding = 0;
	vertexBindingDescription.stride = sizeof(Vertex);
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return vertexBindingDescription;
}

static std::array<VkVertexInputAttributeDescription, 2> getVertexAttributeDescriptions()
{
	std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions{};

	vertexAttributeDescriptions[0].binding = 0;
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributeDescriptions[0].offset = offsetof(Vertex, position);

	vertexAttributeDescriptions[1].binding = 0;
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescriptions[1].offset = offsetof(Vertex, color);

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
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.pApplicationInfo = &applicationInfo;

	if (kEnableValidationLayers)
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

		VkBool32 surfaceSupported;
		vkGetPhysicalDeviceSurfaceSupportKHR(potentialPhysicalDevice, graphicsQueueIndex, _surface, &surfaceSupported);
		if (surfaceSupported == VK_FALSE)
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

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	deviceCreateInfo.enabledExtensionCount = ARRAY_SIZE(deviceExtensions);
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

	VkDevice device;
	VK_CALL(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &device));

	return device;
}

static VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	uint32_t surfaceFormatCount = 0;
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, nullptr));

	assert(surfaceFormatCount > 0);

	std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, availableSurfaceFormats.data()));

	for (const auto& availableSurfaceFormat : availableSurfaceFormats)
	{
		if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			return availableSurfaceFormat;
		}
	}

	return availableSurfaceFormats[0];
}

static VkPresentModeKHR chooseSwapchainPresentMode(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	uint32_t presentModeCount = 0;
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr));

	std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, availablePresentModes.data()));

	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (kEnableVSync)
		{
			// TODO: Which one should be used for VSync, VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_MAILBOX_KHR?
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}
		else
		{
			if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				return availablePresentMode;
			}
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapchainExtent(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	GLFWwindow* _pWindow)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	if (surfaceCapabilities.currentExtent.width != UINT32_MAX && surfaceCapabilities.currentExtent.height != UINT32_MAX)
	{
		return surfaceCapabilities.currentExtent;
	}

	int windowWidth, windowHeight;
	glfwGetFramebufferSize(_pWindow, &windowWidth, &windowHeight);

	VkExtent2D actualExtent = { uint32_t(windowWidth), uint32_t(windowHeight) };

	VkExtent2D minImageExtent = surfaceCapabilities.minImageExtent;
	VkExtent2D maxImageExtent = surfaceCapabilities.maxImageExtent;

	actualExtent.width = CLAMP(actualExtent.width, minImageExtent.width, maxImageExtent.width);
	actualExtent.height = CLAMP(actualExtent.height, minImageExtent.height, maxImageExtent.height);

	return actualExtent;
}

static VkSwapchainKHR createSwapchain(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkSurfaceFormatKHR _surfaceFormat,
	VkPresentModeKHR _presentMode,
	VkExtent2D _extent,
	VkSwapchainKHR _oldSwapchain)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	uint32_t minImageCount = CLAMP(kPreferredSwapchainImageCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchainCreateInfo.surface = _surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = _surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = _surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = _extent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = _presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = _oldSwapchain;

	VkSwapchainKHR swapchain;
	VK_CALL(vkCreateSwapchainKHR(_device, &swapchainCreateInfo, nullptr, &swapchain));

	return swapchain;
}

static VkImageView createImageView(
	VkDevice _device,
	VkImage _image,
	VkFormat _format)
{
	VkImageAspectFlags aspectMask = (_format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = _image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = _format;
	imageViewCreateInfo.subresourceRange.aspectMask = aspectMask;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VK_CALL(vkCreateImageView(_device, &imageViewCreateInfo, 0, &imageView));

	return imageView;
}

static Swapchain createSwapchain(
	GLFWwindow* _pWindow,
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkSwapchainKHR _oldSwapchain = VK_NULL_HANDLE)
{
	VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(_surface, _physicalDevice);
	VkPresentModeKHR presentMode = chooseSwapchainPresentMode(_surface, _physicalDevice);
	VkExtent2D extent = chooseSwapchainExtent(_surface, _physicalDevice, _pWindow);

	Swapchain swapchain{};
	swapchain.imageFormat = surfaceFormat.format;
	swapchain.extent = extent;

	swapchain.swapchain = createSwapchain(
		_surface,
		_physicalDevice,
		_device,
		surfaceFormat,
		presentMode,
		extent,
		_oldSwapchain);

	uint32_t imageCount;
	vkGetSwapchainImagesKHR(_device, swapchain.swapchain, &imageCount, nullptr);
	swapchain.images.resize(imageCount);
	vkGetSwapchainImagesKHR(_device, swapchain.swapchain, &imageCount, swapchain.images.data());

	swapchain.imageViews.resize(swapchain.images.size());
	for (size_t imageIndex = 0; imageIndex < swapchain.imageViews.size(); ++imageIndex)
	{
		swapchain.imageViews[imageIndex] = createImageView(_device, swapchain.images[imageIndex], swapchain.imageFormat);
	}

	return swapchain;
}

static void destroySwapchain(
	VkDevice _device,
	Swapchain _swapchain)
{
	for (VkImageView imageView : _swapchain.imageViews)
	{
		vkDestroyImageView(_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(_device, _swapchain.swapchain, nullptr);
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
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = getVertexAttributeDescriptions();

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
	VkDeviceSize _size,
	VkBufferUsageFlags _usageFlags,
	VkMemoryPropertyFlags _memoryFlags)
{
	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.size = _size;
	bufferCreateInfo.usage = _usageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Buffer buffer{};
	VK_CALL(vkCreateBuffer(_device, &bufferCreateInfo, nullptr, &buffer.buffer));

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(_device, buffer.buffer, &memoryRequirements);

	uint32_t memoryTypeIndex = tryFindMemoryType(_physicalDevice, memoryRequirements.memoryTypeBits, _memoryFlags);
	assert(memoryTypeIndex != -1);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CALL(vkAllocateMemory(_device, &allocateInfo, nullptr, &buffer.memory));

	vkBindBufferMemory(_device, buffer.buffer, buffer.memory, 0);

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

static Buffer createBuffer(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkQueue _queue,
	VkCommandPool _commandPool,
	VkDeviceSize _size,
	void* _pContents,
	VkBufferUsageFlags _usageFlags)
{
	Buffer stagingBuffer = createBuffer(_physicalDevice, _device, _size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(_device, stagingBuffer.memory, 0, _size, 0, &data);
	memcpy(data, _pContents, (size_t)_size);
	vkUnmapMemory(_device, stagingBuffer.memory);

	Buffer buffer = createBuffer(_physicalDevice, _device, _size, _usageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	copyBuffer(_device, _queue, _commandPool, stagingBuffer.buffer, buffer.buffer, _size);

	vkDestroyBuffer(_device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(_device, stagingBuffer.memory, nullptr);

	return buffer;
}

static void destroyBuffer(
	VkDevice _device,
	Buffer _buffer)
{
	vkDestroyBuffer(_device, _buffer.buffer, nullptr);
	vkFreeMemory(_device, _buffer.memory, nullptr);
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
	uint32_t* _spvCode)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shaderModuleCreateInfo.codeSize = _spvCodeSize;
	shaderModuleCreateInfo.pCode = _spvCode;

	VkShaderModule shaderModule;
	VK_CALL(vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule));

	return shaderModule;
}

static std::vector<uint8_t> readFile(
	const char* _filePath)
{
	FILE* file = fopen(_filePath, "rb");
	assert(file != nullptr);

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	std::vector<uint8_t> fileContents(fileSize);
	size_t bytesToRead = fread(fileContents.data(), sizeof(uint8_t), fileSize, file);
	fclose(file);

	return fileContents;
}

int main(int argc, const char** argv)
{
	GLFWwindow* pWindow = nullptr;
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

		pWindow = glfwCreateWindow(kWindowWidth, kWindowHeight, "proto_vk", nullptr, nullptr);
	}

	if (pWindow == nullptr)
	{
		glfwTerminate();
		assert(!"Window creation failed!");
	}

	VK_CALL(volkInitialize());

	VkInstance instance = createInstance();

	VkDebugUtilsMessengerEXT debugMessenger = kEnableValidationLayers ? createDebugMessenger(instance) : VK_NULL_HANDLE;

	VkSurfaceKHR surface = createSurface(instance, pWindow);

	VkPhysicalDevice physicalDevice = tryPickPhysicalDevice(instance, surface);
	assert(physicalDevice != VK_NULL_HANDLE);

	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
		printf("%s will be used.\n", physicalDeviceProperties.deviceName);
	}

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

	const Vertex vertices[] =
	{
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
	};

	Buffer vertexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool, sizeof(vertices), (void*)vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	const uint16_t indices[] =
	{
		0, 1, 2, 2, 3, 0
	};

	Buffer indexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool, sizeof(indices), (void*)indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	std::vector<VkCommandBuffer> commandBuffers(kMaxFramesInFlightCount);
	std::vector<FramePacing> framePacings(kMaxFramesInFlightCount);

	for (uint32_t frameIndex = 0; frameIndex < kMaxFramesInFlightCount; ++frameIndex)
	{
		commandBuffers[frameIndex] = createCommandBuffer(device, commandPool);
		framePacings[frameIndex] = createFramePacing(device);
	}

	uint32_t currentFrame = 0;

	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();

		VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
		FramePacing framePacing = framePacings[currentFrame];

		VK_CALL(vkWaitForFences(device, 1, &framePacing.inFlightFence, VK_TRUE, UINT64_MAX));

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

		VkExtent2D currentExtent = surfaceCapabilities.currentExtent;

		if (currentExtent.width == 0 || currentExtent.height == 0)
		{
			continue;
		}

		if (swapchain.extent.width != currentExtent.width || swapchain.extent.height != currentExtent.height)
		{
			VK_CALL(vkDeviceWaitIdle(device));

			for (VkFramebuffer framebuffer : framebuffers)
			{
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			}

			Swapchain newSwapchain = createSwapchain(pWindow, surface, physicalDevice, device, swapchain.swapchain);
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
		VK_CALL(vkAcquireNextImageKHR(device, swapchain.swapchain, UINT64_MAX, framePacing.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));

		{
			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = framebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapchain.extent;

			VkClearValue clearColor = { {{ 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f }} };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = float(swapchain.extent.width);
			viewport.height = float(swapchain.extent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapchain.extent;

			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

			vkCmdDrawIndexed(commandBuffer, ARRAY_SIZE(indices), 1, 0, 0, 0);

			vkCmdEndRenderPass(commandBuffer);

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
		presentInfo.pSwapchains = &swapchain.swapchain;
		presentInfo.pImageIndices = &imageIndex;

		VK_CALL(vkQueuePresentKHR(graphicsQueue, &presentInfo));

		currentFrame = (currentFrame + 1) % kMaxFramesInFlightCount;
	}

	VK_CALL(vkDeviceWaitIdle(device));

	{
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

	return 0;
}
