#include "common.h"
#include "device.h"
#include "buffer.h"
#include "texture.h"
#include "shader.h"
#include "pipeline.h"
#include "pass.h"

Binding::Binding(Buffer _buffer)
{
	bufferInfo.buffer = _buffer.resource;
	bufferInfo.offset = 0;
	bufferInfo.range = _buffer.size;
}

Binding::Binding(Texture _texture)
{
	imageInfo.sampler = _texture.sampler;
	imageInfo.imageView = _texture.view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void executePass(
	VkCommandBuffer _commandBuffer,
	PassDesc _desc,
	LAMBDA() _callback)
{
	if (_desc.pipeline.type == VK_PIPELINE_BIND_POINT_GRAPHICS)
	{
		VkRenderingInfoKHR renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
		renderingInfo.layerCount = 1;
		renderingInfo.renderArea.offset = {
			.x = int32_t(_desc.viewport.offset.x),
			.y = int32_t(_desc.viewport.offset.y) };
		renderingInfo.renderArea.extent = {
			.width = uint32_t(_desc.viewport.extent.x),
			.height = uint32_t(_desc.viewport.extent.y) };

		std::vector<VkRenderingAttachmentInfoKHR> colorAttachments;
		colorAttachments.reserve(_desc.colorAttachments.size());

		for (const Attachment& colorAttachment : _desc.colorAttachments)
		{
			VkRenderingAttachmentInfoKHR attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			attachment.imageView = colorAttachment.texture.view;
			attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
			attachment.loadOp = colorAttachment.loadOp;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.clearValue.color = colorAttachment.clear.color;

			colorAttachments.push_back(attachment);
		}

		renderingInfo.colorAttachmentCount = uint32_t(colorAttachments.size());
		renderingInfo.pColorAttachments = colorAttachments.data();

		VkRenderingAttachmentInfoKHR depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		depthAttachment.imageView = _desc.depthStencilAttachment.texture.view;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		depthAttachment.loadOp = _desc.depthStencilAttachment.loadOp;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.clearValue.depthStencil = _desc.depthStencilAttachment.clear.depthStencil;

		if (_desc.depthStencilAttachment.texture.view != VK_NULL_HANDLE)
		{
			renderingInfo.pDepthAttachment = &depthAttachment;
		}

		vkCmdBeginRenderingKHR(_commandBuffer, &renderingInfo);
	}

	if (_desc.viewport.extent.x != 0 && _desc.viewport.extent.y != 0)
	{
		VkViewport viewport = {
			.x = _desc.viewport.offset.x,
			.y = _desc.viewport.offset.y,
			.width = _desc.viewport.extent.x,
			.height = _desc.viewport.extent.y,
			.minDepth = 0.0f,
			.maxDepth = 1.0f };

		vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);
	}

	if (_desc.scissor.extent.x != 0 && _desc.scissor.extent.y != 0)
	{
		VkRect2D scissorRect = {
			.offset = {
				.x = _desc.scissor.offset.x,
				.y = _desc.scissor.offset.y },
			.extent = {
				.width = _desc.scissor.extent.x,
				.height = _desc.scissor.extent.y } };

		vkCmdSetScissor(_commandBuffer, 0, 1, &scissorRect);
	}

	if (_desc.pushConstants.size != 0 && _desc.pushConstants.pData != nullptr)
	{
		vkCmdPushConstants(_commandBuffer, _desc.pipeline.pipelineLayout,
			_desc.pipeline.pushConstants.stageFlags, 0, _desc.pushConstants.size, _desc.pushConstants.pData);
	}

	vkCmdPushDescriptorSetWithTemplateKHR(_commandBuffer, _desc.pipeline.updateTemplate,
		_desc.pipeline.pipelineLayout, 0, _desc.bindings.begin());

	vkCmdBindPipeline(_commandBuffer, _desc.pipeline.type, _desc.pipeline.pipeline);

	_callback();

	if (_desc.pipeline.type == VK_PIPELINE_BIND_POINT_GRAPHICS)
	{
		vkCmdEndRenderingKHR(_commandBuffer);
	}
}
