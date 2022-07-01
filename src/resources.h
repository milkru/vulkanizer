#pragma once

struct Buffer
{
	VkBuffer bufferVk = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	size_t size = 0;
	void* data = nullptr;
};

Buffer createBuffer(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkBufferUsageFlags _usageFlags,
	VkMemoryPropertyFlags _memoryFlags,
	VkDeviceSize _size);

Buffer createBuffer(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkQueue _queue,
	VkCommandPool _commandPool,
	VkBufferUsageFlags _usageFlags,
	VkDeviceSize _size,
	void* _pContents = nullptr);

void destroyBuffer(
	VkDevice _device,
	Buffer& _rBuffer);

void transferImageLayout(
	VkCommandBuffer _commmandBuffer,
	VkImage _image,
	VkImageAspectFlags _aspectMask,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask);
