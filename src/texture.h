#pragma once

struct TextureDesc
{
	uint32_t width = 0u;
	uint32_t height = 0u;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageUsageFlags usage = 0u;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkSampler sampler = VK_NULL_HANDLE;
};

struct Texture
{
	VkImage resource = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

Texture createTexture(
	Device _device,
	TextureDesc _desc);

void destroyTexture(
	Device _device,
	Texture& _rTexture);

VkImageView createImageView(
	VkDevice _device,
	VkImage _image,
	VkFormat _format);

struct SamplerDesc
{
	VkFilter filter = VK_FILTER_LINEAR;
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
};

VkSampler createSampler(
	VkDevice _device,
	SamplerDesc _desc);

void textureBarrier(
	VkCommandBuffer _commandBuffer,
	Texture _texture,
	VkImageAspectFlags _aspectMask,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
