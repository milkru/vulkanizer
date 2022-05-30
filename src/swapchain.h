#pragma once

struct Swapchain
{
	VkSwapchainKHR swapchainVk = VK_NULL_HANDLE;
	VkFormat imageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D extent = {};
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
};

Swapchain createSwapchain(
	GLFWwindow* _pWindow,
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkSwapchainKHR _oldSwapchain = VK_NULL_HANDLE);

void destroySwapchain(
	VkDevice _device,
	Swapchain& _rSwapchain);
