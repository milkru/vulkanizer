#include "common.h"
#include "gui.h"
#include "device.h"
#include "resources.h"
#include "pipeline.h"
#include "utils.h"

#include <stdio.h>
#include <array>
#include <algorithm>
#include <imgui.h>

struct GUI
{
	struct
	{
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstantBlock;

	VkPhysicalDevice physicalDevice;
	VkDevice device;
	Buffer vertexBuffers[kMaxFramesInFlightCount];
	Buffer indexBuffers[kMaxFramesInFlightCount];
	VkSampler fontSampler;
	VkDeviceMemory fontMemory;
	VkImage fontImage;
	VkImageView fontImageView ;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkDescriptorSetLayout descriptorSetLayout;
};

static GUI& getGUI()
{
	return *(GUI*)ImGui::GetIO().BackendRendererUserData;
}

static void createFontTexture(
	VkQueue _copyQueue,
	VkCommandPool _commandPool)
{
	ImGuiIO& io = ImGui::GetIO();
	GUI& gui = getGUI();

	unsigned char* fontData;
	int32_t textureWidth;
	int32_t textureHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &textureWidth, &textureHeight);

	VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.extent.width = textureWidth;
	imageCreateInfo.extent.height = textureHeight;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CALL(vkCreateImage(gui.device, &imageCreateInfo, nullptr, &gui.fontImage));

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(gui.device, gui.fontImage, &memoryRequirements);

