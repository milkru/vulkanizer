#include "common.h"
#include "device.h"
#include "gui.h"
#include "pipeline.h"
#include "resources.h"
#include "sync.h"

#include <stdio.h>
#include <algorithm>
#include <imgui.h>

struct GUI
{
	struct
	{
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstantBlock;

	Device device;
	std::array<Buffer, kMaxFramesInFlightCount> vertexBuffers;
	std::array<Buffer, kMaxFramesInFlightCount> indexBuffers;
	VkSampler fontSampler;
	Image fontImage;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkDescriptorUpdateTemplate descriptorUpdateTemplate;
	VkPipeline graphicsPipeline;
};

static GUI& getGUI()
{
	return *(GUI*)ImGui::GetIO().BackendRendererUserData;
}

static void createFontTexture()
{
	ImGuiIO& io = ImGui::GetIO();
	GUI& gui = getGUI();

	unsigned char* fontData;
	int32_t textureWidth;
	int32_t textureHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &textureWidth, &textureHeight);

	gui.fontImage = createImage(gui.device, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureWidth, textureHeight, VK_FORMAT_R8G8B8A8_UNORM);

	VkDeviceSize uploadSize = VkDeviceSize(4 * textureWidth * textureHeight) * sizeof(char);

	Buffer stagingBuffer = createBuffer(gui.device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uploadSize);

	memcpy(stagingBuffer.data, fontData, uploadSize);

	immediateSubmit(gui.device, [&](VkCommandBuffer _commandBuffer)
		{
			transferImageLayout(
				_commandBuffer,
				gui.fontImage.imageVk,
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
				gui.fontImage.imageVk,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&bufferCopyRegion
			);

			transferImageLayout(
				_commandBuffer,
				gui.fontImage.imageVk,
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

	VK_CALL(vkCreateSampler(gui.device.deviceVk, &samplerCreateInfo, nullptr, &gui.fontSampler));
}

static void createPipeline(
	VkFormat _colorFormat,
	VkFormat _depthFormat,
	std::vector<Shader> _shaders)
{
	GUI& gui = getGUI();

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

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &_colorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = _depthFormat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineCreateInfo.stageCount = uint32_t(shaderStageCreateInfos.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = gui.pipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;


	VK_CALL(vkCreateGraphicsPipelines(gui.device.deviceVk, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &gui.graphicsPipeline));
}

void initializeGUI(
	Device _device,
	VkFormat _colorFormat,
	VkFormat _depthFormat,
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
	gui.device = _device;

	createFontTexture();

	Shader vertexShader = createShader(gui.device, "shaders/gui.vert.spv");
	Shader fragmentShader = createShader(gui.device, "shaders/gui.frag.spv");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(gui.pushConstantBlock);

	gui.descriptorSetLayout = createDescriptorSetLayout(gui.device.deviceVk, { vertexShader, fragmentShader });
	gui.pipelineLayout = createPipelineLayout(gui.device.deviceVk, { gui.descriptorSetLayout }, { pushConstantRange });
	gui.descriptorUpdateTemplate = createDescriptorUpdateTemplate(gui.device.deviceVk,
		gui.descriptorSetLayout, gui.pipelineLayout, VK_PIPELINE_BIND_POINT_GRAPHICS, { vertexShader, fragmentShader });

	createPipeline(_colorFormat, _depthFormat, { vertexShader, fragmentShader });

	destroyShader(gui.device, fragmentShader);
	destroyShader(gui.device, vertexShader);
}

void terminateGUI()
{
	GUI& gui = getGUI();

	for (Buffer& vertexBuffer : gui.vertexBuffers)
	{
		destroyBuffer(gui.device, vertexBuffer);
	}

	for (Buffer& indexBuffer : gui.indexBuffers)
	{
		destroyBuffer(gui.device, indexBuffer);
	}

	vkDestroySampler(gui.device.deviceVk, gui.fontSampler, nullptr);

	destroyImage(gui.device, gui.fontImage);

	vkDestroyPipeline(gui.device.deviceVk, gui.graphicsPipeline, nullptr);
	vkDestroyDescriptorUpdateTemplate(gui.device.deviceVk, gui.descriptorUpdateTemplate, nullptr);
	vkDestroyPipelineLayout(gui.device.deviceVk, gui.pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(gui.device.deviceVk, gui.descriptorSetLayout, nullptr);

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
	const float kMaxPlotValue = 20.0f;
	ImGui::PlotLines("", &_rState.points[0], _rState.points.size(), 0,
		title, kMinPlotValue, kMaxPlotValue, ImVec2(270.0f, 50.0f));
}

void newFrameGUI(
	GLFWwindow* _pWindow,
	InfoGUI& _rInfo)
{
	newGlfwFrame(_pWindow);

	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	{
		ImGui::SetNextWindowPos(ImVec2(25, 30), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? 0.8f : 0.4f);

		ImGui::Begin("Performance", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

		ImGui::Text("Device: %s", _rInfo.deviceName);
		ImGui::Separator();

		static TimeGraphState cpuGraphState{};
		cpuGraphState.name = "CPU";
		plotTimeGraph(1.e3 * io.DeltaTime, cpuGraphState);

		static TimeGraphState gpuGraphState{};
		gpuGraphState.name = "GPU";
		plotTimeGraph(_rInfo.gpuTime, gpuGraphState);

		ImGui::Separator();

		ImGui::Text("Input Assembly Vertices:     %lld", _rInfo.inputAssemblyVertices);
		ImGui::Text("Input Assembly Primitives:   %lld", _rInfo.inputAssemblyPrimitives);
		ImGui::Text("Vertex Shader Invocations:   %lld", _rInfo.vertexShaderInvocations);
		ImGui::Text("Clipping Invocations:        %lld", _rInfo.clippingInvocations);
		ImGui::Text("Clipping Primitives:         %lld", _rInfo.clippingPrimitives);
		ImGui::Text("Fragment Shader Invocations: %lld", _rInfo.fragmentShaderInvocations);
		ImGui::Text("Compute Shader Invocations:  %lld", _rInfo.computeShaderInvocations);

		ImGui::End();
	}

	{
		ImGui::SetNextWindowPos(ImVec2(25, 350), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? 0.8f : 0.4f);

		ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

		ImGui::BeginDisabled(!_rInfo.bMeshShadingPipelineSupported);
		ImGui::Checkbox("Mesh Shading Pipeline", &_rInfo.bMeshShadingPipelineEnabled);
		ImGui::BeginDisabled(!_rInfo.bMeshShadingPipelineEnabled);
		ImGui::Checkbox("Meshlet Cone Culling", &_rInfo.bMeshletConeCulling);
		ImGui::EndDisabled();
		ImGui::EndDisabled();

		ImGui::End();
	}

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

		gui.vertexBuffers[_frameIndex] = createBuffer(gui.device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferSize);
	}

	if (gui.indexBuffers[_frameIndex].bufferVk == VK_NULL_HANDLE || gui.indexBuffers[_frameIndex].size < indexBufferSize)
	{
		destroyBuffer(gui.device, gui.indexBuffers[_frameIndex]);

		gui.indexBuffers[_frameIndex] = createBuffer(gui.device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
	fontImageDescriptorInfo.imageView = gui.fontImage.view;
	fontImageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkCmdPushDescriptorSetWithTemplateKHR(_commandBuffer,
		gui.descriptorUpdateTemplate, gui.pipelineLayout, 0, &fontImageDescriptorInfo);

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
