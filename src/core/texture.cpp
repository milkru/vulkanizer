#include "device.h"
#include "texture.h"

#include <string.h>
#include <CRC.h>

struct SharedSampler
{
	u32 shareCount = 0;
	VkSampler sampler = VK_NULL_HANDLE;
};

static std::map<u32, SharedSampler> gSamplerCache;

static VkImageAspectFlags getAspectMask(
	VkFormat _format)
{
	return _format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

static VkImageView createImageView(
	VkDevice _device,
	VkImage _image,
	VkFormat _format,
	u32 _baseMipLevel,
	u32 _levelCount)
{
	VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = _image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = _format;
	imageViewCreateInfo.subresourceRange.aspectMask = getAspectMask(_format);
	imageViewCreateInfo.subresourceRange.baseMipLevel = _baseMipLevel;
	imageViewCreateInfo.subresourceRange.levelCount = _levelCount;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VK_CALL(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &imageView));

	return imageView;
}

static u32 calculateSamplerHash(
	SamplerDesc _desc)
{
	u32 hash = CRC::Calculate(&_desc.filterMode, sizeof(_desc.filterMode), CRC::CRC_32());
	hash = CRC::Calculate(&_desc.reductionMode, sizeof(_desc.reductionMode), CRC::CRC_32(), hash);
	hash = CRC::Calculate(&_desc.addressMode, sizeof(_desc.addressMode), CRC::CRC_32(), hash);
	hash = CRC::Calculate(&_desc.mipmapMode, sizeof(_desc.mipmapMode), CRC::CRC_32(), hash);
	return hash;
}

static Sampler getOrCreateSampler(
	VkDevice _device,
	SamplerDesc _desc)
{
	u32 hash = calculateSamplerHash(_desc);
	auto sharedSampler = gSamplerCache.find(hash);

	if (sharedSampler != gSamplerCache.end())
	{
		++sharedSampler->second.shareCount;

		return {
			.hash = hash,
			.resource = sharedSampler->second.sampler };
	}

	VkSamplerReductionModeCreateInfo reductionMode = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO };
	reductionMode.reductionMode = _desc.reductionMode;

	VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.pNext = &reductionMode;
	samplerCreateInfo.magFilter = _desc.filterMode;
	samplerCreateInfo.minFilter = _desc.filterMode;
	samplerCreateInfo.mipmapMode = _desc.mipmapMode;
	samplerCreateInfo.addressModeU = _desc.addressMode;
	samplerCreateInfo.addressModeV = _desc.addressMode;
	samplerCreateInfo.addressModeW = _desc.addressMode;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerCreateInfo.maxLod = 16.0f;
	samplerCreateInfo.maxAnisotropy = 8.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;

	VkSampler sampler;
	VK_CALL(vkCreateSampler(_device, &samplerCreateInfo, nullptr, &sampler));

	gSamplerCache[hash] = {
		.shareCount = 1,
		.sampler = sampler };

	return {
		.hash = hash,
		.resource = sampler };
}

static void destroySampler(
	Device& _rDevice,
	Sampler& _rSampler)
{
	auto& rSharedSampler = gSamplerCache[_rSampler.hash];

	assert(rSharedSampler.sampler == _rSampler.resource);
	assert(rSharedSampler.shareCount > 0);

	if (rSharedSampler.shareCount == 1)
	{
		vkDestroySampler(_rDevice.device, rSharedSampler.sampler, nullptr);
	}

	--rSharedSampler.shareCount;
}

Texture createTexture(
	Device& _rDevice,
	TextureDesc _desc)
{
	assert(_desc.mipCount > 0);
	assert(_desc.width != 0);
	assert(_desc.height != 0);
	assert(_desc.format != VK_FORMAT_UNDEFINED);

	Texture texture = {
		.width = _desc.width,
		.height = _desc.height,
		.mipIndex = 0,
		.mipCount = _desc.mipCount,
		.format = _desc.format,
		.resource = _desc.resource,
		.bFromSwapchain = _desc.resource != VK_NULL_HANDLE };

	if (texture.resource == VK_NULL_HANDLE)
	{
		assert(_desc.usage != 0);

		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = texture.format;
		imageCreateInfo.extent.width = _desc.width;
		imageCreateInfo.extent.height = _desc.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = _desc.mipCount;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = _desc.usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo allocationCreateInfo{};
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		VK_CALL(vmaCreateImage(_rDevice.allocator, &imageCreateInfo,
			&allocationCreateInfo, &texture.resource, &texture.allocation, nullptr));
	}

	texture.sampler = getOrCreateSampler(_rDevice.device, _desc.sampler);
	texture.view = createImageView(_rDevice.device, texture.resource, texture.format, 0, _desc.mipCount);

	if (_desc.layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		immediateSubmit(_rDevice, [&](VkCommandBuffer _commandBuffer)
			{
				textureBarrier(_commandBuffer, texture,
					VK_IMAGE_LAYOUT_UNDEFINED, _desc.layout,
					VK_ACCESS_NONE, _desc.access);
			});
	}

	return texture;
}

void destroyTexture(
	Device& _rDevice,
	Texture& _rTexture)
{
	vkDestroyImageView(_rDevice.device, _rTexture.view, nullptr);
	destroySampler(_rDevice, _rTexture.sampler);

	if (!_rTexture.bFromSwapchain)
	{
		vmaDestroyImage(_rDevice.allocator, _rTexture.resource, _rTexture.allocation);
	}
}

Texture createTextureView(
	Device& _rDevice,
	TextureViewDesc _desc)
{
	assert(_desc.mipCount > 0);
	assert(_desc.pParent);

	Texture texture = *_desc.pParent;
	texture.mipIndex = _desc.mipIndex;
	texture.mipCount = _desc.mipCount;
	texture.sampler = getOrCreateSampler(_rDevice.device, _desc.sampler);
	texture.view = createImageView(_rDevice.device, _desc.pParent->resource,
		_desc.pParent->format, _desc.mipIndex, _desc.mipCount);

	return texture;
}

void destroyTextureView(
	Device& _rDevice,
	Texture& _rTexture)
{
	vkDestroyImageView(_rDevice.device, _rTexture.view, nullptr);
	destroySampler(_rDevice, _rTexture.sampler);
}

void textureBarrier(
	VkCommandBuffer _commandBuffer,
	Texture& _rTexture,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask,
	VkPipelineStageFlags _dstStageMask)
{
	VkImageSubresourceRange subresourceRange = {
		.aspectMask = getAspectMask(_rTexture.format),
		.baseMipLevel = _rTexture.mipIndex,
		.levelCount = _rTexture.mipCount,
		.baseArrayLayer = 0,
		.layerCount = 1 };

	VkImageMemoryBarrier imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.srcAccessMask = _srcAccessMask;
	imageMemoryBarrier.dstAccessMask = _dstAccessMask;
	imageMemoryBarrier.oldLayout = _oldLayout;
	imageMemoryBarrier.newLayout = _newLayout;
	imageMemoryBarrier.image = _rTexture.resource;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// TODO-MILKRU: Store all pipeline barriers and flush them together once they are needed (i.e. before draw call).
	vkCmdPipelineBarrier(
		_commandBuffer,
		_srcStageMask,
		_dstStageMask,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}
