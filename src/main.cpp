#include "common.h"
#include "device.h"
#include "swapchain.h"
#include "resources.h"
#include "pipeline.h"
#include "query.h"
#include "mesh.h"
#include "utils.h"
#include "gui.h"

#include <easy/profiler.h>

#include <string.h>
#include <chrono>

const uint32_t kWindowWidth = 1280;
const uint32_t kWindowHeight = 720;

struct FramePacing
{
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
	VkFence inFlightFence = VK_NULL_HANDLE;
};

static GLFWwindow* createWindow()
{
	glfwSetErrorCallback(
		[](int32_t _error, const char* _description)
		{
			fprintf(stderr, "GLFW error: %s\n", _description);
		}
	);

	if (!glfwInit())
	{
		assert(!"GLFW not initialized properly!");
	}

	if (!glfwVulkanSupported())
	{
		assert(!"Vulkan is not supported!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* pWindow = glfwCreateWindow(kWindowWidth, kWindowHeight, "vulkanizer", nullptr, nullptr);
	if (pWindow == nullptr)
	{
		glfwTerminate();
		assert(!"Window creation failed!");
	}

	return pWindow;
}

static VkRenderPass createRenderPass(
	VkDevice _device,
	VkFormat _colorFormat)
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = _colorFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkRenderPass renderPass;
	VK_CALL(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &renderPass));

	return renderPass;
}

static VkDescriptorSetLayout createDescriptorSetLayout(
	VkDevice _device)
{
	// TODO-MILKRU: Get these bindings directly from shader using the spirv_reflect and reuse the code in the ImGui code.
	VkDescriptorSetLayoutBinding bindings[1]{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	descriptorSetLayoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);
	descriptorSetLayoutCreateInfo.pBindings = bindings;

	VkDescriptorSetLayout descriptorSetLayout;
	VK_CALL(vkCreateDescriptorSetLayout(_device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

	return descriptorSetLayout;
}

static VkPipeline createGraphicsPipeline(
	VkDevice _device,
	VkRenderPass _renderPass,
	VkPipelineLayout _pipelineLayout,
	VkShaderModule _vertexShader,
	VkShaderModule _fragmentShader)
{
	VkPipelineShaderStageCreateInfo vertexShaderStageCreatenfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertexShaderStageCreatenfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreatenfo.module = _vertexShader;
	vertexShaderStageCreatenfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = _fragmentShader;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] =
	{
		vertexShaderStageCreatenfo,
		fragmentShaderStageCreateInfo
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	VkDynamicState dynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStates);
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = ARRAY_SIZE(shaderStageCreateInfos);
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = _pipelineLayout;
	pipelineCreateInfo.renderPass = _renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline graphicsPipeline;
	VK_CALL(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline));

	return graphicsPipeline;
}

static VkFramebuffer createFramebuffer(
	VkDevice _device,
	VkRenderPass _renderPass,
	VkExtent2D _extent,
	const std::vector<VkImageView>& _rAttachments)
{
	VkFramebufferCreateInfo framebufferICreatenfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	framebufferICreatenfo.renderPass = _renderPass;
	framebufferICreatenfo.attachmentCount = _rAttachments.size();
	framebufferICreatenfo.pAttachments = _rAttachments.data();
	framebufferICreatenfo.width = _extent.width;
	framebufferICreatenfo.height = _extent.height;
	framebufferICreatenfo.layers = 1;

	VkFramebuffer framebuffer;
	VK_CALL(vkCreateFramebuffer(_device, &framebufferICreatenfo, nullptr, &framebuffer));

	return framebuffer;
}

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

static FramePacing createFramePacing(
	VkDevice _device)
{
	FramePacing framePacing{};
	framePacing.imageAvailableSemaphore = createSemaphore(_device);
	framePacing.renderFinishedSemaphore = createSemaphore(_device);
	framePacing.inFlightFence = createFence(_device);
	return framePacing;
}

