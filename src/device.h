#pragma once

struct DeviceDesc
{
	bool bEnableValidationLayers = false;
	bool bEnableMeshShadingPipeline = true;
};

struct Queue
{
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t index = ~0u;
};

struct Device
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	Queue graphicsQueue{};
	VmaAllocator allocator = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	bool bMeshShadingPipelineAllowed = false;
};

Device createDevice(
	GLFWwindow* _pWindow,
	DeviceDesc _desc);

void destroyDevice(
	Device& _rDevice);

VkCommandBuffer createCommandBuffer(
	Device _device);

void immediateSubmit(
	Device _device,
	LAMBDA(VkCommandBuffer) _callback);
