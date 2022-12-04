#include "device.h"
#include "buffer.h"
#include "texture.h"
#include "shader.h"
#include "pipeline.h"
#include "pass.h"

Binding::Binding(
	Buffer& _rBuffer)
{
	bufferInfo = {
		.buffer = _rBuffer.resource,
		.offset = 0u,
		.range = _rBuffer.byteSize };
}

Binding::Binding(
	Texture& _rTexture, VkImageLayout _layout)
{
	imageInfo = {
		.sampler = _rTexture.sampler.resource,
		.imageView = _rTexture.view,
		.imageLayout = _layout };
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
			.x = i32(_desc.viewport.offset.x),
			.y = i32(_desc.viewport.offset.y) };
		renderingInfo.renderArea.extent = {
			.width = u32(_desc.viewport.extent.x),
			.height = u32(_desc.viewport.extent.y) };

		std::vector<VkRenderingAttachmentInfoKHR> colorAttachments;
		colorAttachments.reserve(_desc.colorAttachments.size());

		for (const Attachment& rColorAttachment : _desc.colorAttachments)
		{
			assert(rColorAttachment.texture.resource != VK_NULL_HANDLE);

			VkRenderingAttachmentInfoKHR attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
			attachment.imageView = rColorAttachment.texture.view;
			attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
			attachment.loadOp = rColorAttachment.loadOp;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.clearValue.color = rColorAttachment.clear.color;

			colorAttachments.push_back(attachment);
		}

		renderingInfo.colorAttachmentCount = u32(colorAttachments.size());
		renderingInfo.pColorAttachments = colorAttachments.data();

		VkRenderingAttachmentInfoKHR depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		if (_desc.depthStencilAttachment.texture.resource != VK_NULL_HANDLE)
		{
			depthAttachment.imageView = _desc.depthStencilAttachment.texture.view;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
			depthAttachment.loadOp = _desc.depthStencilAttachment.loadOp;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.clearValue.depthStencil = _desc.depthStencilAttachment.clear.depthStencil;

			renderingInfo.pDepthAttachment = &depthAttachment;
		}

		vkCmdBeginRenderingKHR(_commandBuffer, &renderingInfo);

		if (_desc.viewport.extent.x != 0.0f && _desc.viewport.extent.y != 0.0f)
		{
			VkViewport viewport = {
				.x = _desc.viewport.offset.x,
				.y = _desc.viewport.offset.y,
				.width = _desc.viewport.extent.x,
				.height = _desc.viewport.extent.y,
				.minDepth = 0.0f,
				.maxDepth = 1.0f };

			vkCmdSetViewport(_commandBuffer, 0u, 1u, &viewport);
		}

		if (_desc.scissor.extent.x != 0u && _desc.scissor.extent.y != 0u)
		{
			VkRect2D scissorRect = {
				.offset = {
					.x = _desc.scissor.offset.x,
					.y = _desc.scissor.offset.y },
				.extent = {
					.width = _desc.scissor.extent.x,
					.height = _desc.scissor.extent.y } };

			vkCmdSetScissor(_commandBuffer, 0u, 1u, &scissorRect);
		}
	}

	if (_desc.pushConstants.byteSize != 0u && _desc.pushConstants.pData != nullptr)
	{
		vkCmdPushConstants(_commandBuffer, _desc.pipeline.pipelineLayout,
			_desc.pipeline.pushConstants.stageFlags, 0u, _desc.pushConstants.byteSize, _desc.pushConstants.pData);
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
