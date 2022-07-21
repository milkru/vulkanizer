#include "common.h"
#include "device.h"
#include "sync.h"

VkSemaphore createSemaphore(
	VkDevice _device)
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore;
	VK_CALL(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &semaphore));

	return semaphore;
}

VkFence createFence(
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
	FramePacingState framePacing{};
	framePacing.imageAvailableSemaphore = createSemaphore(_device.deviceVk);
	framePacing.renderFinishedSemaphore = createSemaphore(_device.deviceVk);
	framePacing.inFlightFence = createFence(_device.deviceVk);

	return framePacing;
}

void destroyFramePacingState(
	Device _device,
	FramePacingState _framePacing)
{
	vkDestroySemaphore(_device.deviceVk, _framePacing.renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(_device.deviceVk, _framePacing.imageAvailableSemaphore, nullptr);
	vkDestroyFence(_device.deviceVk, _framePacing.inFlightFence, nullptr);
}
