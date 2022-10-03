#pragma once

struct SwapchainDesc
{
	bool bEnableVSync = true;
	uint32_t preferredSwapchainImageCount = 2;
	VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE;
};

struct Swapchain
{
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkExtent2D extent{};
	VkFormat format = VK_FORMAT_UNDEFINED;
	std::vector<Texture> textures{};
};

Swapchain createSwapchain(
	GLFWwindow* _pWindow,
	Device _device,
	SwapchainDesc _desc);

void destroySwapchain(
	Device _device,
	Swapchain& _rSwapchain);

void submitAndPresent(
	VkCommandBuffer _commandBuffer,
	Device _device,
	Swapchain _swapchain,
	uint32_t _imageIndex,
	FramePacingState _framePacingState);
