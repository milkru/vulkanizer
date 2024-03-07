#pragma once

struct Sampler
{
	u32 hash;
	VkSampler resource;
};

struct Texture
{
	u32 width = 0;
	u32 height = 0;
	u32 mipIndex = 0;
	u32 mipCount = 1;
	VkFormat format = VK_FORMAT_UNDEFINED;
	Sampler sampler{};
	VkImageView view = VK_NULL_HANDLE;
	VkImage resource = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	bool bFromSwapchain = false;
};

struct SamplerDesc
{
	VkFilter filterMode = VK_FILTER_LINEAR;                                             // Filter mode.
	VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;  // Reduction mode for linear filtering.
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;           // Address mode.
	VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;                    // Mipmap mode.
};

struct TextureDesc
{
	u32 width = 0;                                     // Texture width in pixels.
	u32 height = 0;                                    // Texture height in pixels.
	u32 mipCount = 1;                                  // Number of mipmaps.
	VkFormat format = VK_FORMAT_UNDEFINED;             // Texture pixel format.
	VkImageUsageFlags usage = 0;                       // Texture usage flags.
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;  // [Optional] Initial texture layout.
	VkAccessFlags access = VK_ACCESS_NONE;             // [Optional] Initial texture access.
	SamplerDesc sampler{};                             // Sampler descriptor.
	VkImage resource = VK_NULL_HANDLE;                 // [Optional] Usually used for swapchain images.
};

struct TextureViewDesc
{
	u32 mipIndex = 0;           // First mipmap used for texture view.
	u32 mipCount = 1;           // Number of mipmaps.
	SamplerDesc sampler{};      // Sampler descriptor.
	Texture* pParent = nullptr; // [Optional] Original texture state for custom texture views.
};

Texture createTexture(
	Device& _rDevice,
	TextureDesc _desc);

void destroyTexture(
	Device& _rDevice,
	Texture& _rTexture);

Texture createTextureView(
	Device& _rDevice,
	TextureViewDesc _desc);

void destroyTextureView(
	Device& _rDevice,
	Texture& _rTexture);

void textureBarrier(
	VkCommandBuffer _commandBuffer,
	Texture& _rTexture,
	VkImageLayout _oldLayout,
	VkImageLayout _newLayout,
	VkAccessFlags _srcAccessMask,
	VkAccessFlags _dstAccessMask,
	VkPipelineStageFlags _srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
	VkPipelineStageFlags _dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
