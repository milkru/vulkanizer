#include "common.h"
#include "device.h"
#include "texture.h"

#include <string.h>

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

Texture createTexture(
	Device _device,
	TextureDesc _desc)
{
	assert(_desc.width != 0);
	assert(_desc.height != 0);
	assert(_desc.format != VK_FORMAT_UNDEFINED);
	assert(_desc.usage != 0);

	Texture texture = {
		.sampler = _desc.sampler,
		.format = _desc.format };

	VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = texture.format;
	imageCreateInfo.extent.width = _desc.width;
	imageCreateInfo.extent.height = _desc.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = _desc.usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VK_CALL(vmaCreateImage(_device.allocator, &imageCreateInfo,
		&allocationCreateInfo, &texture.resource, &texture.allocation, nullptr));

	texture.view = createImageView(_device.device, texture.resource, texture.format);

	if (_desc.layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		immediateSubmit(_device, [&](VkCommandBuffer _commandBuffer)
			{
				textureBarrier(_commandBuffer, texture, VK_IMAGE_ASPECT_DEPTH_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, _desc.layout);
			});
	}

	return texture;
}

void destroyTexture(
	Device _device,
	Texture& _rTexture)
{
	vkDestroyImageView(_device.device, _rTexture.view, nullptr);
	vmaDestroyImage(_device.allocator, _rTexture.resource, _rTexture.allocation);

	_rTexture = {};
}

VkSampler createSampler(
	VkDevice _device,
	SamplerDesc _desc)
{
	VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.magFilter = _desc.filter;
	samplerCreateInfo.minFilter = _desc.filter;
	samplerCreateInfo.mipmapMode = _desc.mipmapMode;
	samplerCreateInfo.addressModeU = _desc.addressMode;
	samplerCreateInfo.addressModeV = _desc.addressMode;
	samplerCreateInfo.addressModeW = _desc.addressMode;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VkSampler sampler;
	VK_CALL(vkCreateSampler(_device, &samplerCreateInfo, nullptr, &sampler));

	return sampler;
}

// Image layout transfers.
// https://github.com/SaschaWillems/Vulkan
void textureBarrier(
	VkCommandBuffer _commandBuffer,
	Texture _texture,
	VkImageAspectFlags _aspectMask,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	VkImageSubresourceRange subresourceRange = {
		.aspectMask = _aspectMask,
		.baseMipLevel = 0,
		.levelCount = 1,
		.layerCount = 1 };

	VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.oldLayout = _oldLayout;
	imageMemoryBarrier.newLayout = _newLayout;
	imageMemoryBarrier.image = _texture.resource;
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
