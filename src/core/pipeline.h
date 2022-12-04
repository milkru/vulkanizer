#pragma once

struct Pipeline
{
	VkPipelineBindPoint type = VK_PIPELINE_BIND_POINT_MAX_ENUM;
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorUpdateTemplate updateTemplate = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPushConstantRange pushConstants{};
};

struct RasterizationDesc
{
	VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;         // Rasterization cull mode.
	VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;  // Front face orientation for culling.
};

struct DepthStencilDesc
{
	bool bDepthTestEnable = false;                       // Enable depth testing.
	bool bDepthWriteEnable = false;                      // Enable writing to depth buffer.
	VkCompareOp depthCompareOp = VK_COMPARE_OP_GREATER;  // Comparison operation for depth testing.
};

struct ColorAttachmentDesc
{
	VkFormat format = VK_FORMAT_UNDEFINED;  // Color render target format.
	bool bBlendEnable = false;              // Enable color blending.
};

struct AttachmentLayout
{
	std::initializer_list<ColorAttachmentDesc> colorAttachments;
	VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
};

struct GraphicsPipelineDesc
{
	Shaders shaders;                      // Graphics pipeline shaders.
	AttachmentLayout attachmentLayout{};  // Render target layout.
	RasterizationDesc rasterization{};    // Rasterization descriptor.
	DepthStencilDesc depthStencil{};      // Depth stencil descriptor.
};

Pipeline createGraphicsPipeline(
	Device& _rDevice,
	GraphicsPipelineDesc _desc);

Pipeline createComputePipeline(
	Device& _rDevice,
	Shader& _rShader);

void destroyPipeline(
	Device& _rDevice,
	Pipeline& _rPipeline);
