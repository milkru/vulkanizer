#pragma once

struct Swapchain
{
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkExtent2D extent{};
	VkFormat format = VK_FORMAT_UNDEFINED;
	std::vector<Texture> textures{};
};

struct SwapchainDesc
{
	bool bEnableVSync = true;                      // Enable vertical sync.
	u32 preferredSwapchainImageCount = 2;          // Preferred number of swapchain images.
	VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE;  // [Optional] Old swapchain which can be used for faster creation.
};

Swapchain createSwapchain(
	GLFWwindow* _pWindow,
	Device& _rDevice,
	SwapchainDesc _desc);

void destroySwapchain(
	Device& _rDevice,
	Swapchain& _rSwapchain);

void submitAndPresent(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Swapchain _swapchain,
	u32 _imageIndex,
	FramePacingState _framePacingState);
