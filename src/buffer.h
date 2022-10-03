#pragma once

enum class MemoryAccess : uint8_t
{
	Host,
	Device,
};

struct BufferDesc
{
	uint64_t size = 0ull;
	MemoryAccess access = MemoryAccess::Device;
	VkBufferUsageFlags usage = 0u;
	void* pContents = nullptr;
};

struct Buffer
{
	VkDeviceSize size = 0ull;
	VkBuffer resource = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	void* pMappedData = nullptr;
};

Buffer createBuffer(
	Device _device,
	BufferDesc _desc);

void destroyBuffer(
	Device _device,
	Buffer& _rBuffer);

void bufferBarrier(
	VkCommandBuffer _commandBuffer,
	Device _device,
	Buffer _buffer,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

void fillBuffer(
	VkCommandBuffer _commandBuffer,
	Device _device,
	Buffer _buffer,
	uint32_t _data,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
