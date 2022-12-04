#include "device.h"
#include "buffer.h"

Buffer createBuffer(
	Device& _rDevice,
	BufferDesc _desc)
{
	assert(_desc.byteSize > 0u);
	assert(_desc.usage != 0u);

	if (_desc.pContents)
	{
		_desc.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.size = _desc.byteSize;
	bufferCreateInfo.usage = _desc.usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	// This enables only sequential writes into this memory,
	// so if we end up needing random access, use VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
	allocationCreateInfo.flags = _desc.access == MemoryAccess::Host ?
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0u;

	Buffer buffer = { .byteSize = _desc.byteSize };

	VK_CALL(vmaCreateBuffer(_rDevice.allocator, &bufferCreateInfo,
		&allocationCreateInfo, &buffer.resource, &buffer.allocation, nullptr));

	if (_desc.access == MemoryAccess::Host)
	{
		// Persistently mapped memory, which should be faster on NVidia.
		vmaMapMemory(_rDevice.allocator, buffer.allocation, &buffer.pMappedData);
	}

	if (_desc.pContents)
	{
		if (_desc.access == MemoryAccess::Host)
		{
			memcpy(buffer.pMappedData, _desc.pContents, buffer.byteSize);
		}
		else
		{
			Buffer stagingBuffer = createBuffer(_rDevice, {
				.byteSize = _desc.byteSize,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.pContents = _desc.pContents });

			immediateSubmit(_rDevice, [&](VkCommandBuffer _commandBuffer)
				{
					VkBufferCopy copyRegion = { .size = _desc.byteSize };
					vkCmdCopyBuffer(_commandBuffer, stagingBuffer.resource, buffer.resource, 1, &copyRegion);
				});

			destroyBuffer(_rDevice, stagingBuffer);
		}
	}

	return buffer;
}

void destroyBuffer(
	Device& _rDevice,
	Buffer& _rBuffer)
{
	if (_rBuffer.pMappedData)
	{
		vmaUnmapMemory(_rDevice.allocator, _rBuffer.allocation);
	}

	vmaDestroyBuffer(_rDevice.allocator, _rBuffer.resource, _rBuffer.allocation);
}

void bufferBarrier(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Buffer& _rBuffer,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	VkBufferMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	memoryBarrier.buffer = _rBuffer.resource;
	memoryBarrier.size = _rBuffer.byteSize;
	memoryBarrier.srcAccessMask = _srcAccessMask;
	memoryBarrier.dstAccessMask = _dstAccessMask;
	memoryBarrier.srcQueueFamilyIndex = _rDevice.graphicsQueue.index;
	memoryBarrier.dstQueueFamilyIndex = _rDevice.graphicsQueue.index;

	vkCmdPipelineBarrier(
		_commandBuffer,
		_srcStageMask,
		_dstStageMask,
		0u,
		0u, nullptr,
		1u, &memoryBarrier,
		0u, nullptr);
}

void fillBuffer(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Buffer& _rBuffer,
	u32 _value,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	bufferBarrier(_commandBuffer, _rDevice, _rBuffer,
		_srcAccessMask, VK_ACCESS_TRANSFER_WRITE_BIT,
		_srcStageMask, VK_PIPELINE_STAGE_TRANSFER_BIT);

	vkCmdFillBuffer(_commandBuffer, _rBuffer.resource, 0u, _rBuffer.byteSize, _value);

	bufferBarrier(_commandBuffer, _rDevice, _rBuffer,
		VK_ACCESS_TRANSFER_WRITE_BIT, _dstAccessMask,
		VK_PIPELINE_STAGE_TRANSFER_BIT, _dstStageMask);
}
