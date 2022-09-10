#include "common.h"
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "resources.h"

const uint32_t kPreferredSwapchainImageCount = 2;
const bool kbEnableVSync = true;

static VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	uint32_t surfaceFormatCount = 0;
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, nullptr));

	assert(surfaceFormatCount > 0);

	std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, availableSurfaceFormats.data()));

	for (const VkSurfaceFormatKHR& availableSurfaceFormat : availableSurfaceFormats)
	{
		if (availableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			return availableSurfaceFormat;
		}
	}

	return availableSurfaceFormats[0];
}

static VkPresentModeKHR chooseSwapchainPresentMode(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	uint32_t presentModeCount = 0;
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr));

	std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
	VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, availablePresentModes.data()));

	for (const VkPresentModeKHR& availablePresentMode : availablePresentModes)
	{
		if (kbEnableVSync)
		{
			// TODO-MILKRU: Which one should be used for VSync, VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_MAILBOX_KHR?
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}
		else
		{
			if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				return availablePresentMode;
			}
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapchainExtent(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	GLFWwindow* _pWindow)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	if (surfaceCapabilities.currentExtent.width != UINT32_MAX && surfaceCapabilities.currentExtent.height != UINT32_MAX)
	{
		return surfaceCapabilities.currentExtent;
	}

	int32_t windowWidth, windowHeight;
	glfwGetFramebufferSize(_pWindow, &windowWidth, &windowHeight);

	VkExtent2D actualExtent = { uint32_t(windowWidth), uint32_t(windowHeight) };

	VkExtent2D minImageExtent = surfaceCapabilities.minImageExtent;
	VkExtent2D maxImageExtent = surfaceCapabilities.maxImageExtent;

	actualExtent.width = glm::clamp(actualExtent.width, minImageExtent.width, maxImageExtent.width);
	actualExtent.height = glm::clamp(actualExtent.height, minImageExtent.height, maxImageExtent.height);

	return actualExtent;
}

static VkSwapchainKHR createSwapchain(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkSurfaceFormatKHR _surfaceFormat,
	VkPresentModeKHR _presentMode,
	VkExtent2D _extent,
	VkSwapchainKHR _oldSwapchain)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	uint32_t minImageCount = glm::clamp(kPreferredSwapchainImageCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchainCreateInfo.surface = _surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = _surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = _surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = _extent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = _presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = _oldSwapchain;

	VkSwapchainKHR swapchain;
	VK_CALL(vkCreateSwapchainKHR(_device, &swapchainCreateInfo, nullptr, &swapchain));

	return swapchain;
}

Swapchain createSwapchain(
	GLFWwindow* _pWindow,
	Device _device,
	VkSwapchainKHR _oldSwapchain)
{
	VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(_device.surface, _device.physicalDevice);
	VkPresentModeKHR presentMode = chooseSwapchainPresentMode(_device.surface, _device.physicalDevice);
	VkExtent2D extent = chooseSwapchainExtent(_device.surface, _device.physicalDevice, _pWindow);

	Swapchain swapchain{};
	swapchain.imageFormat = surfaceFormat.format;
	swapchain.extent = extent;

	swapchain.swapchainVk = createSwapchain(
		_device.surface,
		_device.physicalDevice,
		_device.deviceVk,
		surfaceFormat,
		presentMode,
		extent,
		_oldSwapchain);

	uint32_t imageCount;
	vkGetSwapchainImagesKHR(_device.deviceVk, swapchain.swapchainVk, &imageCount, nullptr);
	swapchain.images.resize(imageCount);
	vkGetSwapchainImagesKHR(_device.deviceVk, swapchain.swapchainVk, &imageCount, swapchain.images.data());

	swapchain.imageViews.resize(swapchain.images.size());
	for (size_t imageIndex = 0; imageIndex < swapchain.imageViews.size(); ++imageIndex)
	{
		swapchain.imageViews[imageIndex] = createImageView(_device.deviceVk, swapchain.images[imageIndex], swapchain.imageFormat);
	}

	return swapchain;
}

void destroySwapchain(
	Device _device,
	Swapchain& _rSwapchain)
{
	for (VkImageView imageView : _rSwapchain.imageViews)
	{
		vkDestroyImageView(_device.deviceVk, imageView, nullptr);
	}

	vkDestroySwapchainKHR(_device.deviceVk, _rSwapchain.swapchainVk, nullptr);

	_rSwapchain = {};
}
