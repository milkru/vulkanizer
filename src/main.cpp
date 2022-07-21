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

static VkRenderPass createRenderPass(
	VkDevice _device,
	VkFormat _colorFormat,
	VkFormat _depthFormat)
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

	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = _depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentReference;
	subpass.pDepthStencilAttachment = &depthAttachmentReference;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassCreateInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkRenderPass renderPass;
	VK_CALL(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &renderPass));

	return renderPass;
}

static VkPipeline createGraphicsPipeline(
	VkDevice _device,
	VkRenderPass _renderPass,
	VkPipelineLayout _pipelineLayout,
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

	// TODO-MILKRU: Implement Vulkan 1.3 dynamic rendering.

	VkRenderPass renderPass = createRenderPass(device.deviceVk, swapchain.imageFormat, depthImage.format);

	std::vector<VkFramebuffer> framebuffers(swapchain.imageViews.size());
	for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
	{
		framebuffers[frameBufferIndex] = createFramebuffer(device.deviceVk, renderPass, swapchain.extent,
			{ swapchain.imageViews[frameBufferIndex], depthImage.view });
	}

	Shader taskShader = device.bMeshShadingPipelineSupported ? createShader(device, "shaders/scene.task.spv") : Shader();
	Shader meshShader = device.bMeshShadingPipelineSupported ? createShader(device, "shaders/scene.mesh.spv") : Shader();

	Shader vertShader = createShader(device, "shaders/scene.vert.spv");
	Shader fragShader = createShader(device, "shaders/scene.frag.spv");

	struct
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	} cameraMatrices;

	// TODO-MILKRU: Implement descriptorTemplates.
	// Unify templates, descriptorSets and pipelineLayouts.
	// Implement better resource binding system.

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(cameraMatrices);

	VkDescriptorSetLayout descriptorSetLayout =
		createDescriptorSetLayout(device.deviceVk, { vertShader, fragShader });

	VkPipelineLayout pipelineLayout =
		createPipelineLayout(device.deviceVk, { descriptorSetLayout }, { pushConstantRange });

	VkPipeline graphicsPipeline =
		createGraphicsPipeline(device.deviceVk, renderPass, pipelineLayout, { vertShader, fragShader });

	VkPushConstantRange pushConstantRangeNV{};
	pushConstantRangeNV.stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
	pushConstantRangeNV.offset = 0;
	pushConstantRangeNV.size = sizeof(cameraMatrices);

	VkDescriptorSetLayout descriptorSetLayoutNV = device.bMeshShadingPipelineSupported ?
		createDescriptorSetLayout(device.deviceVk, { taskShader, meshShader, fragShader }) : VK_NULL_HANDLE;

	VkPipelineLayout pipelineLayoutNV = device.bMeshShadingPipelineSupported ?
		createPipelineLayout(device.deviceVk, { descriptorSetLayoutNV }, { pushConstantRangeNV }) : VK_NULL_HANDLE;

	VkPipeline graphicsPipelineNV = device.bMeshShadingPipelineSupported ?
		createGraphicsPipeline(device.deviceVk, renderPass, pipelineLayoutNV, { taskShader, meshShader, fragShader }) : VK_NULL_HANDLE;

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
		sizeof(meshopt_Meshlet) * mesh.meshlets.size(), mesh.meshlets.data()) : Buffer();

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

	initializeGUI(device, renderPass, (float)kWindowWidth, (float)kWindowHeight);

	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(device.physicalDevice, &physicalDeviceProperties);

	InfoGUI infoGUI{};
	infoGUI.deviceName = physicalDeviceProperties.deviceName;

	bool bMeshShadingPipelineEnabled =
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

			for (VkFramebuffer framebuffer : framebuffers)
			{
				vkDestroyFramebuffer(device.deviceVk, framebuffer, nullptr);
			}

			destroyImage(device, depthImage);

			Swapchain newSwapchain = createSwapchain(pWindow, device, swapchain.swapchainVk);
			destroySwapchain(device, swapchain);
			swapchain = newSwapchain;

			depthImage = createImage(device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				swapchain.extent.width, swapchain.extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

			framebuffers.resize(swapchain.imageViews.size());
			for (size_t frameBufferIndex = 0; frameBufferIndex < framebuffers.size(); ++frameBufferIndex)
			{
				framebuffers[frameBufferIndex] = createFramebuffer(device.deviceVk, renderPass, swapchain.extent,
					{ swapchain.imageViews[frameBufferIndex], depthImage.view });
			}

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

			VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffers[imageIndex];
			renderPassBeginInfo.renderArea.offset = { 0, 0 };
			renderPassBeginInfo.renderArea.extent = swapchain.extent;

			std::array<VkClearValue, 2> clearValues{};
			clearValues[0].color = { { 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f } };
			clearValues[1].depthStencil = { 0.0f, 0 };

			renderPassBeginInfo.clearValueCount = uint32_t(clearValues.size());
			renderPassBeginInfo.pClearValues = clearValues.data();

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

				{
					static auto startTime = std::chrono::high_resolution_clock::now();

					auto currentTime = std::chrono::high_resolution_clock::now();
					float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

					cameraMatrices.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
					cameraMatrices.view = glm::lookAt(glm::vec3(1.5f, 0.5f, 1.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
					cameraMatrices.proj = getInfinitePerspectiveMatrix(glm::radians(45.0f), float(swapchain.extent.width) / float(swapchain.extent.height), 0.1f);
				}

				vkCmdPushConstants(commandBuffer,
					bMeshShadingPipelineEnabled ? pipelineLayoutNV : pipelineLayout,
					bMeshShadingPipelineEnabled ? VK_SHADER_STAGE_MESH_BIT_NV : VK_SHADER_STAGE_VERTEX_BIT,
					0, sizeof(cameraMatrices), &cameraMatrices);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					bMeshShadingPipelineEnabled ? graphicsPipelineNV : graphicsPipeline);

				VkDescriptorBufferInfo vertexBufferDescriptorInfo{};
				vertexBufferDescriptorInfo.buffer = vertexBuffer.bufferVk;
				vertexBufferDescriptorInfo.offset = 0;
				vertexBufferDescriptorInfo.range = vertexBuffer.size;

				VkDescriptorBufferInfo meshletBufferDescriptorInfo{};
				meshletBufferDescriptorInfo.buffer = meshletBuffer.bufferVk;
				meshletBufferDescriptorInfo.offset = 0;
				meshletBufferDescriptorInfo.range = meshletBuffer.size;

				VkDescriptorBufferInfo meshletVerticesBufferDescriptorInfo{};
				meshletVerticesBufferDescriptorInfo.buffer = meshletVerticesBuffer.bufferVk;
				meshletVerticesBufferDescriptorInfo.offset = 0;
				meshletVerticesBufferDescriptorInfo.range = meshletVerticesBuffer.size;

				VkDescriptorBufferInfo meshletTrianglesBufferDescriptorInfo{};
				meshletTrianglesBufferDescriptorInfo.buffer = meshletTrianglesBuffer.bufferVk;
				meshletTrianglesBufferDescriptorInfo.offset = 0;
				meshletTrianglesBufferDescriptorInfo.range = meshletTrianglesBuffer.size;

				std::vector<VkWriteDescriptorSet> writeDescriptorSets;
				writeDescriptorSets.reserve(4);

				VkWriteDescriptorSet vertexBufferWriteDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				vertexBufferWriteDescriptorSet.dstBinding = 0;
				vertexBufferWriteDescriptorSet.descriptorCount = 1;
				vertexBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				vertexBufferWriteDescriptorSet.pBufferInfo = &vertexBufferDescriptorInfo;
				writeDescriptorSets.push_back(vertexBufferWriteDescriptorSet);

				if (bMeshShadingPipelineEnabled)
				{
					VkWriteDescriptorSet meshletBufferWriteDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					meshletBufferWriteDescriptorSet.dstBinding = 1;
					meshletBufferWriteDescriptorSet.descriptorCount = 1;
					meshletBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					meshletBufferWriteDescriptorSet.pBufferInfo = &meshletBufferDescriptorInfo;
					writeDescriptorSets.push_back(meshletBufferWriteDescriptorSet);

					VkWriteDescriptorSet meshletVerticesBufferWriteDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					meshletVerticesBufferWriteDescriptorSet.dstBinding = 2;
					meshletVerticesBufferWriteDescriptorSet.descriptorCount = 1;
					meshletVerticesBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					meshletVerticesBufferWriteDescriptorSet.pBufferInfo = &meshletVerticesBufferDescriptorInfo;
					writeDescriptorSets.push_back(meshletVerticesBufferWriteDescriptorSet);

					VkWriteDescriptorSet meshletTrianglesBufferWriteDescriptorSet = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
					meshletTrianglesBufferWriteDescriptorSet.dstBinding = 3;
					meshletTrianglesBufferWriteDescriptorSet.descriptorCount = 1;
					meshletTrianglesBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					meshletTrianglesBufferWriteDescriptorSet.pBufferInfo = &meshletTrianglesBufferDescriptorInfo;
					writeDescriptorSets.push_back(meshletTrianglesBufferWriteDescriptorSet);
				}

				vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
					bMeshShadingPipelineEnabled ? pipelineLayoutNV : pipelineLayout,
					0, writeDescriptorSets.size(), writeDescriptorSets.data());

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

				vkCmdEndRenderPass(commandBuffer);
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
			vkDestroyDescriptorSetLayout(device.deviceVk, descriptorSetLayoutNV, nullptr);
			vkDestroyPipelineLayout(device.deviceVk, pipelineLayoutNV, nullptr);
		}

		vkDestroyPipeline(device.deviceVk, graphicsPipeline, nullptr);
		vkDestroyDescriptorSetLayout(device.deviceVk, descriptorSetLayout, nullptr);
		vkDestroyPipelineLayout(device.deviceVk, pipelineLayout, nullptr);

		for (VkFramebuffer framebuffer : framebuffers)
		{
			vkDestroyFramebuffer(device.deviceVk, framebuffer, nullptr);
		}

		vkDestroyRenderPass(device.deviceVk, renderPass, nullptr);

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
