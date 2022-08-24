#include "common.h"
#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "resources.h"
#include "sync.h"
#include "mesh.h"
#include "gui.h"
#include "utils.h"
#include "query.h"
#include "shaders/constants.h"

#include <easy/profiler.h>

#include <string.h>
#include <chrono>

const uint32_t kWindowWidth = 1280;
const uint32_t kWindowHeight = 720;

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

static VkPipeline createGraphicsPipeline(
	VkDevice _device,
	VkPipelineLayout _pipelineLayout,
	std::initializer_list<VkFormat> _colorFormats,
	VkFormat _depthFormat,
	std::initializer_list<Shader> _shaders)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
	shaderStageCreateInfos.reserve(_shaders.size());

	for (const Shader& shader : _shaders)
	{
		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		shaderStageCreateInfo.stage = shader.stage;
		shaderStageCreateInfo.module = shader.module;
		shaderStageCreateInfo.pName = shader.entry.c_str();

		shaderStageCreateInfos.push_back(shaderStageCreateInfo);
	}

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
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

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

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
	pipelineRenderingCreateInfo.colorAttachmentCount = uint32_t(_colorFormats.size());
	pipelineRenderingCreateInfo.pColorAttachmentFormats = _colorFormats.begin();
	pipelineRenderingCreateInfo.depthAttachmentFormat = _depthFormat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = uint32_t(shaderStageCreateInfos.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = _pipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline graphicsPipeline;
	VK_CALL(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline));

	return graphicsPipeline;
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

	Device device = createDevice(pWindow);
	Swapchain swapchain = createSwapchain(pWindow, device);

	Image depthImage = createImage(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		swapchain.extent.width, swapchain.extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	Shader taskShader = device.bMeshShadingPipelineSupported ? createShader(device, "shaders/scene.task.spv") : Shader();
	Shader meshShader = device.bMeshShadingPipelineSupported ? createShader(device, "shaders/scene.mesh.spv") : Shader();

	Shader vertShader = createShader(device, "shaders/scene.vert.spv");
	Shader fragShader = createShader(device, "shaders/scene.frag.spv");

	struct
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
		glm::vec3 cameraPosition;
		uint32_t enableConeCulling;
	} globals;

	// TODO-MILKRU: Unify templates, descriptorSets and pipelineLayouts.

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(globals);

	VkDescriptorSetLayout descriptorSetLayout =
		createDescriptorSetLayout(device.deviceVk, { vertShader, fragShader });

	VkPipelineLayout pipelineLayout =
		createPipelineLayout(device.deviceVk, { descriptorSetLayout }, { pushConstantRange });

	VkDescriptorUpdateTemplate descriptorUpdateTemplate =
		createDescriptorUpdateTemplate(device.deviceVk, descriptorSetLayout, pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, { vertShader, fragShader });

	VkPipeline graphicsPipeline =
		createGraphicsPipeline(device.deviceVk, pipelineLayout, { swapchain.imageFormat }, depthImage.format, { vertShader, fragShader });

	// TODO-MILKRU: Infer shader stages using SpirV.
	VkPushConstantRange pushConstantRangeNV{};
	pushConstantRangeNV.stageFlags = VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV;
	pushConstantRangeNV.offset = 0;
	pushConstantRangeNV.size = sizeof(globals);

	VkDescriptorSetLayout descriptorSetLayoutNV = device.bMeshShadingPipelineSupported ?
		createDescriptorSetLayout(device.deviceVk, { taskShader, meshShader, fragShader }) : VK_NULL_HANDLE;

	VkPipelineLayout pipelineLayoutNV = device.bMeshShadingPipelineSupported ?
		createPipelineLayout(device.deviceVk, { descriptorSetLayoutNV }, { pushConstantRangeNV }) : VK_NULL_HANDLE;

	VkDescriptorUpdateTemplate descriptorUpdateTemplateNV = device.bMeshShadingPipelineSupported ?
		createDescriptorUpdateTemplate(device.deviceVk, descriptorSetLayoutNV, pipelineLayoutNV,
			VK_PIPELINE_BIND_POINT_GRAPHICS, { taskShader, meshShader, fragShader }) : VK_NULL_HANDLE;

	VkPipeline graphicsPipelineNV = device.bMeshShadingPipelineSupported ?
		createGraphicsPipeline(device.deviceVk, pipelineLayoutNV, { swapchain.imageFormat }, depthImage.format,
			{ taskShader, meshShader, fragShader }) : VK_NULL_HANDLE;

	destroyShader(device, fragShader);
	destroyShader(device, vertShader);

	if (device.bMeshShadingPipelineSupported)
	{
		destroyShader(device, meshShader);
		destroyShader(device, taskShader);
	}

	Mesh mesh{};
	{
		EASY_BLOCK("LoadMesh");
		const char* meshPath = argv[1];
		mesh = loadMesh(meshPath, device.bMeshShadingPipelineSupported);
	}

	// TODO-MILKRU: Use one big buffer for sub allocation, or integrate VMA.

	Buffer meshletBuffer = device.bMeshShadingPipelineSupported ?
		createBuffer(device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(Meshlet) * mesh.meshlets.size(), mesh.meshlets.data()) : Buffer();

	Buffer meshletVerticesBuffer = device.bMeshShadingPipelineSupported ?
		createBuffer(device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(uint32_t) * mesh.meshletVertices.size(), mesh.meshletVertices.data()) : Buffer();
	
	Buffer meshletTrianglesBuffer = device.bMeshShadingPipelineSupported ?
		createBuffer(device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(uint8_t) * mesh.meshletTriangles.size(), mesh.meshletTriangles.data()) : Buffer();

	Buffer vertexBuffer = createBuffer(device, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(Vertex) * mesh.vertices.size(), mesh.vertices.data());

	Buffer indexBuffer = createBuffer(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		sizeof(uint32_t) * mesh.indices.size(), mesh.indices.data());

	std::array<VkCommandBuffer, kMaxFramesInFlightCount> commandBuffers;
	for (VkCommandBuffer& commandBuffer : commandBuffers)
	{
		commandBuffer = createCommandBuffer(device);
	}

	std::array<FramePacingState, kMaxFramesInFlightCount> framePacingStates;
	for (FramePacingState& framePacingState : framePacingStates)
	{
		framePacingState = createFramePacingState(device);
	}

	QueryPool timestampsQueryPool = createQueryPool(device, VK_QUERY_TYPE_TIMESTAMP, /*_queryCount*/ 2);
	QueryPool statisticsQueryPool = createQueryPool(device, VK_QUERY_TYPE_PIPELINE_STATISTICS, /*_queryCount*/ 1);

	uint32_t frameIndex = 0;

	initializeGUI(device, swapchain.imageFormat, depthImage.format, (float)kWindowWidth, (float)kWindowHeight);

	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(device.physicalDevice, &physicalDeviceProperties);

	InfoGUI infoGUI{};
	infoGUI.deviceName = physicalDeviceProperties.deviceName;

	bool bMeshShadingPipelineEnabled =
		infoGUI.bMeshletConeCulling =
		infoGUI.bMeshShadingPipelineEnabled =
		infoGUI.bMeshShadingPipelineSupported =
		device.bMeshShadingPipelineSupported;

	while (!glfwWindowShouldClose(pWindow))
	{
		EASY_BLOCK("Frame");

		glfwPollEvents();

		newFrameGUI(pWindow, infoGUI);

		bMeshShadingPipelineEnabled = infoGUI.bMeshShadingPipelineEnabled;

		VkCommandBuffer commandBuffer = commandBuffers[frameIndex];
		FramePacingState framePacingState = framePacingStates[frameIndex];

		{
			EASY_BLOCK("WaitForFences");
			VK_CALL(vkWaitForFences(device.deviceVk, 1, &framePacingState.inFlightFence, VK_TRUE, UINT64_MAX));
		}

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, device.surface, &surfaceCapabilities));

		VkExtent2D currentExtent = surfaceCapabilities.currentExtent;

		if (currentExtent.width == 0 || currentExtent.height == 0)
		{
			continue;
		}

		if (swapchain.extent.width != currentExtent.width || swapchain.extent.height != currentExtent.height)
		{
			EASY_BLOCK("RecreateSwapchain");

			VK_CALL(vkDeviceWaitIdle(device.deviceVk));

			destroyImage(device, depthImage);

			Swapchain newSwapchain = createSwapchain(pWindow, device, swapchain.swapchainVk);
			destroySwapchain(device, swapchain);
			swapchain = newSwapchain;

			depthImage = createImage(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				swapchain.extent.width, swapchain.extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

			continue;
		}

		VK_CALL(vkResetFences(device.deviceVk, 1, &framePacingState.inFlightFence));

		uint32_t imageIndex;
		VK_CALL(vkAcquireNextImageKHR(device.deviceVk, swapchain.swapchainVk, UINT64_MAX, framePacingState.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex));

		{
			EASY_BLOCK("Draw");

			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			resetQueryPool(commandBuffer, timestampsQueryPool);
			resetQueryPool(commandBuffer, statisticsQueryPool);

			VkRenderingAttachmentInfoKHR colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
			colorAttachment.imageView = swapchain.imageViews[imageIndex];
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue.color = { { 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f } };

			VkRenderingAttachmentInfoKHR depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR };
			depthAttachment.imageView = depthImage.view;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.clearValue.depthStencil = { 0.0f, 0 };

			VkRenderingInfoKHR renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO_KHR };
			renderingInfo.renderArea.offset = { 0, 0 };
			renderingInfo.renderArea.extent = swapchain.extent;
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = &depthAttachment;

			{
				GPU_BLOCK(commandBuffer, timestampsQueryPool, Main);
				GPU_STATS(commandBuffer, statisticsQueryPool, Main);

				transferImageLayout(commandBuffer, swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

				vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);

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

				{
					static auto startTime = std::chrono::high_resolution_clock::now();

					auto currentTime = std::chrono::high_resolution_clock::now();
					float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

					globals.cameraPosition = glm::vec3(2.0f, 0.0f, 0.0f);
					globals.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
					globals.view = glm::lookAt(globals.cameraPosition, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
					globals.proj = getInfinitePerspectiveMatrix(glm::radians(45.0f), float(swapchain.extent.width) / float(swapchain.extent.height), 0.1f);
					globals.enableConeCulling = infoGUI.bMeshletConeCulling ? 1 : 0;
				}

				vkCmdPushConstants(commandBuffer,
					bMeshShadingPipelineEnabled ? pipelineLayoutNV : pipelineLayout,
					bMeshShadingPipelineEnabled ? VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV : VK_SHADER_STAGE_VERTEX_BIT,
					0, sizeof(globals), &globals);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					bMeshShadingPipelineEnabled ? graphicsPipelineNV : graphicsPipeline);

				std::vector<DescriptorInfo> descriptorInfos;
				descriptorInfos.reserve(4);

				if (bMeshShadingPipelineEnabled)
				{
					descriptorInfos.push_back(meshletBuffer.bufferVk);
					descriptorInfos.push_back(meshletVerticesBuffer.bufferVk);
					descriptorInfos.push_back(meshletTrianglesBuffer.bufferVk);
				}

				descriptorInfos.push_back(vertexBuffer.bufferVk);

				vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer,
					bMeshShadingPipelineEnabled ? descriptorUpdateTemplateNV : descriptorUpdateTemplate,
					bMeshShadingPipelineEnabled ? pipelineLayoutNV : pipelineLayout, 0, descriptorInfos.data());

				if (!bMeshShadingPipelineEnabled)
				{
					vkCmdBindIndexBuffer(commandBuffer, indexBuffer.bufferVk, 0, VK_INDEX_TYPE_UINT32);
				}

				for (uint32_t drawIndex = 0; drawIndex < 1; ++drawIndex)
				{
					if (bMeshShadingPipelineEnabled)
					{
						uint32_t taskCount = (uint32_t(mesh.meshlets.size()) + kShaderGroupSizeNV - 1) / kShaderGroupSizeNV;
						vkCmdDrawMeshTasksNV(commandBuffer, taskCount, 0);
					}
					else
					{
						vkCmdDrawIndexed(commandBuffer, uint32_t(mesh.indices.size()), 1, 0, 0, 0);
					}
				}

				drawFrameGUI(commandBuffer, frameIndex);

				vkCmdEndRenderingKHR(commandBuffer);

				transferImageLayout(commandBuffer, swapchain.images[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
			}

			VK_CALL(vkEndCommandBuffer(commandBuffer));
		}

		VkSemaphore waitSemaphores[] = { framePacingState.imageAvailableSemaphore };
		VkSemaphore signalSemaphores[] = { framePacingState.renderFinishedSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = ARRAY_SIZE(waitSemaphores);
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.signalSemaphoreCount = ARRAY_SIZE(signalSemaphores);
		submitInfo.pSignalSemaphores = signalSemaphores;

		VK_CALL(vkQueueSubmit(device.graphicsQueue.queueVk, 1, &submitInfo, framePacingState.inFlightFence));

		VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapchain.swapchainVk;
		presentInfo.pImageIndices = &imageIndex;

		VK_CALL(vkQueuePresentKHR(device.graphicsQueue.queueVk, &presentInfo));

		updateQueryPoolResults(device, timestampsQueryPool);
		updateQueryPoolResults(device, statisticsQueryPool);

		GPU_BLOCK_RESULT(timestampsQueryPool, Main, physicalDeviceProperties.limits, infoGUI.gpuTime);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyVertices, infoGUI.inputAssemblyVertices);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyPrimitives, infoGUI.inputAssemblyPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::VertexShaderInvocations, infoGUI.vertexShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingInvocations, infoGUI.clippingInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingPrimitives, infoGUI.clippingPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::FragmentShaderInvocations, infoGUI.fragmentShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ComputeShaderInvocations, infoGUI.computeShaderInvocations);

		frameIndex = (frameIndex + 1) % kMaxFramesInFlightCount;
	}

	{
		EASY_BLOCK("DeviceWaitIdle");
		VK_CALL(vkDeviceWaitIdle(device.deviceVk));
	}

	{
		EASY_BLOCK("Cleanup");

		terminateGUI();

		destroyQueryPool(device, timestampsQueryPool);
		destroyQueryPool(device, statisticsQueryPool);

		for (FramePacingState& framePacingState : framePacingStates)
		{
			destroyFramePacingState(device, framePacingState);
		}

		if (device.bMeshShadingPipelineSupported)
		{
			destroyBuffer(device, meshletBuffer);
			destroyBuffer(device, meshletVerticesBuffer);
			destroyBuffer(device, meshletTrianglesBuffer);
		}

		destroyBuffer(device, vertexBuffer);
		destroyBuffer(device, indexBuffer);

		if (device.bMeshShadingPipelineSupported)
		{
			vkDestroyPipeline(device.deviceVk, graphicsPipelineNV, nullptr);
			vkDestroyDescriptorUpdateTemplate(device.deviceVk, descriptorUpdateTemplateNV, nullptr);
			vkDestroyPipelineLayout(device.deviceVk, pipelineLayoutNV, nullptr);
			vkDestroyDescriptorSetLayout(device.deviceVk, descriptorSetLayoutNV, nullptr);
		}

		vkDestroyPipeline(device.deviceVk, graphicsPipeline, nullptr);
		vkDestroyDescriptorUpdateTemplate(device.deviceVk, descriptorUpdateTemplate, nullptr);
		vkDestroyPipelineLayout(device.deviceVk, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device.deviceVk, descriptorSetLayout, nullptr);

		destroyImage(device, depthImage);

		destroySwapchain(device, swapchain);

		destroyDevice(device);

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
