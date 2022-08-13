#pragma once

struct Buffer
{
	VkBuffer bufferVk = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	size_t size = 0;
	void* data = nullptr;
};

Buffer createBuffer(
	Device _device,
	VkBufferUsageFlags _usageFlags,
	VkMemoryPropertyFlags _memoryFlags,
	VkDeviceSize _size,
	void* _pContents = nullptr);

void destroyBuffer(
	Device _device,
	Buffer& _rBuffer);

VkImageView createImageView(
	VkDevice _device,
	VkImage _image,
	VkFormat _format);

struct Image
{
	VkImage imageVk = VK_NULL_HANDLE;
	// TODO-MILKRU: Probably a good idea to move view outside of the image struct,
	// since we will need to interpret one image in multiple ways in the future.
	VkImageView view = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
};

Image createImage(
	Device _device,
	VkImageUsageFlags _usage,
	VkMemoryPropertyFlags _memoryFlags,
	uint32_t _width,
	uint32_t _height,
	VkFormat _format,
	VkImageLayout _layout = VK_IMAGE_LAYOUT_UNDEFINED);

void destroyImage(
	Device _device,
	Image& _rImage);

void transferImageLayout(
	VkCommandBuffer _commmandBuffer,
	VkImage _image,
	VkImageAspectFlags _aspectMask,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
