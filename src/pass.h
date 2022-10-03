#pragma once

struct Viewport
{
	glm::vec2 offset{};
	glm::vec2 extent{};
};

struct Scissor
{
	glm::ivec2 offset{};
	glm::uvec2 extent{};
};

struct Attachment
{
	Texture texture{};
	VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	VkClearValue clear = { 0 };
};

typedef std::initializer_list<Attachment> Attachments;

union Binding
{
	Binding(Buffer _buffer);
	Binding(Texture _texture);

	VkDescriptorImageInfo imageInfo;
	VkDescriptorBufferInfo bufferInfo;
};

typedef std::initializer_list<Binding> Bindings;

struct PushConstants
{
	uint32_t size = 0u;
	void* pData = nullptr;
};

struct PassDesc
{
	Pipeline pipeline{};
	Viewport viewport{};
	Scissor scissor{};
	Attachments colorAttachments;
	Attachment depthStencilAttachment{};
	Bindings bindings;
	PushConstants pushConstants;
};

void executePass(
	VkCommandBuffer _commandBuffer,
	PassDesc _desc,
	LAMBDA() _callback);
