#include "device.h"
#include "texture.h"
#include "frame_pacing.h"
#include "swapchain.h"
#include "window.h"

static VkSurfaceFormatKHR chooseSwapchainSurfaceFormat(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	u32 surfaceFormatCount = 0u;
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, nullptr));

	assert(surfaceFormatCount > 0u);

	std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(surfaceFormatCount);
	VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &surfaceFormatCount, availableSurfaceFormats.data()));

	for (VkSurfaceFormatKHR& rAvailableSurfaceFormat : availableSurfaceFormats)
	{
		if (rAvailableSurfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			return rAvailableSurfaceFormat;
		}
	}

	return availableSurfaceFormats[0];
}

static VkPresentModeKHR chooseSwapchainPresentMode(
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice,
	bool _bEnableVSync)
{
	if (!_bEnableVSync)
	{
		u32 presentModeCount = 0u;
		VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr));

		std::vector<VkPresentModeKHR> availablePresentModes(presentModeCount);
		VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, availablePresentModes.data()));

		for (VkPresentModeKHR& rAvailablePresentMode : availablePresentModes)
		{
			if (rAvailablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				return rAvailablePresentMode;
			}
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseSwapchainExtent(
	GLFWwindow* _pWindow,
	VkSurfaceKHR _surface,
	VkPhysicalDevice _physicalDevice)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	if (surfaceCapabilities.currentExtent.width != UINT32_MAX && surfaceCapabilities.currentExtent.height != UINT32_MAX)
	{
		return surfaceCapabilities.currentExtent;
	}

	i32 framebufferWidth, framebufferHeight;
	glfwGetFramebufferSize(_pWindow, &framebufferWidth, &framebufferHeight);

	VkExtent2D actualExtent = { u32(framebufferWidth), u32(framebufferHeight) };

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
	VkSwapchainKHR _oldSwapchain,
	u32 _preferredSwapchainImageCount)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

	u32 minImageCount = glm::clamp(_preferredSwapchainImageCount, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

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
	Device& _rDevice,
	SwapchainDesc _desc)
{
	VkSurfaceFormatKHR surfaceFormat = chooseSwapchainSurfaceFormat(_rDevice.surface, _rDevice.physicalDevice);
	VkPresentModeKHR presentMode = chooseSwapchainPresentMode(_rDevice.surface, _rDevice.physicalDevice, _desc.bEnableVSync);
	VkExtent2D extent = chooseSwapchainExtent(_pWindow, _rDevice.surface, _rDevice.physicalDevice);

	Swapchain swapchain = {
		.extent = extent,
		.format = surfaceFormat.format };

	swapchain.swapchain = createSwapchain(
		_rDevice.surface,
		_rDevice.physicalDevice,
		_rDevice.device,
		surfaceFormat,
		presentMode,
		extent,
		_desc.oldSwapchain,
		_desc.preferredSwapchainImageCount);

	u32 imageCount;
	vkGetSwapchainImagesKHR(_rDevice.device, swapchain.swapchain, &imageCount, nullptr);
	std::vector<VkImage> swapchainImages(imageCount);
	vkGetSwapchainImagesKHR(_rDevice.device, swapchain.swapchain, &imageCount, swapchainImages.data());

	swapchain.textures.reserve(imageCount);
	for (size_t imageIndex = 0; imageIndex < imageCount; ++imageIndex)
	{
		swapchain.textures.push_back(createTexture(_rDevice, {
			.width = extent.width,
			.height = extent.height,
			.format = surfaceFormat.format,
			.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.access = VK_ACCESS_NONE,
			.resource = swapchainImages[imageIndex] }));
	}

	return swapchain;
}

void destroySwapchain(
	Device& _rDevice,
	Swapchain& _rSwapchain)
{
	for (Texture& rTexture : _rSwapchain.textures)
	{
		destroyTexture(_rDevice, rTexture);
	}

	_rSwapchain.textures.clear();

	vkDestroySwapchainKHR(_rDevice.device, _rSwapchain.swapchain, nullptr);

	_rSwapchain = {};
}

void submitAndPresent(
	VkCommandBuffer _commandBuffer,
	Device& _rDevice,
	Swapchain _swapchain,
	u32 _imageIndex,
	FramePacingState _framePacingState)
{
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1u;
	submitInfo.pWaitSemaphores = &_framePacingState.imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1u;
	submitInfo.pCommandBuffers = &_commandBuffer;
	submitInfo.signalSemaphoreCount = 1u;
	submitInfo.pSignalSemaphores = &_framePacingState.renderFinishedSemaphore;

	VK_CALL(vkQueueSubmit(_rDevice.graphicsQueue.queue, 1, &submitInfo, _framePacingState.inFlightFence));

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1u;
	presentInfo.pWaitSemaphores = &_framePacingState.renderFinishedSemaphore;
	presentInfo.swapchainCount = 1u;
	presentInfo.pSwapchains = &_swapchain.swapchain;
	presentInfo.pImageIndices = &_imageIndex;

	VK_CALL(vkQueuePresentKHR(_rDevice.graphicsQueue.queue, &presentInfo));
}
