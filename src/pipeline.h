#pragma once

struct ColorAttachmentState
{
	VkFormat format = VK_FORMAT_UNDEFINED;
	bool blendEnable = false;
};

struct AttachmentLayout
{
	std::initializer_list<ColorAttachmentState> colorAttachmentStates;
	VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
};

struct RasterizationState
{
	VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
	VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
};

struct DepthStencilState
{
	bool depthTestEnable = false;
	bool depthWriteEnable = false;
	VkCompareOp depthCompareOp = VK_COMPARE_OP_GREATER;
};

struct GraphicsPipelineDesc
{
	Shaders shaders;
	AttachmentLayout attachmentLayout{};
	RasterizationState rasterizationState{};
	DepthStencilState depthStencilState{};
};

struct Pipeline
{
	VkPipelineBindPoint type = VK_PIPELINE_BIND_POINT_MAX_ENUM;
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorUpdateTemplate updateTemplate = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPushConstantRange pushConstants{};
};

Pipeline createGraphicsPipeline(
	Device _device,
	GraphicsPipelineDesc _desc);

Pipeline createComputePipeline(
	Device _device,
	Shader _shader);

void destroyPipeline(
	Device _device,
	Pipeline& _rPipeline);
