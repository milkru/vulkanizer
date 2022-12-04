#include "device.h"
#include "frame_pacing.h"

static VkSemaphore createSemaphore(
	VkDevice _device)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore;
	VK_CALL(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &semaphore));

	return semaphore;
}

static VkFence createFence(
	VkDevice _device)
{
	VkFenceCreateInfo fenceCreateInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence;
	VK_CALL(vkCreateFence(_device, &fenceCreateInfo, nullptr, &fence));

	return fence;
}

FramePacingState createFramePacingState(
	Device& _rDevice)
{
	return {
		.imageAvailableSemaphore = createSemaphore(_rDevice.device),
		.renderFinishedSemaphore = createSemaphore(_rDevice.device),
		.inFlightFence = createFence(_rDevice.device) };
}

void destroyFramePacingState(
	Device& _rDevice,
	FramePacingState& _rFramePacingState)
{
	vkDestroySemaphore(_rDevice.device, _rFramePacingState.renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(_rDevice.device, _rFramePacingState.imageAvailableSemaphore, nullptr);
	vkDestroyFence(_rDevice.device, _rFramePacingState.inFlightFence, nullptr);

	_rFramePacingState = {};
}
