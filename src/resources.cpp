#include "common.h"
#include "device.h"
#include "pipeline.h"
#include "resources.h"

#include <string.h>

Buffer createBuffer(
	Device _device,
	VkBufferUsageFlags _usageFlags,
	VkMemoryPropertyFlags _memoryFlags,
	VkDeviceSize _size,
	void* _pContents)
{
	assert(_size > 0);

	if (_pContents)
	{
		_usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferCreateInfo.size = _size;
	bufferCreateInfo.usage = _usageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	Buffer buffer{};
	VK_CALL(vkCreateBuffer(_device.deviceVk, &bufferCreateInfo, nullptr, &buffer.bufferVk));

	buffer.size = _size;

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(_device.deviceVk, buffer.bufferVk, &memoryRequirements);

	uint32_t memoryTypeIndex = tryFindMemoryType(_device, memoryRequirements.memoryTypeBits, _memoryFlags);
	assert(memoryTypeIndex != ~0u);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CALL(vkAllocateMemory(_device.deviceVk, &allocateInfo, nullptr, &buffer.memory));

	vkBindBufferMemory(_device.deviceVk, buffer.bufferVk, buffer.memory, 0);

	if (_memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		// Persistently mapped memory, which should be faster on NVidia.
		vkMapMemory(_device.deviceVk, buffer.memory, 0, _size, 0, &buffer.data);
	}

	if (_pContents)
	{
		Buffer stagingBuffer = createBuffer(_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _size);

		memcpy(stagingBuffer.data, _pContents, stagingBuffer.size);

		immediateSubmit(_device, [&](VkCommandBuffer _commandBuffer)
			{
				VkBufferCopy copyRegion{};
				copyRegion.size = _size;
				vkCmdCopyBuffer(_commandBuffer, stagingBuffer.bufferVk, buffer.bufferVk, 1, &copyRegion);
			});

		destroyBuffer(_device, stagingBuffer);
	}

	return buffer;
}

void destroyBuffer(
	Device _device,
	Buffer& _rBuffer)
{
	if (_rBuffer.data)
	{
		vkUnmapMemory(_device.deviceVk, _rBuffer.memory);
	}

	vkDestroyBuffer(_device.deviceVk, _rBuffer.bufferVk, nullptr);
	vkFreeMemory(_device.deviceVk, _rBuffer.memory, nullptr);

	_rBuffer = {};
}

VkImageView createImageView(
	VkDevice _device,
	VkImage _image,
	VkFormat _format)
{
	VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = _image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = _format;
	imageViewCreateInfo.subresourceRange.aspectMask = (_format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VK_CALL(vkCreateImageView(_device, &imageViewCreateInfo, 0, &imageView));

	return imageView;
}

Image createImage(
	Device _device,
	VkImageUsageFlags _usage,
	VkMemoryPropertyFlags _memoryFlags,
	uint32_t _width,
	uint32_t _height,
	VkFormat _format,
	VkImageLayout _layout)
{
	VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = _format;
	imageCreateInfo.extent.width = _width;
	imageCreateInfo.extent.height = _height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = _usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	Image image{};
	image.format = _format;

	VK_CALL(vkCreateImage(_device.deviceVk, &imageCreateInfo, nullptr, &image.imageVk));

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(_device.deviceVk, image.imageVk, &memoryRequirements);

	uint32_t memoryTypeIndex = tryFindMemoryType(_device, memoryRequirements.memoryTypeBits, _memoryFlags);
	assert(memoryTypeIndex != ~0u);

	VkMemoryAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex;

	VK_CALL(vkAllocateMemory(_device.deviceVk, &allocateInfo, nullptr, &image.memory));
	VK_CALL(vkBindImageMemory(_device.deviceVk, image.imageVk, image.memory, 0));

	image.view = createImageView(_device.deviceVk, image.imageVk, _format);

	if (_layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		immediateSubmit(_device, [&](VkCommandBuffer _commandBuffer)
			{
				transferImageLayout(_commandBuffer, image.imageVk, VK_IMAGE_ASPECT_DEPTH_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, _layout);
			});
	}

	return image;
}

void destroyImage(
	Device _device,
	Image& _rImage)
{
	vkDestroyImage(_device.deviceVk, _rImage.imageVk, nullptr);
	vkDestroyImageView(_device.deviceVk, _rImage.view, nullptr);
	vkFreeMemory(_device.deviceVk, _rImage.memory, nullptr);

	_rImage = {};
}

// Image layout transfers.
// https://github.com/SaschaWillems/Vulkan
void transferImageLayout(
	VkCommandBuffer _commandBuffer,
	VkImage _image,
	VkImageAspectFlags _aspectMask,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = _aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.oldLayout = _oldLayout;
	imageMemoryBarrier.newLayout = _newLayout;
	imageMemoryBarrier.image = _image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Source access mask controls actions that have to be finished on the old layout,
	// before it will be transitioned to the new layout.
	switch (_oldLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined.
		// Only valid as initial layout.
		// No flags required, listed only for completeness.
		imageMemoryBarrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized.
		// Only valid as initial layout for linear images, preserves memory contents.
		// Make sure host writes have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment.
		// Make sure any writes to the color buffer have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment.
		// Make sure any writes to the depth/stencil buffer have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source.
		// Make sure any reads from the image have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination.
		// Make sure any writes to the image have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader.
		// Make sure any shader reads from the image have been finished.
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

	default:
		break;
	}

	// Destination access mask controls the dependency for the new image layout.
	switch (_newLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination.
		// Make sure any writes to the image have been finished.
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source.
		// Make sure any reads from the image have been finished.
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment.
		// Make sure any writes to the color buffer have been finished.
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment.
		// Make sure any writes to depth/stencil buffer have been finished.
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader.
		// Make sure any writes to the image have been finished.
		if (imageMemoryBarrier.srcAccessMask == 0)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}

		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

	default:
		break;
	}

	vkCmdPipelineBarrier(
		_commandBuffer,
		_srcStageMask,
		_dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}
