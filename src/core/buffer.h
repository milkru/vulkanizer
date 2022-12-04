#pragma once

struct Buffer
{
	VkDeviceSize byteSize = 0ull;
	VkBuffer resource = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	void* pMappedData = nullptr;
};

enum class MemoryAccess : u8
{
	Host,
	Device,
};

struct BufferDesc
{
	u64 byteSize = 0ull;                         // Buffer size in bytes.
	MemoryAccess access = MemoryAccess::Device;  // Buffer memory access.
	VkBufferUsageFlags usage = 0u;               // Buffer usage flags.
	void* pContents = nullptr;                   // [Optional] Initial buffer contents.
};

Buffer createBuffer(
	Device& _rDevice,
	BufferDesc _desc);

void destroyBuffer(
	Device& _rDevice,
	Buffer& _rBuffer);

void bufferBarrier(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Buffer& _rBuffer,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

void fillBuffer(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Buffer& _rBuffer,
	u32 _value,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
