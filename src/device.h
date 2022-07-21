#pragma once

struct Queue
{
	VkQueue queueVk = VK_NULL_HANDLE;
	uint32_t index = ~0u;
};

struct Device
{
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice deviceVk = VK_NULL_HANDLE;
	Queue graphicsQueue{};
	VkCommandPool commandPool = VK_NULL_HANDLE;
	bool bMeshShadingPipelineSupported = false;
};

Device createDevice(
	GLFWwindow* _pWindow);

void destroyDevice(
	Device& _rDevice);

uint32_t tryFindMemoryType(
	Device _device,
	uint32_t _typeFilter,
	VkMemoryPropertyFlags _memoryFlags);

VkCommandBuffer createCommandBuffer(
	Device _device);

template<typename Lambda>
void immediateSubmit(
	Device _device,
	Lambda&& _callback)
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

	VK_CALL(vkQueueSubmit(_device.graphicsQueue.queueVk, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL(vkQueueWaitIdle(_device.graphicsQueue.queueVk));

	vkFreeCommandBuffers(_device.deviceVk, _device.commandPool, 1, &commandBuffer);
}
