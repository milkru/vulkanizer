#include "common.h"
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
	Device _device)
{
	return {
		.imageAvailableSemaphore = createSemaphore(_device.device),
		.renderFinishedSemaphore = createSemaphore(_device.device),
		.inFlightFence = createFence(_device.device) };
}

void destroyFramePacingState(
	Device _device,
	FramePacingState& _rFramePacingState)
{
	vkDestroySemaphore(_device.device, _rFramePacingState.renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(_device.device, _rFramePacingState.imageAvailableSemaphore, nullptr);
	vkDestroyFence(_device.device, _rFramePacingState.inFlightFence, nullptr);

	_rFramePacingState = {};
}
