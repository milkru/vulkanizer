#include "common.h"
#include "device.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

static VkInstance createInstance(
	bool _bEnableValidationLayers)
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
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
	};

	VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	applicationInfo.pApplicationName = "vulkanizer";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.pEngineName = "vulkanizer";
	applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceCreateInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceCreateInfo.pApplicationInfo = &applicationInfo;

	if (_bEnableValidationLayers)
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
	VkInstance _instance,
	bool _bEnableValidationLayers)
{
	if (!_bEnableValidationLayers)
	{
		return VK_NULL_HANDLE;
	}

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
	GLFWwindow* _pWindow,
	VkInstance _instance)
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

static const char* kRequiredDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
};

static bool isDeviceExtensionAvailable(
	const std::vector<VkExtensionProperties>& _availableExtensions,
	const char* _extensionName)
{
	for (const VkExtensionProperties& availableExtension : _availableExtensions)
	{
		if (strcmp(availableExtension.extensionName, _extensionName) == 0)
		{
			return true;
		}
	}

	return false;
}

static bool areRequiredDeviceExtensionSupported(const VkPhysicalDevice inDevice)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(inDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(inDevice, nullptr, &extensionCount, availableExtensions.data());

	for (const char* requiredExtensionName : kRequiredDeviceExtensions)
	{
		if (!isDeviceExtensionAvailable(availableExtensions, requiredExtensionName))
		{
			return false;
		}
	}

	return true;
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

		if (!areRequiredDeviceExtensionSupported(potentialPhysicalDevice))
		{
			continue;
		}

		physicalDevice = potentialPhysicalDevice;
		break;
	}

	return physicalDevice;
}

static bool isMeshShadingPipelineSupported(
	VkPhysicalDevice _physicalDevice)
{
	uint32_t extensionCount;

	vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(_physicalDevice, nullptr, &extensionCount, extensions.data());

	for (VkExtensionProperties& extension : extensions)
	{
		if (strcmp(extension.extensionName, VK_NV_MESH_SHADER_EXTENSION_NAME) == 0)
		{
			return true;
		}
	}

	return false;
}

static VkDevice createDevice(
	VkPhysicalDevice _physicalDevice,
	uint32_t _queueFamilyIndex,
	bool _bMeshShadingAllowed)
{
	std::vector<const char*> deviceExtensions(kRequiredDeviceExtensions, std::end(kRequiredDeviceExtensions));
	if (_bMeshShadingAllowed)
	{
		deviceExtensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
	}

	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueCreateInfo.queueFamilyIndex = _queueFamilyIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	deviceFeatures2.features.pipelineStatisticsQuery = VK_TRUE;
	deviceFeatures2.features.shaderInt16 = VK_TRUE;

	VkPhysicalDeviceVulkan11Features deviceFeatures11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	deviceFeatures11.storageBuffer16BitAccess = VK_TRUE;
	deviceFeatures11.shaderDrawParameters = VK_TRUE;

	VkPhysicalDeviceVulkan12Features deviceFeatures12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
	deviceFeatures12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	deviceFeatures12.storagePushConstant8 = VK_TRUE;
	deviceFeatures12.shaderFloat16 = VK_TRUE;
	deviceFeatures12.shaderInt8 = VK_TRUE;
	deviceFeatures12.drawIndirectCount = VK_TRUE;

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR };
	dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceMeshShaderFeaturesNV meshShaderFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV };
	meshShaderFeatures.taskShader = VK_TRUE;
	meshShaderFeatures.meshShader = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	deviceCreateInfo.pNext = &deviceFeatures2;
	deviceFeatures2.pNext = &deviceFeatures11;
	deviceFeatures11.pNext = &deviceFeatures12;
	deviceFeatures12.pNext = &dynamicRenderingFeatures;

	if (_bMeshShadingAllowed)
	{
		dynamicRenderingFeatures.pNext = &meshShaderFeatures;
	}

	VkDevice device;
	VK_CALL(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &device));

	return device;
}