	uint32_t memoryTypeIndex = tryFindMemoryType(gui.physicalDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	assert(memoryTypeIndex != -1);

	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CALL(vkAllocateMemory(gui.device, &memoryAllocateInfo, nullptr, &gui.fontMemory));
	VK_CALL(vkBindImageMemory(gui.device, gui.fontImage, gui.fontMemory, 0));

	VkImageViewCreateInfo imageViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewCreateInfo.image = gui.fontImage;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VK_CALL(vkCreateImageView(gui.device, &imageViewCreateInfo, nullptr, &gui.fontImageView));

	VkDeviceSize uploadSize = textureWidth * textureHeight * 4 * sizeof(char);

	Buffer stagingBuffer = createBuffer(gui.physicalDevice, gui.device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uploadSize);

	memcpy(stagingBuffer.data, fontData, uploadSize);

	immediateSubmit(gui.device, _copyQueue, _commandPool, [&](VkCommandBuffer _commandBuffer)
		{
			transferImageLayout(
				_commandBuffer,
				gui.fontImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_HOST_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkBufferImageCopy bufferCopyRegion{};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = textureWidth;
			bufferCopyRegion.imageExtent.height = textureHeight;
			bufferCopyRegion.imageExtent.depth = 1;

			vkCmdCopyBufferToImage(
				_commandBuffer,
				stagingBuffer.bufferVk,
				gui.fontImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&bufferCopyRegion
			);

			transferImageLayout(
				_commandBuffer,
				gui.fontImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		});

	destroyBuffer(gui.device, stagingBuffer);

	VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VK_CALL(vkCreateSampler(gui.device, &samplerCreateInfo, nullptr, &gui.fontSampler));
}

static void createPipeline(
	VkRenderPass _renderPass)
{
	GUI& gui = getGUI();

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(gui.pushConstantBlock);

	gui.pipelineLayout = createPipelineLayout(gui.device, { gui.descriptorSetLayout }, { pushConstantRange });

	std::vector<uint8_t> vertexShaderCode = readFile("shaders/gui.vert.spv");
	VkShaderModule vertexShader = createShaderModule(gui.device, vertexShaderCode.size(), (uint32_t*)vertexShaderCode.data());

	VkPipelineShaderStageCreateInfo vertexShaderStageCreatenfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	vertexShaderStageCreatenfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreatenfo.module = vertexShader;
	vertexShaderStageCreatenfo.pName = "main";

	std::vector<uint8_t> fragmentShaderCode = readFile("shaders/gui.frag.spv");
	VkShaderModule fragmentShader = createShaderModule(gui.device, fragmentShaderCode.size(), (uint32_t*)fragmentShaderCode.data());

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShader;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] =
	{
		vertexShaderStageCreatenfo,
		fragmentShaderStageCreateInfo
	};

	VkVertexInputBindingDescription vertexInputBindings[1]{};
	vertexInputBindings[0].binding = 0;
	vertexInputBindings[0].stride = sizeof(ImDrawVert);
	vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription positionAttribute{};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	positionAttribute.offset = offsetof(ImDrawVert, pos);

	VkVertexInputAttributeDescription texCoordAttribute{};
	texCoordAttribute.binding = 0;
	texCoordAttribute.location = 1;
	texCoordAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	texCoordAttribute.offset = offsetof(ImDrawVert, uv);

	VkVertexInputAttributeDescription colorAttribute{};
	colorAttribute.binding = 0;
	colorAttribute.location = 2;
	colorAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
	colorAttribute.offset = offsetof(ImDrawVert, col);

	VkVertexInputAttributeDescription vertexInputAttributes[] =
	{
		positionAttribute,
		texCoordAttribute,
		colorAttribute,
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = ARRAY_SIZE(vertexInputBindings);
	vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindings;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = ARRAY_SIZE(vertexInputAttributes);
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.blendEnable = VK_TRUE;
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &blendAttachmentState;

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
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = gui.pipelineLayout;
	pipelineCreateInfo.renderPass = _renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VK_CALL(vkCreateGraphicsPipelines(gui.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &gui.graphicsPipeline));

	vkDestroyShaderModule(gui.device, fragmentShader, nullptr);
	vkDestroyShaderModule(gui.device, vertexShader, nullptr);
}

void initializeGUI(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkQueue _copyQueue,
	VkCommandPool _commandPool,
	VkRenderPass _renderPass,
	float _windowWidth,
	float _windowHeight)
{
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererUserData = new GUI();
	io.DisplaySize = ImVec2(_windowWidth, _windowHeight);
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.FrameRounding = 5.0f;
	style.WindowRounding = 7.0f;
	style.WindowBorderSize = 2.0f;

	for (uint32_t colorIndex = 0; colorIndex < ImGuiCol_COUNT; ++colorIndex)
	{
		ImVec4& color = style.Colors[colorIndex];
		color.x = color.y = color.z = (0.2125f * color.x) + (0.7154f * color.y) + (0.0721f * color.z);
	}

	GUI& gui = getGUI();
	gui.physicalDevice = _physicalDevice;
	gui.device = _device;

	createFontTexture(_copyQueue, _commandPool);

	// TODO-MILKRU: Once SpirV reflect gets integrated, generate descriptor set layouts automatically.
	VkDescriptorSetLayoutBinding bindings[1]{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	descriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	descriptorSetLayoutCreateInfo.pBindings = bindings;
	descriptorSetLayoutCreateInfo.bindingCount = ARRAY_SIZE(bindings);

	VK_CALL(vkCreateDescriptorSetLayout(gui.device, &descriptorSetLayoutCreateInfo, nullptr, &gui.descriptorSetLayout));

	createPipeline(_renderPass);
}

void terminateGUI()
{
	GUI& gui = getGUI();

	for (uint32_t bufferIndex = 0; bufferIndex < ARRAY_SIZE(gui.vertexBuffers); ++bufferIndex)
	{
		destroyBuffer(gui.device, gui.vertexBuffers[bufferIndex]);
	}

	for (uint32_t bufferIndex = 0; bufferIndex < ARRAY_SIZE(gui.vertexBuffers); ++bufferIndex)
	{
		destroyBuffer(gui.device, gui.indexBuffers[bufferIndex]);
	}

	vkDestroyImage(gui.device, gui.fontImage, nullptr);
	vkDestroyImageView(gui.device, gui.fontImageView, nullptr);
	vkFreeMemory(gui.device, gui.fontMemory, nullptr);
	vkDestroySampler(gui.device, gui.fontSampler, nullptr);
	vkDestroyPipeline(gui.device, gui.graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(gui.device, gui.pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gui.device, gui.descriptorSetLayout, nullptr);

	IM_DELETE(&gui);

	ImGui::DestroyContext();
}

static void newGlfwFrame(
	GLFWwindow* _pWindow)
{
	int32_t windowWidth;
	int32_t windowHeight;
	glfwGetWindowSize(_pWindow, &windowWidth, &windowHeight);

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)windowWidth, (float)windowHeight);

	int32_t framebufferWidth;
	int32_t framebufferHeight;
	glfwGetFramebufferSize(_pWindow, &framebufferWidth, &framebufferHeight);

	if (windowWidth > 0 && windowHeight > 0)
	{
		io.DisplayFramebufferScale = ImVec2(
			(float)framebufferWidth / (float)windowWidth,
			(float)framebufferHeight / (float)windowHeight);
	}

	static double previousTime = 0.0;
	double currentTime = glfwGetTime();
	
	io.DeltaTime = (float)(currentTime - previousTime);
	previousTime = currentTime;

	double xMousePosition;
	double yMousePosition;
	glfwGetCursorPos(_pWindow, &xMousePosition, &yMousePosition);

	io.MousePos = ImVec2(xMousePosition, yMousePosition);
	io.MouseDown[0] = glfwGetMouseButton(_pWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	io.MouseDown[1] = glfwGetMouseButton(_pWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

struct TimeGraphState
{
	static const uint32_t kMaxPlotPoints = 64;
	std::array<float, kMaxPlotPoints> points{};
	const char* name = "Unassigned";
};

void plotTimeGraph(
	float _newPoint,
	TimeGraphState& _rState)
{
	std::rotate(_rState.points.begin(), _rState.points.begin() + 1, _rState.points.end());
	_rState.points.back() = _newPoint;

	char title[32];
	sprintf(title, "%s Time: %.2f ms", _rState.name, _newPoint);

	const float kMinPlotValue = 0.0f;
	const float kMaxPlotValue = 40.0f;
	ImGui::PlotLines("", &_rState.points[0], _rState.points.size(), 0,
		title, kMinPlotValue, kMaxPlotValue, ImVec2(270.0f, 50.0f));
}

void newFrameGUI(
	GLFWwindow* _pWindow,
	InfoGUI _info)
{
	newGlfwFrame(_pWindow);

	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(25, 30), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? 0.8f : 0.4f);

	ImGui::Begin("Performance", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("Device: %s", _info.deviceName);
	ImGui::Separator();

	static TimeGraphState cpuGraphState{};
	cpuGraphState.name = "CPU";
	plotTimeGraph(1.e3 * io.DeltaTime, cpuGraphState);

	static TimeGraphState gpuGraphState{};
	gpuGraphState.name = "GPU";
	plotTimeGraph(_info.gpuTime, gpuGraphState);

	ImGui::Separator();

	ImGui::Text("Input Assembly Vertices:     %lld", _info.inputAssemblyVertices);
	ImGui::Text("Input Assembly Primitives:   %lld", _info.inputAssemblyPrimitives);
	ImGui::Text("Vertex Shader Invocations:   %lld", _info.vertexShaderInvocations);
	ImGui::Text("Clipping Invocations:        %lld", _info.clippingInvocations);
	ImGui::Text("Clipping Primitives:         %lld", _info.clippingPrimitives);
	ImGui::Text("Fragment Shader Invocations: %lld", _info.fragmentShaderInvocations);
	ImGui::Text("Compute Shader Invocations:  %lld", _info.computeShaderInvocations);

	ImGui::End();

	ImGui::Render();
}

static void updateBuffers(
	uint32_t _frameIndex)
{
	ImDrawData* drawData = ImGui::GetDrawData();

	VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
	VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

	if (vertexBufferSize == 0 || indexBufferSize == 0)
	{
		return;
	}

	GUI& gui = getGUI();

	if (gui.vertexBuffers[_frameIndex].bufferVk == VK_NULL_HANDLE || gui.vertexBuffers[_frameIndex].size < vertexBufferSize)
	{
		destroyBuffer(gui.device, gui.vertexBuffers[_frameIndex]);

		gui.vertexBuffers[_frameIndex] = createBuffer(gui.physicalDevice, gui.device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize);
	}

	if (gui.indexBuffers[_frameIndex].bufferVk == VK_NULL_HANDLE || gui.indexBuffers[_frameIndex].size < indexBufferSize)
	{
		destroyBuffer(gui.device, gui.indexBuffers[_frameIndex]);

		gui.indexBuffers[_frameIndex] = createBuffer(gui.physicalDevice, gui.device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBufferSize);
	}

	ImDrawVert* vertexData = (ImDrawVert*)gui.vertexBuffers[_frameIndex].data;
	ImDrawIdx* indexData = (ImDrawIdx*)gui.indexBuffers[_frameIndex].data;

	for (int32_t cmdListIndex = 0; cmdListIndex < drawData->CmdListsCount; ++cmdListIndex)
	{
		const ImDrawList* cmdList = drawData->CmdLists[cmdListIndex];

		memcpy(vertexData, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(indexData, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

		vertexData += cmdList->VtxBuffer.Size;
		indexData += cmdList->IdxBuffer.Size;
	}
}

void drawFrameGUI(
	VkCommandBuffer _commandBuffer,
	uint32_t _frameIndex)
{
	updateBuffers(_frameIndex);

	ImGuiIO& io = ImGui::GetIO();
	GUI& gui = getGUI();

	vkCmdBindPipeline(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gui.graphicsPipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = io.DisplaySize.x;
	viewport.height = io.DisplaySize.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(_commandBuffer, 0, 1, &viewport);

	gui.pushConstantBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	gui.pushConstantBlock.translate = glm::vec2(-1.0f);

	vkCmdPushConstants(_commandBuffer, gui.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
		sizeof(gui.pushConstantBlock), &gui.pushConstantBlock);

	VkDescriptorImageInfo fontImageDescriptorInfo{};
	fontImageDescriptorInfo.sampler = gui.fontSampler;
	fontImageDescriptorInfo.imageView = gui.fontImageView;
	fontImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet writeDescriptorSets[1]{};
	writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[0].dstBinding = 0;
	writeDescriptorSets[0].descriptorCount = 1;
	writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSets[0].pImageInfo = &fontImageDescriptorInfo;

	vkCmdPushDescriptorSetKHR(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		gui.pipelineLayout, 0, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets);

	ImDrawData* pDrawData = ImGui::GetDrawData();
	int32_t globalIndexOffset = 0;
	int32_t globalVertexOffset = 0;

	if (pDrawData->CmdListsCount > 0)
	{
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(_commandBuffer, 0, 1, &gui.vertexBuffers[_frameIndex].bufferVk, offsets);
		vkCmdBindIndexBuffer(_commandBuffer, gui.indexBuffers[_frameIndex].bufferVk, 0, VK_INDEX_TYPE_UINT16);

		for (int32_t cmdListIndex = 0; cmdListIndex < pDrawData->CmdListsCount; ++cmdListIndex)
		{
			ImDrawList* pCmdList = pDrawData->CmdLists[cmdListIndex];
			for (int32_t cmdBufferIndex = 0; cmdBufferIndex < pCmdList->CmdBuffer.Size; ++cmdBufferIndex)
			{
				ImDrawCmd* pCmdBuffer = &pCmdList->CmdBuffer[cmdBufferIndex];

				VkRect2D scissorRect;
				scissorRect.offset.x = std::max((int32_t)(pCmdBuffer->ClipRect.x), 0);
				scissorRect.offset.y = std::max((int32_t)(pCmdBuffer->ClipRect.y), 0);
				scissorRect.extent.width = (uint32_t)(pCmdBuffer->ClipRect.z - pCmdBuffer->ClipRect.x);
				scissorRect.extent.height = (uint32_t)(pCmdBuffer->ClipRect.w - pCmdBuffer->ClipRect.y);

				vkCmdSetScissor(_commandBuffer, 0, 1, &scissorRect);

				uint32_t firstIndex = pCmdBuffer->IdxOffset + globalIndexOffset;
				int32_t vertexOffset = pCmdBuffer->VtxOffset + globalVertexOffset;
				vkCmdDrawIndexed(_commandBuffer, pCmdBuffer->ElemCount, 1, firstIndex, vertexOffset, 0);
			}

			globalIndexOffset += pCmdList->IdxBuffer.Size;
			globalVertexOffset += pCmdList->VtxBuffer.Size;
		}
	}
}
