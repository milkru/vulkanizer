#include "common.h"
#include "device.h"
#include "texture.h"
#include "gui.h"
#include "buffer.h"
#include "shader.h"
#include "pipeline.h"
#include "pass.h"
#include "frame_pacing.h"
#include "window.h"
#include "shaders/shader_constants.h"

#include <algorithm>
#include <stdio.h>
#include <imgui.h>

namespace gui
{
	struct Context
	{
		struct
		{
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstantBlock;

		Device device;
		std::array<Buffer, kMaxFramesInFlightCount> vertexBuffers;
		std::array<Buffer, kMaxFramesInFlightCount> indexBuffers;
		Texture fontTexture;
		Pipeline pipeline;
	};

	static Context& getContext()
	{
		return *(Context*)ImGui::GetIO().BackendRendererUserData;
	}

	static void createFontTexture()
	{
		ImGuiIO& io = ImGui::GetIO();
		Context& context = getContext();

		uint8_t* fontData;
		int32_t textureWidth;
		int32_t textureHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &textureWidth, &textureHeight);

		context.fontTexture = createTexture(context.device, {
			.width = uint32_t(textureWidth),
			.height = uint32_t(textureHeight),
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.sampler = createSampler(context.device.device, {
				.filter = VK_FILTER_LINEAR,
				.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR }) });

		// TODO-MILKRU: This is basically the pContents from buffer. Implement similar for textures.
		VkDeviceSize uploadSize = VkDeviceSize(4 * textureWidth * textureHeight) * sizeof(char);

		Buffer stagingBuffer = createBuffer(context.device, {
			.size = uploadSize,
			.access = MemoryAccess::Host,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT });

		memcpy(stagingBuffer.pMappedData, fontData, uploadSize);

		immediateSubmit(context.device, [&](VkCommandBuffer _commandBuffer)
			{
				textureBarrier(
					_commandBuffer,
					context.fontTexture,
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
					stagingBuffer.resource,
					context.fontTexture.resource,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &bufferCopyRegion
				);

				textureBarrier(
					_commandBuffer,
					context.fontTexture,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			});

		destroyBuffer(context.device, stagingBuffer);
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
				float(framebufferWidth) / float(windowWidth),
				float(framebufferHeight) / float(windowHeight));
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

		Context& context = getContext();

		if (context.vertexBuffers[_frameIndex].resource == VK_NULL_HANDLE || context.vertexBuffers[_frameIndex].size < vertexBufferSize)
		{
			destroyBuffer(context.device, context.vertexBuffers[_frameIndex]);

			context.vertexBuffers[_frameIndex] = createBuffer(context.device, {
				.size = vertexBufferSize,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT });
		}

		if (context.indexBuffers[_frameIndex].resource == VK_NULL_HANDLE || context.indexBuffers[_frameIndex].size < indexBufferSize)
		{
			destroyBuffer(context.device, context.indexBuffers[_frameIndex]);

			context.indexBuffers[_frameIndex] = createBuffer(context.device, {
				.size = indexBufferSize,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT });
		}

		ImDrawVert* vertexData = (ImDrawVert*)context.vertexBuffers[_frameIndex].pMappedData;
		ImDrawIdx* indexData = (ImDrawIdx*)context.indexBuffers[_frameIndex].pMappedData;

		for (int32_t cmdListIndex = 0; cmdListIndex < drawData->CmdListsCount; ++cmdListIndex)
		{
			const ImDrawList* cmdList = drawData->CmdLists[cmdListIndex];

			memcpy(vertexData, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indexData, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

			vertexData += cmdList->VtxBuffer.Size;
			indexData += cmdList->IdxBuffer.Size;
		}
	}

