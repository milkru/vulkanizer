#pragma once

const u32 kMaxFramesInFlightCount = 2u;

struct FramePacingState
{
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

FramePacingState createFramePacingState(
	Device& _rDevice);

void destroyFramePacingState(
	Device& _rDevice,
	FramePacingState& _rFramePacingState);
