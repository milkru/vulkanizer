#include "common.h"
#include "device.h"
#include "buffer.h"

Buffer createBuffer(
	Device _device,
	BufferDesc _desc)
{
	assert(_desc.size > 0);
	assert(_desc.usage != 0);

	if (_desc.pContents)
	{
		_desc.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.size = _desc.size;
	bufferCreateInfo.usage = _desc.usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	// MILKRU-NOTE: This enables only sequential writes into this memory,
	// so if we end up needing random access, use VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
	allocationCreateInfo.flags = _desc.access == MemoryAccess::Host ?
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

	Buffer buffer = { .size = _desc.size };

	VK_CALL(vmaCreateBuffer(_device.allocator, &bufferCreateInfo,
		&allocationCreateInfo, &buffer.resource, &buffer.allocation, nullptr));

	if (_desc.access == MemoryAccess::Host)
	{
		// Persistently mapped memory, which should be faster on NVidia.
		vmaMapMemory(_device.allocator, buffer.allocation, &buffer.pMappedData);
	}

	if (_desc.pContents)
	{
		if (_desc.access == MemoryAccess::Host)
		{
			memcpy(buffer.pMappedData, _desc.pContents, buffer.size);
		}
		else
		{
			Buffer stagingBuffer = createBuffer(_device, {
				.size = _desc.size,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.pContents = _desc.pContents });

			immediateSubmit(_device, [&](VkCommandBuffer _commandBuffer)
				{
					VkBufferCopy copyRegion = { .size = _desc.size };
					vkCmdCopyBuffer(_commandBuffer, stagingBuffer.resource, buffer.resource, 1, &copyRegion);
				});

			destroyBuffer(_device, stagingBuffer);
		}
	}

	return buffer;
}

void destroyBuffer(
	Device _device,
	Buffer& _rBuffer)
{
	if (_rBuffer.pMappedData)
	{
		vmaUnmapMemory(_device.allocator, _rBuffer.allocation);
	}

	vmaDestroyBuffer(_device.allocator, _rBuffer.resource, _rBuffer.allocation);

	_rBuffer = {};
}

void bufferBarrier(
	VkCommandBuffer _commandBuffer,
	Device _device,
	Buffer _buffer,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	VkBufferMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	memoryBarrier.buffer = _buffer.resource;
	memoryBarrier.size = _buffer.size;
	memoryBarrier.srcAccessMask = _srcAccessMask;
	memoryBarrier.dstAccessMask = _dstAccessMask;
	memoryBarrier.srcQueueFamilyIndex = _device.graphicsQueue.index;
	memoryBarrier.dstQueueFamilyIndex = _device.graphicsQueue.index;

	vkCmdPipelineBarrier(
		_commandBuffer,
		_srcStageMask,
		_dstStageMask,
		0,
		0, nullptr,
		1, &memoryBarrier,
		0, nullptr);
}

void fillBuffer(
	VkCommandBuffer _commandBuffer,
	Device _device,
	Buffer _buffer,
	uint32_t _data,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	bufferBarrier(_commandBuffer, _device, _buffer,
		_srcAccessMask, VK_ACCESS_TRANSFER_WRITE_BIT,
		_srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT);

	vkCmdFillBuffer(_commandBuffer, _buffer.resource, 0, _buffer.size, _data);

	bufferBarrier(_commandBuffer, _device, _buffer,
		VK_ACCESS_TRANSFER_WRITE_BIT, _dstAccessMask,
		VK_PIPELINE_STAGE_TRANSFER_BIT, _dstStageMask);
}
