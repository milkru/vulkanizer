#include "core/device.h"
#include "core/texture.h"
#include "core/buffer.h"
#include "core/shader.h"
#include "core/pipeline.h"
#include "core/pass.h"
#include "core/frame_pacing.h"
#include "core/query.h"

#include "gui.h"
#include "window.h"
#include "gpu_profiler.h"
#include "shaders/shader_interop.h"

#include <algorithm>
#include <stdio.h>
#include <imgui.h>

namespace gui
{
	struct Context
	{
		Context(Device& _rDevice)
			: rDevice(_rDevice) {}

		struct
		{
			v2 scale;
			v2 translate;
		} pushConstantBlock;

		Device& rDevice;
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
		ImGuiIO& rIO = ImGui::GetIO();
		Context& rContext = getContext();

		u8* pFontData;
		i32 textureWidth;
		i32 textureHeight;
		rIO.Fonts->GetTexDataAsRGBA32(&pFontData, &textureWidth, &textureHeight);

		rContext.fontTexture = createTexture(rContext.rDevice, {
			.width = u32(textureWidth),
			.height = u32(textureHeight),
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.sampler = {
				.filterMode = VK_FILTER_LINEAR,
				.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR } });

		// TODO-MILKRU: This is basically the pContents from buffer. Implement similar for textures.
		VkDeviceSize uploadSize = VkDeviceSize(4u * textureWidth * textureHeight) * sizeof(char);

		Buffer stagingBuffer = createBuffer(rContext.rDevice, {
			.byteSize = uploadSize,
			.access = MemoryAccess::Host,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT });

		memcpy(stagingBuffer.pMappedData, pFontData, uploadSize);