	void initialize(
		Device _device,
		VkFormat _colorFormat,
		VkFormat _depthFormat,
		float _windowWidth,
		float _windowHeight)
	{
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererUserData = new Context();
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

		Context& context = getContext();
		context.device = _device;

		createFontTexture();

		Shader vertShader = createShader(context.device, { .pPath = "shaders/gui.vert.spv", .pEntry = "main" });
		Shader fragShader = createShader(context.device, { .pPath = "shaders/gui.frag.spv", .pEntry = "main" });

		context.pipeline = createGraphicsPipeline(context.device, {
			.shaders = { vertShader, fragShader },
			.attachmentLayout = {
				.colorAttachmentStates = { {
					.format = _colorFormat,
					.blendEnable = true } },
				.depthStencilFormat = { _depthFormat }},
			.rasterizationState = {
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
			.depthStencilState = {
				.depthTestEnable = false,
				.depthWriteEnable = false,
				.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL } });

		destroyShader(context.device, fragShader);
		destroyShader(context.device, vertShader);
	}

	void terminate()
	{
		Context& context = getContext();

		for (Buffer& vertexBuffer : context.vertexBuffers)
		{
			destroyBuffer(context.device, vertexBuffer);
		}

		for (Buffer& indexBuffer : context.indexBuffers)
		{
			destroyBuffer(context.device, indexBuffer);
		}

		vkDestroySampler(context.device.device, context.fontTexture.sampler, nullptr);

		destroyTexture(context.device, context.fontTexture);
		destroyPipeline(context.device, context.pipeline);

		IM_DELETE(&context);

		ImGui::DestroyContext();
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

		char title[64];
		sprintf(title, "%s Time: %.2f ms", _rState.name, _newPoint);

		const float kMinPlotValue = 0.0f;
		const float kMaxPlotValue = 20.0f;
		ImGui::PlotLines("", &_rState.points[0], _rState.points.size(), 0,
			title, kMinPlotValue, kMaxPlotValue, ImVec2(300.0f, 50.0f));
	}

	void newFrame(
		GLFWwindow* _pWindow,
		State& _rState)
	{
		newGlfwFrame(_pWindow);

		ImGui::NewFrame();

		ImGuiIO& io = ImGui::GetIO();

		{
			ImGui::SetNextWindowPos(ImVec2(25, 30), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? 0.8f : 0.4f);

			ImGui::Begin("Performance", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

			ImGui::Text("Device: %s", _rState.deviceName);
			ImGui::Separator();

			static TimeGraphState cpuGraphState{};
			cpuGraphState.name = "CPU";
			plotTimeGraph(1.e3 * io.DeltaTime, cpuGraphState);

			static TimeGraphState generateDrawsGpuGraphState{};
			generateDrawsGpuGraphState.name = "Generate Draws GPU";
			plotTimeGraph(_rState.generateDrawsGpuTime, generateDrawsGpuGraphState);

			static TimeGraphState geometryGpuGraphState{};
			geometryGpuGraphState.name = "Geometry GPU";
			plotTimeGraph(_rState.geometryGpuTime, geometryGpuGraphState);

			ImGui::Separator();

			ImGui::Text("Input Assembly Vertices:     %lld", _rState.inputAssemblyVertices);
			ImGui::Text("Input Assembly Primitives:   %lld", _rState.inputAssemblyPrimitives);
			ImGui::Text("Vertex Shader Invocations:   %lld", _rState.vertexShaderInvocations);
			ImGui::Text("Clipping Invocations:        %lld", _rState.clippingInvocations);
			ImGui::Text("Clipping Primitives:         %lld", _rState.clippingPrimitives);
			ImGui::Text("Fragment Shader Invocations: %lld", _rState.fragmentShaderInvocations);
			ImGui::Text("Compute Shader Invocations:  %lld", _rState.computeShaderInvocations);

			ImGui::End();
		}

		{
			ImGui::SetNextWindowPos(ImVec2(25, 410), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ? 0.8f : 0.4f);

			ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

			ImGui::Checkbox("Force Lod", &_rState.bForceMeshLodEnabled);
			ImGui::BeginDisabled(!_rState.bForceMeshLodEnabled);
			ImGui::SameLine();
			ImGui::SliderInt("##Forced Lod", &_rState.forcedLod, 0, kMaxMeshLods - 1);
			ImGui::EndDisabled();
			ImGui::Checkbox("Mesh Frustum Culling", &_rState.bMeshFrustumCullingEnabled);
			ImGui::Checkbox("Freeze Camera", &_rState.bFreezeCameraEnabled);
			ImGui::Separator();

			ImGui::BeginDisabled(!_rState.bMeshShadingPipelineSupported);
			ImGui::Checkbox("Mesh Shading Pipeline", &_rState.bMeshShadingPipelineEnabled);
			ImGui::BeginDisabled(!_rState.bMeshShadingPipelineEnabled);
			ImGui::Checkbox("Meshlet Cone Culling", &_rState.bMeshletConeCullingEnabled);
			ImGui::Checkbox("Meshlet Frustum Culling", &_rState.bMeshletFrustumCullingEnabled);
			ImGui::EndDisabled();
			ImGui::EndDisabled();

			ImGui::End();
		}

		ImGui::Render();
	}

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		uint32_t _frameIndex,
		Texture _attachment)
	{
		updateBuffers(_frameIndex);

		ImGuiIO& io = ImGui::GetIO();
		Context& context = getContext();

		context.pushConstantBlock = {
			.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y),
			.translate = glm::vec2(-1.0f) };

		executePass(_commandBuffer, {
			.pipeline = context.pipeline,
			.viewport = {
				.offset = { 0.0f, 0.0f },
				.extent = { io.DisplaySize.x, io.DisplaySize.y }},
			.colorAttachments = {{
				.texture = _attachment,
				.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD }},
			.bindings = {
				context.vertexBuffers[_frameIndex],
				context.fontTexture },
			.pushConstants = {
				.size = sizeof(context.pushConstantBlock),
				.pData = &context.pushConstantBlock } },
				[&]()
			{
				ImDrawData* pDrawData = ImGui::GetDrawData();
				int32_t globalIndexOffset = 0;
				int32_t globalVertexOffset = 0;

				if (pDrawData->CmdListsCount > 0)
				{
					vkCmdBindIndexBuffer(_commandBuffer, context.indexBuffers[_frameIndex].resource, 0, VK_INDEX_TYPE_UINT16);

					for (int32_t cmdListIndex = 0; cmdListIndex < pDrawData->CmdListsCount; ++cmdListIndex)
					{
						ImDrawList* pCmdList = pDrawData->CmdLists[cmdListIndex];
						for (int32_t cmdBufferIndex = 0; cmdBufferIndex < pCmdList->CmdBuffer.Size; ++cmdBufferIndex)
						{
							ImDrawCmd* pCmdBuffer = &pCmdList->CmdBuffer[cmdBufferIndex];

							VkRect2D scissorRect = {
								.offset = {
									.x = std::max((int32_t)(pCmdBuffer->ClipRect.x), 0),
									.y = std::max((int32_t)(pCmdBuffer->ClipRect.y), 0) },
								.extent = {
									.width = uint32_t(pCmdBuffer->ClipRect.z - pCmdBuffer->ClipRect.x),
									.height = uint32_t(pCmdBuffer->ClipRect.w - pCmdBuffer->ClipRect.y) } };

							vkCmdSetScissor(_commandBuffer, 0, 1, &scissorRect);

							uint32_t firstIndex = pCmdBuffer->IdxOffset + globalIndexOffset;
							int32_t vertexOffset = pCmdBuffer->VtxOffset + globalVertexOffset;
							vkCmdDrawIndexed(_commandBuffer, pCmdBuffer->ElemCount, 1, firstIndex, vertexOffset, 0);
						}

						globalIndexOffset += pCmdList->IdxBuffer.Size;
						globalVertexOffset += pCmdList->VtxBuffer.Size;
					}
				}
			});
	}
}
