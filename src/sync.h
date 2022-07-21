#pragma once

const uint32_t kMaxFramesInFlightCount = 2;

VkSemaphore createSemaphore(
	VkDevice _device);

VkFence createFence(
	VkDevice _device);

struct FramePacingState
{
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

FramePacingState createFramePacingState(
	Device _device);

void destroyFramePacingState(
	Device _device,
	FramePacingState _framePacing);
