#pragma once

const uint32_t kMaxFramesInFlightCount = 2;

VkInstance createInstance();

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT _messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT _messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData,
	void* _pUserData);

VkDebugUtilsMessengerEXT createDebugMessenger(
	VkInstance _instance);

VkSurfaceKHR createSurface(
	VkInstance _instance,
	GLFWwindow* _pWindow);

uint32_t tryGetGraphicsQueueFamilyIndex(
	VkPhysicalDevice _physicalDevice);

VkPhysicalDevice tryPickPhysicalDevice(
	VkInstance _instance,
	VkSurfaceKHR _surface);

VkDevice createDevice(
	VkPhysicalDevice _physicalDevice,
	uint32_t _queueFamilyIndex);

uint32_t tryFindMemoryType(
	VkPhysicalDevice _physicalDevice,
	uint32_t _typeFilter,
	VkMemoryPropertyFlags _memoryFlags);

VkCommandPool createCommandPool(
	VkDevice _device,
	uint32_t _queueFamilyIndex);

VkCommandBuffer createCommandBuffer(
	VkDevice _device,
	VkCommandPool _commandPool);

template<typename T>
void immediateSubmit(
	VkDevice _device,
	VkQueue _queue,
	VkCommandPool _commandPool,
	T&& _lambda)
{
	VkCommandBuffer commandBuffer = createCommandBuffer(_device, _commandPool);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	_lambda(commandBuffer);

	VK_CALL(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CALL(vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL(vkQueueWaitIdle(_queue));

	vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
}