static void destroyFramePacing(
	VkDevice _device,
	FramePacing _framePacing)
{
	vkDestroySemaphore(_device, _framePacing.renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(_device, _framePacing.imageAvailableSemaphore, nullptr);
	vkDestroyFence(_device, _framePacing.inFlightFence, nullptr);
}

int32_t main(int32_t argc, const char** argv)
{
	EASY_MAIN_THREAD;
	EASY_PROFILER_ENABLE;

	if (argc != 2)
	{
		printf("Mesh path is required as a command line argument.\n");
		return 1;
	}

	GLFWwindow* pWindow = createWindow();

	VK_CALL(volkInitialize());

	VkInstance instance = createInstance();

	VkDebugUtilsMessengerEXT debugMessenger = createDebugMessenger(instance);

	VkSurfaceKHR surface = createSurface(instance, pWindow);

	VkPhysicalDevice physicalDevice = tryPickPhysicalDevice(instance, surface);
	assert(physicalDevice != VK_NULL_HANDLE);

	uint32_t graphicsQueueIndex = tryGetGraphicsQueueFamilyIndex(physicalDevice);
	assert(graphicsQueueIndex != ~0u);

	VkDevice device = createDevice(physicalDevice, graphicsQueueIndex);

	VkQueue graphicsQueue;
	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &graphicsQueue);

	Swapchain swapchain = createSwapchain(pWindow, surface, physicalDevice, device);

	VkRenderPass renderPass = createRenderPass(device, swapchain.imageFormat);

	VkDescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout(device);

	VkPipelineLayout pipelineLayout = createPipelineLayout(device, { descriptorSetLayout }, {});

	VkPipeline graphicsPipeline;
	{
		std::vector<uint8_t> vertexShaderCode = readFile("shaders/scene.vert.spv");
		VkShaderModule vertexShader = createShaderModule(device, vertexShaderCode.size(), (uint32_t*)vertexShaderCode.data());

		std::vector<uint8_t> fragmentShaderCode = readFile("shaders/scene.frag.spv");
		VkShaderModule fragmentShader = createShaderModule(device, fragmentShaderCode.size(), (uint32_t*)fragmentShaderCode.data());

		graphicsPipeline = createGraphicsPipeline(device, renderPass, pipelineLayout, vertexShader, fragmentShader);

		vkDestroyShaderModule(device, fragmentShader, nullptr);
		vkDestroyShaderModule(device, vertexShader, nullptr);
	}

	std::vector<VkFramebuffer> framebuffers(swapchain.imageViews.size());
	for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
	{
		framebuffers[frameBufferIndex] = createFramebuffer(device, renderPass, swapchain.extent, { swapchain.imageViews[frameBufferIndex] });
	}

	VkCommandPool commandPool = createCommandPool(device, graphicsQueueIndex);

	Mesh mesh;
	{
		EASY_BLOCK("LoadMesh");
		const char* meshPath = argv[1];
		mesh = loadMesh(meshPath);
	}

	// NOTE-MILKRU: Use one big buffer for sub allocation.
	Buffer vertexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Vertex) * mesh.vertices.size(), mesh.vertices.data());

	Buffer indexBuffer = createBuffer(physicalDevice, device, graphicsQueue, commandPool,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data());

	VkCommandBuffer commandBuffers[kMaxFramesInFlightCount];
	FramePacing framePacings[kMaxFramesInFlightCount];

	for (uint32_t frameIndex = 0; frameIndex < kMaxFramesInFlightCount; ++frameIndex)
	{
		commandBuffers[frameIndex] = createCommandBuffer(device, commandPool);
		framePacings[frameIndex] = createFramePacing(device);
	}

	QueryPool timestampsQueryPool = createQueryPool(device, VK_QUERY_TYPE_TIMESTAMP, /*_queryCount*/ 2);
	QueryPool statisticsQueryPool = createQueryPool(device, VK_QUERY_TYPE_PIPELINE_STATISTICS, /*_queryCount*/ 1);

	uint32_t currentFrame = 0;

	initializeGUI(physicalDevice, device, graphicsQueue, commandPool, renderPass, (float)kWindowWidth, (float)kWindowHeight);

	InfoGUI infoGUI{};

	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	infoGUI.deviceName = physicalDeviceProperties.deviceName;

	while (!glfwWindowShouldClose(pWindow))
	{
		EASY_BLOCK("Frame");

		glfwPollEvents();

		newFrameGUI(pWindow, infoGUI);

		VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
		FramePacing framePacing = framePacings[currentFrame];

		{
			EASY_BLOCK("WaitForFences");
			VK_CALL(vkWaitForFences(device, 1, &framePacing.inFlightFence, VK_TRUE, UINT64_MAX));
		}

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

		VkExtent2D currentExtent = surfaceCapabilities.currentExtent;

		if (currentExtent.width == 0 || currentExtent.height == 0)
		{
			continue;
		}

		if (swapchain.extent.width != currentExtent.width || swapchain.extent.height != currentExtent.height)
		{
			EASY_BLOCK("RecreateSwapchain");

			VK_CALL(vkDeviceWaitIdle(device));

			for (VkFramebuffer framebuffer : framebuffers)
			{
				vkDestroyFramebuffer(device, framebuffer, nullptr);
			}

			Swapchain newSwapchain = createSwapchain(pWindow, surface, physicalDevice, device, swapchain.swapchainVk);
			destroySwapchain(device, swapchain);
			swapchain = newSwapchain;

			framebuffers.resize(swapchain.imageViews.size());
			for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
			{
				framebuffers[frameBufferIndex] = createFramebuffer(device, renderPass, swapchain.extent, { swapchain.imageViews[frameBufferIndex] });
			}

			continue;
		}

		VK_CALL(vkResetFences(device, 1, &framePacing.inFlightFence));

		uint32_t imageIndex;
		VK_CALL(vkAcquireNextImageKHR(device, swapchain.swapchainVk, UINT64_MAX, framePacing.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));

		{
			EASY_BLOCK("Draw");

			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			resetQueryPool(commandBuffer, timestampsQueryPool);
			resetQueryPool(commandBuffer, statisticsQueryPool);

			VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = swapchain.extent;

			VkClearValue clearColor = { {{ 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f }} };
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = &clearColor;

			{
				GPU_BLOCK(commandBuffer, timestampsQueryPool, Main);
				GPU_STATS(commandBuffer, statisticsQueryPool, Main);

				vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Flipped viewport.
				// https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
				VkViewport viewport{};
				viewport.x = 0.0f;
				viewport.y = float(swapchain.extent.height);
				viewport.width = float(swapchain.extent.width);
				viewport.height = -float(swapchain.extent.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissorRect{};
				scissorRect.offset = { 0, 0 };
				scissorRect.extent = swapchain.extent;

				vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				VkDescriptorBufferInfo vertexBufferDescriptorInfo{};
				vertexBufferDescriptorInfo.buffer = vertexBuffer.bufferVk;
				vertexBufferDescriptorInfo.offset = 0;
				vertexBufferDescriptorInfo.range = vertexBuffer.size;

				VkWriteDescriptorSet writeDescriptorSets[1]{};
				writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[0].dstBinding = 0;
				writeDescriptorSets[0].descriptorCount = 1;
				writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writeDescriptorSets[0].pBufferInfo = &vertexBufferDescriptorInfo;

				vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout, 0, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets);

				vkCmdBindIndexBuffer(commandBuffer, indexBuffer.bufferVk, 0, VK_INDEX_TYPE_UINT32);

				for (uint32_t drawIndex = 0; drawIndex < 4096; ++drawIndex)
				{
					vkCmdDrawIndexed(commandBuffer, uint32_t(mesh.indices.size()), 1, 0, 0, 0);
				}

				drawFrameGUI(commandBuffer, currentFrame);

				vkCmdEndRenderPass(commandBuffer);
			}

			VK_CALL(vkEndCommandBuffer(commandBuffer));
		}

		VkSemaphore waitSemaphores[] = { framePacing.imageAvailableSemaphore };
		VkSemaphore signalSemaphores[] = { framePacing.renderFinishedSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = ARRAY_SIZE(waitSemaphores);
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = ARRAY_SIZE(signalSemaphores);
		submitInfo.pSignalSemaphores = signalSemaphores;

		VK_CALL(vkQueueSubmit(graphicsQueue, 1, &submitInfo, framePacing.inFlightFence));

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain.swapchainVk;
		presentInfo.pImageIndices = &imageIndex;

		VK_CALL(vkQueuePresentKHR(graphicsQueue, &presentInfo));

		updateQueryPoolResults(device, timestampsQueryPool);
		updateQueryPoolResults(device, statisticsQueryPool);

		GPU_BLOCK_RESULT(timestampsQueryPool, physicalDeviceProperties.limits, Main, infoGUI.gpuTime);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyVertices, infoGUI.inputAssemblyVertices);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyPrimitives, infoGUI.inputAssemblyPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::VertexShaderInvocations, infoGUI.vertexShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingInvocations, infoGUI.clippingInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingPrimitives, infoGUI.clippingPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::FragmentShaderInvocations, infoGUI.fragmentShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ComputeShaderInvocations, infoGUI.computeShaderInvocations);

		currentFrame = (currentFrame + 1) % kMaxFramesInFlightCount;
	}

	{
		EASY_BLOCK("DeviceWaitIdle");
		VK_CALL(vkDeviceWaitIdle(device));
	}

	{
		EASY_BLOCK("Cleanup");

		terminateGUI();

		for (VkFramebuffer framebuffer : framebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		destroySwapchain(device, swapchain);

		destroyBuffer(device, vertexBuffer);
		destroyBuffer(device, indexBuffer);

		for (size_t frameIndex = 0; frameIndex < kMaxFramesInFlightCount; ++frameIndex)
		{
			destroyFramePacing(device, framePacings[frameIndex]);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		destroyQueryPool(device, timestampsQueryPool);
		destroyQueryPool(device, statisticsQueryPool);

		vkDestroyDevice(device, nullptr);

		if (debugMessenger != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(pWindow);

		glfwTerminate();
	}

	{
		const char* profileCaptureFileName = "cpu_profile_capture.prof";
		profiler::dumpBlocksToFile(profileCaptureFileName);
		printf("CPU profile capture saved to %s file.\n", profileCaptureFileName);
	}

	return 0;
}