		immediateSubmit(rContext.rDevice, [&](VkCommandBuffer _commandBuffer)
			{
				textureBarrier(_commandBuffer, rContext.fontTexture,
					VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				VkBufferImageCopy bufferCopyRegion{};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = textureWidth;
				bufferCopyRegion.imageExtent.height = textureHeight;
				bufferCopyRegion.imageExtent.depth = 1;

				vkCmdCopyBufferToImage(
					_commandBuffer,
					stagingBuffer.resource,
					rContext.fontTexture.resource,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &bufferCopyRegion
				);

				textureBarrier(_commandBuffer, rContext.fontTexture,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			});

		destroyBuffer(rContext.rDevice, stagingBuffer);
	}

	static void newGlfwFrame(
		GLFWwindow* _pWindow)
	{
		i32 windowWidth;
		i32 windowHeight;
		glfwGetWindowSize(_pWindow, &windowWidth, &windowHeight);

		ImGuiIO& rIO = ImGui::GetIO();
		rIO.DisplaySize = ImVec2((f32)windowWidth, (f32)windowHeight);

		i32 framebufferWidth;
		i32 framebufferHeight;
		glfwGetFramebufferSize(_pWindow, &framebufferWidth, &framebufferHeight);

		if (windowWidth > 0 && windowHeight > 0)
		{
			rIO.DisplayFramebufferScale = ImVec2(
				f32(framebufferWidth) / f32(windowWidth),
				f32(framebufferHeight) / f32(windowHeight));
		}

		static f64 previousTime = 0.0;
		f64 currentTime = glfwGetTime();

		rIO.DeltaTime = (f32)(currentTime - previousTime);
		previousTime = currentTime;

		f64 xMousePosition;
		f64 yMousePosition;
		glfwGetCursorPos(_pWindow, &xMousePosition, &yMousePosition);

		rIO.MousePos = ImVec2(xMousePosition, yMousePosition);
		rIO.MouseDown[0] = glfwGetMouseButton(_pWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
		rIO.MouseDown[1] = glfwGetMouseButton(_pWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
	}

	static void updateBuffers(
		u32 _frameIndex)
	{
		ImDrawData* drawData = ImGui::GetDrawData();

		VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

		if (vertexBufferSize == 0 || indexBufferSize == 0)
		{
			return;
		}

		Context& rContext = getContext();

		if (rContext.vertexBuffers[_frameIndex].resource == VK_NULL_HANDLE ||
			rContext.vertexBuffers[_frameIndex].byteSize < vertexBufferSize)
		{
			if (rContext.vertexBuffers[_frameIndex].resource != VK_NULL_HANDLE)
			{
				destroyBuffer(rContext.rDevice, rContext.vertexBuffers[_frameIndex]);
			}

			rContext.vertexBuffers[_frameIndex] = createBuffer(rContext.rDevice, {
				.byteSize = vertexBufferSize,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT });
		}

		if (rContext.indexBuffers[_frameIndex].resource == VK_NULL_HANDLE ||
			rContext.indexBuffers[_frameIndex].byteSize < indexBufferSize)
		{
			if (rContext.indexBuffers[_frameIndex].resource != VK_NULL_HANDLE)
			{
				destroyBuffer(rContext.rDevice, rContext.indexBuffers[_frameIndex]);
			}

			rContext.indexBuffers[_frameIndex] = createBuffer(rContext.rDevice, {
				.byteSize = indexBufferSize,
				.access = MemoryAccess::Host,
				.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT });
		}

		ImDrawVert* vertexData = (ImDrawVert*)rContext.vertexBuffers[_frameIndex].pMappedData;
		ImDrawIdx* indexData = (ImDrawIdx*)rContext.indexBuffers[_frameIndex].pMappedData;

		for (i32 cmdListIndex = 0; cmdListIndex < drawData->CmdListsCount; ++cmdListIndex)
		{
			ImDrawList* pCmdList = drawData->CmdLists[cmdListIndex];

			memcpy(vertexData, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(indexData, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

			vertexData += pCmdList->VtxBuffer.Size;
			indexData += pCmdList->IdxBuffer.Size;
		}
	}

	void initialize(
		Device& _rDevice,
		VkFormat _colorFormat,
		VkFormat _depthFormat,
		f32 _windowWidth,
		f32 _windowHeight)
	{
		ImGui::CreateContext();

		ImGuiIO& rIO = ImGui::GetIO();
		rIO.BackendRendererUserData = new Context(_rDevice);
		rIO.DisplaySize = ImVec2(_windowWidth, _windowHeight);
		rIO.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

		ImGuiStyle& rStyle = ImGui::GetStyle();
		rStyle.FrameRounding = 5.0f;
		rStyle.WindowRounding = 7.0f;
		rStyle.WindowBorderSize = 2.0f;

		for (u32 colorIndex = 0; colorIndex < ImGuiCol_COUNT; ++colorIndex)
		{
			ImVec4& rColor = rStyle.Colors[colorIndex];
			rColor.x = rColor.y = rColor.z = (0.2125f * rColor.x) + (0.7154f * rColor.y) + (0.0721f * rColor.z);
		}

		Context& rContext = getContext();
		rContext.rDevice = _rDevice;

		createFontTexture();

		Shader vertShader = createShader(rContext.rDevice, { .pPath = "shaders/gui.vert.spv", .pEntry = "main" });
		Shader fragShader = createShader(rContext.rDevice, { .pPath = "shaders/gui.frag.spv", .pEntry = "main" });

		rContext.pipeline = createGraphicsPipeline(rContext.rDevice, {
			.shaders = { vertShader, fragShader },
			.attachmentLayout = {
				.colorAttachments = { {
					.format = _colorFormat,
					.bBlendEnable = true } },
				.depthStencilFormat = { _depthFormat }},
			.rasterization = {
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
			.depthStencil = {
				.bDepthTestEnable = false,
				.bDepthWriteEnable = false,
				.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL } });

		destroyShader(rContext.rDevice, fragShader);
		destroyShader(rContext.rDevice, vertShader);
	}

	void terminate()
	{
		Context& rContext = getContext();

		for (Buffer& rVertexBuffer : rContext.vertexBuffers)
		{
			destroyBuffer(rContext.rDevice, rVertexBuffer);
		}

		for (Buffer& rIndexBuffer : rContext.indexBuffers)
		{
			destroyBuffer(rContext.rDevice, rIndexBuffer);
		}

		destroyTexture(rContext.rDevice, rContext.fontTexture);
		destroyPipeline(rContext.rDevice, rContext.pipeline);

		IM_DELETE(&rContext);

		ImGui::DestroyContext();
	}

	struct TimeGraphState
	{
		static const u32 kMaxPlotPoints = 64;
		std::array<f32, kMaxPlotPoints> points{};
		const char* name = "Unassigned";
	};

	void plotTimeGraph(
		f32 _newPoint,
		TimeGraphState& _rGraphState)
	{
		std::rotate(_rGraphState.points.begin(), _rGraphState.points.begin() + 1, _rGraphState.points.end());
		_rGraphState.points.back() = _newPoint;

		char title[64];
		sprintf(title, "%s Time: %.2f ms", _rGraphState.name, _newPoint);

		f32 kMinPlotValue = 0.0f;
		f32 kMaxPlotValue = 20.0f;
		ImGui::PlotLines("", &_rGraphState.points[0], _rGraphState.points.size(), 0,
			title, kMinPlotValue, kMaxPlotValue, ImVec2(300.0f, 50.0f));
	}

	void newFrame(
		GLFWwindow* _pWindow,
		Settings& _rSettings)
	{
		newGlfwFrame(_pWindow);

		ImGui::NewFrame();

		ImGuiIO& rIO = ImGui::GetIO();

		{
			static bool bWindowHovered = false;

			ImGui::SetNextWindowPos(ImVec2(25, 30), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(bWindowHovered ? 0.8f : 0.4f);

			ImGui::Begin("Performance", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

			bWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

			ImGui::Text("Device: %s", _rSettings.deviceName);
			ImGui::Separator();

			{
				static TimeGraphState timeGraph{};
				timeGraph.name = "CPU Time";
				plotTimeGraph(1.e3 * rIO.DeltaTime, timeGraph);
			}

			{
				static TimeGraphState timeGraph{};
				timeGraph.name = "GPU Time";

				f64 totalGpuTime = 0.0;
				for (auto& rGpuTime : _rSettings.gpuTimes)
				{
					totalGpuTime += rGpuTime.second;
				}

				plotTimeGraph(totalGpuTime, timeGraph);
			}

			ImGui::Separator();

			for (auto& rGpuTime : _rSettings.gpuTimes)
			{
				ImGui::Text("%.3f ms - %s", rGpuTime.second, rGpuTime.first.c_str());
			}

			ImGui::Separator();

			{
				ImGui::Text("Input Assembly Vertices:     %lld", _rSettings.inputAssemblyVertices);
				ImGui::Text("Input Assembly Primitives:   %lld", _rSettings.inputAssemblyPrimitives);
				ImGui::Text("Vertex Shader Invocations:   %lld", _rSettings.vertexShaderInvocations);
				ImGui::Text("Clipping Invocations:        %lld", _rSettings.clippingInvocations);
				ImGui::Text("Clipping Primitives:         %lld", _rSettings.clippingPrimitives);
				ImGui::Text("Fragment Shader Invocations: %lld", _rSettings.fragmentShaderInvocations);
				ImGui::Text("Compute Shader Invocations:  %lld", _rSettings.computeShaderInvocations);
			}

			ImGui::End();
		}

		{
			static bool bWindowHovered = false;

			ImGui::SetNextWindowPos(ImVec2(25, 435), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(bWindowHovered ? 0.8f : 0.4f);

			ImGui::Begin("Settings", 0, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

			bWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);

			ImGui::Checkbox("Force Lod", &_rSettings.bForceMeshLodEnabled);
			ImGui::BeginDisabled(!_rSettings.bForceMeshLodEnabled);
			ImGui::SameLine();
			ImGui::SliderInt("##Forced Lod", &_rSettings.forcedLod, 0, kMaxMeshLods - 1);
			ImGui::EndDisabled();
			ImGui::Checkbox("Mesh Frustum Culling", &_rSettings.bMeshFrustumCullingEnabled);
			ImGui::Checkbox("Mesh Occlusion Culling", &_rSettings.bMeshOcclusionCullingEnabled);
			ImGui::Checkbox("Freeze Camera", &_rSettings.bFreezeCameraEnabled);
			ImGui::Separator();

			ImGui::BeginDisabled(!_rSettings.bMeshShadingPipelineSupported);
			ImGui::Checkbox("Mesh Shading Pipeline", &_rSettings.bMeshShadingPipelineEnabled);
			ImGui::BeginDisabled(!_rSettings.bMeshShadingPipelineEnabled);
			ImGui::Checkbox("Meshlet Cone Culling", &_rSettings.bMeshletConeCullingEnabled);
			ImGui::Checkbox("Meshlet Frustum Culling", &_rSettings.bMeshletFrustumCullingEnabled);
			ImGui::EndDisabled();
			ImGui::EndDisabled();

			ImGui::End();
		}

		ImGui::Render();
	}

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		u32 _frameIndex,
		Texture& _rAttachment)
	{
		GPU_BLOCK(_commandBuffer, "GUI");

		updateBuffers(_frameIndex);

		Context& rContext = getContext();

		if (rContext.vertexBuffers[_frameIndex].resource == VK_NULL_HANDLE ||
			rContext.indexBuffers[_frameIndex].resource == VK_NULL_HANDLE)
		{
			return;
		}

		ImGuiIO& rIO = ImGui::GetIO();

		rContext.pushConstantBlock = {
			.scale = v2(2.0f / rIO.DisplaySize.x, 2.0f / rIO.DisplaySize.y),
			.translate = v2(-1.0f) };

		executePass(_commandBuffer, {
			.pipeline = rContext.pipeline,
			.viewport = {
				.offset = { 0.0f, 0.0f },
				.extent = { rIO.DisplaySize.x, rIO.DisplaySize.y }},
			.colorAttachments = {{
				.texture = _rAttachment,
				.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD }},
			.bindings = {
				Binding(rContext.vertexBuffers[_frameIndex]),
				Binding(rContext.fontTexture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) },
			.pushConstants = {
				.byteSize = sizeof(rContext.pushConstantBlock),
				.pData = &rContext.pushConstantBlock } },
				[&]()
			{
				ImDrawData* pDrawData = ImGui::GetDrawData();
				i32 globalIndexOffset = 0;
				i32 globalVertexOffset = 0;

				if (pDrawData->CmdListsCount > 0)
				{
					vkCmdBindIndexBuffer(_commandBuffer, rContext.indexBuffers[_frameIndex].resource, 0, VK_INDEX_TYPE_UINT16);

					for (i32 cmdListIndex = 0; cmdListIndex < pDrawData->CmdListsCount; ++cmdListIndex)
					{
						ImDrawList* pCmdList = pDrawData->CmdLists[cmdListIndex];
						for (i32 cmdBufferIndex = 0; cmdBufferIndex < pCmdList->CmdBuffer.Size; ++cmdBufferIndex)
						{
							ImDrawCmd* pCmdBuffer = &pCmdList->CmdBuffer[cmdBufferIndex];

							VkRect2D scissorRect = {
								.offset = {
									.x = std::max((i32)(pCmdBuffer->ClipRect.x), 0),
									.y = std::max((i32)(pCmdBuffer->ClipRect.y), 0) },
								.extent = {
									.width = u32(pCmdBuffer->ClipRect.z - pCmdBuffer->ClipRect.x),
									.height = u32(pCmdBuffer->ClipRect.w - pCmdBuffer->ClipRect.y) } };

							vkCmdSetScissor(_commandBuffer, 0, 1, &scissorRect);

							u32 firstIndex = pCmdBuffer->IdxOffset + globalIndexOffset;
							i32 vertexOffset = pCmdBuffer->VtxOffset + globalVertexOffset;
							vkCmdDrawIndexed(_commandBuffer, pCmdBuffer->ElemCount, 1, firstIndex, vertexOffset, 0);
						}

						globalIndexOffset += pCmdList->IdxBuffer.Size;
						globalVertexOffset += pCmdList->VtxBuffer.Size;
					}
				}
			});
	}

	void updateGpuPerformanceState(
		VkPhysicalDeviceLimits _deviceLimits,
		Settings& _rSettings)
	{
		for (auto& rBlockName : gpu::profiler::getBlockNames())
		{
			f64 result;
			if (GPU_BLOCK_RESULT(rBlockName, _deviceLimits, result))
			{
				_rSettings.gpuTimes.erase(rBlockName);
				_rSettings.gpuTimes[rBlockName] = result;
			}
		}

		GPU_STATS_RESULT("Frame", StatType::InputAssemblyVertices, _rSettings.inputAssemblyVertices);
		GPU_STATS_RESULT("Frame", StatType::InputAssemblyPrimitives, _rSettings.inputAssemblyPrimitives);
		GPU_STATS_RESULT("Frame", StatType::VertexShaderInvocations, _rSettings.vertexShaderInvocations);
		GPU_STATS_RESULT("Frame", StatType::ClippingInvocations, _rSettings.clippingInvocations);
		GPU_STATS_RESULT("Frame", StatType::ClippingPrimitives, _rSettings.clippingPrimitives);
		GPU_STATS_RESULT("Frame", StatType::FragmentShaderInvocations, _rSettings.fragmentShaderInvocations);
		GPU_STATS_RESULT("Frame", StatType::ComputeShaderInvocations, _rSettings.computeShaderInvocations);
	}
}