static VmaAllocator createAllocator(
	VkInstance _instance,
	VkPhysicalDevice _physicalDevice,
	VkDevice _device)
{
	VmaVulkanFunctions volkFunctions =
	{
		vkGetInstanceProcAddr,
		vkGetDeviceProcAddr,
		vkGetPhysicalDeviceProperties,
		vkGetPhysicalDeviceMemoryProperties,
		vkAllocateMemory,
		vkFreeMemory,
		vkMapMemory,
		vkUnmapMemory,
		vkFlushMappedMemoryRanges,
		vkInvalidateMappedMemoryRanges,
		vkBindBufferMemory,
		vkBindImageMemory,
		vkGetBufferMemoryRequirements,
		vkGetImageMemoryRequirements,
		vkCreateBuffer,
		vkDestroyBuffer,
		vkCreateImage,
		vkDestroyImage,
		vkCmdCopyBuffer,
		vkGetBufferMemoryRequirements2KHR,
		vkGetImageMemoryRequirements2KHR,
		vkBindBufferMemory2KHR,
		vkBindImageMemory2KHR,
		vkGetPhysicalDeviceMemoryProperties2KHR,
		vkGetDeviceBufferMemoryRequirements,
		vkGetDeviceImageMemoryRequirements,
	};

	VmaAllocatorCreateInfo allocatorCreateInfo{};
	allocatorCreateInfo.vulkanApiVersion = volkGetInstanceVersion();
	allocatorCreateInfo.instance = _instance;
	allocatorCreateInfo.physicalDevice = _physicalDevice;
	allocatorCreateInfo.device = _device;
	allocatorCreateInfo.pVulkanFunctions = &volkFunctions;

	VmaAllocator allocator;
	VK_CALL(vmaCreateAllocator(&allocatorCreateInfo, &allocator));

	return allocator;
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

Device createDevice(
	GLFWwindow* _pWindow,
	DeviceDesc _desc)
{
	VK_CALL(volkInitialize());

	Device device{};

	device.instance = createInstance(_desc.bEnableValidationLayers);
	device.debugMessenger = createDebugMessenger(device.instance, _desc.bEnableValidationLayers);
	device.surface = createSurface(_pWindow, device.instance);
	
	device.physicalDevice = tryPickPhysicalDevice(device.instance, device.surface);
	assert(device.physicalDevice != VK_NULL_HANDLE);

	device.graphicsQueue.index = tryGetGraphicsQueueFamilyIndex(device.physicalDevice);
	assert(device.graphicsQueue.index != ~0u);

	device.bMeshShadingPipelineAllowed = _desc.bEnableMeshShadingPipeline && isMeshShadingPipelineSupported(device.physicalDevice);
	device.device = createDevice(device.physicalDevice, device.graphicsQueue.index, device.bMeshShadingPipelineAllowed);

	vkGetDeviceQueue(device.device, device.graphicsQueue.index, 0, &device.graphicsQueue.queue);

	device.allocator = createAllocator(device.instance, device.physicalDevice, device.device);

	device.commandPool = createCommandPool(device.device, device.graphicsQueue.index);

	return device;
}

void destroyDevice(
	Device& _rDevice)
{
	vkDestroyCommandPool(_rDevice.device, _rDevice.commandPool, nullptr);

	vmaDestroyAllocator(_rDevice.allocator);

	vkDestroyDevice(_rDevice.device, nullptr);

	if (_rDevice.debugMessenger != VK_NULL_HANDLE)
	{
		vkDestroyDebugUtilsMessengerEXT(_rDevice.instance, _rDevice.debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(_rDevice.instance, _rDevice.surface, nullptr);
	vkDestroyInstance(_rDevice.instance, nullptr);

	_rDevice = {};
}

VkCommandBuffer createCommandBuffer(
	Device _device)
{
	VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocateInfo.commandPool = _device.commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CALL(vkAllocateCommandBuffers(_device.device, &allocateInfo, &commandBuffer));

	return commandBuffer;
}

void immediateSubmit(
	Device _device,
	LAMBDA(VkCommandBuffer) _callback)
{
	VkCommandBuffer commandBuffer = createCommandBuffer(_device);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	_callback(commandBuffer);

	VK_CALL(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CALL(vkQueueSubmit(_device.graphicsQueue.queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL(vkQueueWaitIdle(_device.graphicsQueue.queue));

	vkFreeCommandBuffers(_device.device, _device.commandPool, 1, &commandBuffer);
}
