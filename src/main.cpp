#include "common.h"
#include "window.h"
#include "camera.h"
#include "device.h"
#include "buffer.h"
#include "texture.h"
#include "shader.h"
#include "frame_pacing.h"
#include "swapchain.h"
#include "pipeline.h"
#include "pass.h"
#include "query.h"
#include "shaders/shader_constants.h"
#include "geometry.h"
#include "gui.h"
#include "utils.h"

#include <easy/profiler.h>

#include <string.h>
#include <chrono>

#ifdef DEBUG_
const bool kbEnableValidationLayers = true;
#else
const bool kbEnableValidationLayers = false;
#endif

const bool kbEnableMeshShadingPipeline = true;

const uint32_t kPreferredSwapchainImageCount = 2u;
const bool kbEnableVSync = false;

const uint32_t kWindowWidth = 1280u;
const uint32_t kWindowHeight = 720u;

const uint32_t kMaxDrawCount = 100000u;
const float kSpawnCubeSize = 100.0f;

struct alignas(16) PerDrawData
{
	glm::mat4 model{};
	uint32_t meshIndex = 0u;
};

struct DrawCommand
{
	uint32_t indexCount = 0u;
	uint32_t instanceCount = 0u;
	uint32_t firstIndex = 0u;
	uint32_t vertexOffset = 0u;
	uint32_t firstInstance = 0u;

	uint32_t taskCount = 0u;
	uint32_t firstTask = 0u;

	uint32_t drawIndex = 0u;
	uint32_t lodIndex = 0u;
};

struct DrawBuffers
{
	Buffer drawsBuffer{};
	Buffer drawCommandsBuffer{};
	Buffer drawCountBuffer{};
};

int32_t main(
	int32_t _argc,
	const char** _argv)
{
	EASY_MAIN_THREAD;
	EASY_PROFILER_ENABLE;

	if (_argc <= 1)
	{
		printf("Provide mesh paths as command arguments.\n");
		return 1;
	}

	GLFWwindow* pWindow = createWindow({
		.width = kWindowWidth,
		.height = kWindowHeight,
		.title = "vulkanizer" });

	Device device = createDevice(pWindow, {
		.bEnableValidationLayers = kbEnableValidationLayers,
		.bEnableMeshShadingPipeline = kbEnableMeshShadingPipeline });

	Swapchain swapchain = createSwapchain(pWindow, device, {
		.bEnableVSync = kbEnableVSync,
		.preferredSwapchainImageCount = kPreferredSwapchainImageCount });

	Camera camera = {
		.fov = 45.0f,
		.aspect = float(swapchain.extent.width) / float(swapchain.extent.height),
		.near = 0.01f,
		.moveSpeed = 2.0f,
		.sensitivity = 55.0f,
		.pitch = 0.0f,
		.yaw = 90.0f,
		.position = glm::vec3(0.0f) };

	Texture depthTexture = createTexture(device, {
		.width = swapchain.extent.width,
		.height = swapchain.extent.height,
		.format = VK_FORMAT_D32_SFLOAT,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL });

	Shader generateDrawsShader = createShader(device, {
		.pPath = "shaders/generate_draws.comp.spv",
		.pEntry = "main" });

	Shader taskShader = device.bMeshShadingPipelineAllowed ?
		createShader(device, {
			.pPath = "shaders/geometry.task.spv",
			.pEntry = "main" }) : Shader();

	Shader meshShader = device.bMeshShadingPipelineAllowed ?
		createShader(device, {
			.pPath = "shaders/geometry.mesh.spv",
			.pEntry = "main" }) : Shader();

	Shader vertShader = createShader(device, {
		.pPath = "shaders/geometry.vert.spv",
		.pEntry = "main" });

	Shader fragShader = createShader(device, {
		.pPath = "shaders/color.frag.spv",
		.pEntry = "main" });

	Pipeline generateDrawsPipeline = createComputePipeline(device, generateDrawsShader);

	Pipeline geometryPipeline = createGraphicsPipeline(device, {
		.shaders = { vertShader, fragShader },
		.attachmentLayout = {
			.colorAttachmentStates = { {
				.format = swapchain.format,
				.blendEnable = true } },
			.depthStencilFormat = { depthTexture.format }},
		.rasterizationState = {
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
		.depthStencilState = {
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthCompareOp = VK_COMPARE_OP_GREATER } });

	Pipeline geometryMeshletPipeline = device.bMeshShadingPipelineAllowed ?
		createGraphicsPipeline(device, {
			.shaders = { taskShader, meshShader, fragShader },
			.attachmentLayout = {
				.colorAttachmentStates = { {
					.format = swapchain.format,
					.blendEnable = true } },
				.depthStencilFormat = { depthTexture.format }},
			.rasterizationState = {
				.cullMode = VK_CULL_MODE_BACK_BIT,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE },
			.depthStencilState = {
				.depthTestEnable = true,
				.depthWriteEnable = true,
				.depthCompareOp = VK_COMPARE_OP_GREATER } }) : Pipeline();

	destroyShader(device, generateDrawsShader);

	if (device.bMeshShadingPipelineAllowed)
	{
		destroyShader(device, meshShader);
		destroyShader(device, taskShader);
	}

	destroyShader(device, fragShader);
	destroyShader(device, vertShader);

	GeometryBuffers geometryBuffers;
	{
		EASY_BLOCK("InitializeGeometry");

		Geometry geometry{};

		for (uint32_t meshIndex = 0; meshIndex < _argc - 1; ++meshIndex)
		{
			const char* meshPath = _argv[meshIndex + 1];
			loadMesh(geometry, meshPath, device.bMeshShadingPipelineAllowed);
		}

		geometryBuffers = {
			.meshletBuffer = device.bMeshShadingPipelineAllowed ?
				createBuffer(device, {
					.size = sizeof(Meshlet) * geometry.meshlets.size(),
					.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.pContents = geometry.meshlets.data() }) : Buffer(),
			
			.meshletVerticesBuffer = device.bMeshShadingPipelineAllowed ?
				createBuffer(device, {
					.size = sizeof(uint32_t) * geometry.meshletVertices.size(),
					.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.pContents = geometry.meshletVertices.data() }) : Buffer(),
			
			.meshletTrianglesBuffer = device.bMeshShadingPipelineAllowed ?
				createBuffer(device, {
					.size = sizeof(uint8_t) * geometry.meshletTriangles.size(),
					.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.pContents = geometry.meshletTriangles.data() }) : Buffer(),
			
			.vertexBuffer = createBuffer(device, {
				.size = sizeof(Vertex) * geometry.vertices.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = geometry.vertices.data() }),
			
			.indexBuffer = createBuffer(device, {
				.size = sizeof(uint32_t) * geometry.indices.size(),
				.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				.pContents = geometry.indices.data() }),
			
			.meshesBuffer = createBuffer(device, {
				.size = sizeof(Mesh) * geometry.meshes.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = geometry.meshes.data() }) };
	}

	DrawBuffers drawBuffers;
	{
		EASY_BLOCK("InitializeDraws");

		std::vector<PerDrawData> perDrawDataVector(kMaxDrawCount);
		for (uint32_t drawIndex = 0; drawIndex < kMaxDrawCount; ++drawIndex)
		{
			PerDrawData perDrawData = { .meshIndex = drawIndex % (_argc - 1) };

			auto randomFloat = []()
			{
				return float(rand()) / RAND_MAX;
			};

			// TODO-MILKRU: Multiply meshlet/mesh bounding spheres by scale.
			perDrawData.model = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

			perDrawData.model = glm::rotate(perDrawData.model,
				glm::radians(360.0f * randomFloat()), glm::vec3(0.0, 1.0, 0.0));

			perDrawData.model = glm::translate(perDrawData.model, {
				kSpawnCubeSize * (randomFloat() - 0.5f),
				kSpawnCubeSize * (randomFloat() - 0.5f),
				kSpawnCubeSize * (randomFloat() - 0.5f) });

			perDrawDataVector[drawIndex] = perDrawData;
		}

		drawBuffers = {
			.drawsBuffer = createBuffer(device, {
				.size = sizeof(PerDrawData) * perDrawDataVector.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = perDrawDataVector.data() }),
			
			.drawCommandsBuffer = createBuffer(device, {
				.size = sizeof(DrawCommand) * kMaxDrawCount,
				.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT }),
			
			.drawCountBuffer = createBuffer(device, {
				.size = sizeof(uint32_t),
				.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT }) };
	}

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

	QueryPool timestampsQueryPool = createQueryPool(device, {
		.type = VK_QUERY_TYPE_TIMESTAMP,
		.queryCount = 4 });

	QueryPool statisticsQueryPool = createQueryPool(device, {
		.type = VK_QUERY_TYPE_PIPELINE_STATISTICS,
		.queryCount = 1 });

	alignas(16) struct
	{
		glm::mat4 viewProjection;
		glm::vec4 frustumPlanes[6];
		glm::vec3 cameraPosition;
		uint32_t maxDrawCount;
		float lodTransitionBase;
		float lodTransitionStep;
		int32_t forcedLod;
		uint32_t enableMeshFrustumCulling;
		uint32_t enableMeshletConeCulling;
		uint32_t enableMeshletFrustumCulling;
	} perFrameData;

	uint32_t frameIndex = 0;

	gui::initialize(device, swapchain.format, depthTexture.format, (float)kWindowWidth, (float)kWindowHeight);

	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(device.physicalDevice, &physicalDeviceProperties);

	gui::State guiState = {
		.deviceName = physicalDeviceProperties.deviceName,
		.bFreezeCameraEnabled = false,
		.bMeshFrustumCullingEnabled = true };

	bool bMeshShadingPipelineEnabled =
		guiState.bMeshletConeCullingEnabled =
		guiState.bMeshletFrustumCullingEnabled =
		guiState.bMeshShadingPipelineEnabled =
		guiState.bMeshShadingPipelineSupported =
		device.bMeshShadingPipelineAllowed;

	while (!glfwWindowShouldClose(pWindow))
	{
		EASY_BLOCK("Frame");

		glfwPollEvents();

		gui::newFrame(pWindow, guiState);

		bMeshShadingPipelineEnabled = guiState.bMeshShadingPipelineEnabled;

		VkCommandBuffer commandBuffer = commandBuffers[frameIndex];
		FramePacingState framePacingState = framePacingStates[frameIndex];

		{
			EASY_BLOCK("WaitForFences");
			VK_CALL(vkWaitForFences(device.device, 1, &framePacingState.inFlightFence, VK_TRUE, UINT64_MAX));
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

			VK_CALL(vkDeviceWaitIdle(device.device));

			destroyTexture(device, depthTexture);

			Swapchain newSwapchain = createSwapchain(pWindow, device, {
				.bEnableVSync = kbEnableVSync,
				.preferredSwapchainImageCount = kPreferredSwapchainImageCount,
				.oldSwapchain = swapchain.swapchain });

			destroySwapchain(device, swapchain);
			swapchain = newSwapchain;

			depthTexture = createTexture(device, {
				.width = swapchain.extent.width,
				.height = swapchain.extent.height,
				.format = VK_FORMAT_D32_SFLOAT,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL });

			continue;
		}

		VK_CALL(vkResetFences(device.device, 1, &framePacingState.inFlightFence));

		uint32_t currentSwapchainImageIndex;
		VK_CALL(vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX,
			framePacingState.imageAvailableSemaphore, VK_NULL_HANDLE, &currentSwapchainImageIndex));

		{
			EASY_BLOCK("Draw");

			vkResetCommandBuffer(commandBuffer, 0);
			VkCommandBufferBeginInfo commandBufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			VK_CALL(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

			resetQueryPool(commandBuffer, timestampsQueryPool);
			resetQueryPool(commandBuffer, statisticsQueryPool);

			{
				GPU_STATS(commandBuffer, statisticsQueryPool, Main);

				{
					EASY_BLOCK("UpdateFrameData");

					static auto previousTime = std::chrono::high_resolution_clock::now();
					auto currentTime = std::chrono::high_resolution_clock::now();

					float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
					previousTime = currentTime;

					updateCamera(pWindow, deltaTime, camera);

					perFrameData.viewProjection = camera.projection * camera.view;
					perFrameData.maxDrawCount = kMaxDrawCount;
					// TODO-MILKRU: Rename?
					perFrameData.lodTransitionBase = 4.0f;
					perFrameData.lodTransitionStep = 1.25f;
					perFrameData.forcedLod = guiState.bForceMeshLodEnabled ? guiState.forcedLod : -1;
					perFrameData.enableMeshFrustumCulling = guiState.bMeshFrustumCullingEnabled ? 1u : 0u;
					perFrameData.enableMeshletConeCulling = guiState.bMeshletConeCullingEnabled ? 1u : 0u;
					perFrameData.enableMeshletFrustumCulling = guiState.bMeshletFrustumCullingEnabled ? 1u : 0u;

					if (!guiState.bFreezeCameraEnabled)
					{
						perFrameData.cameraPosition = camera.position;
						getFrustumPlanes(camera, perFrameData.frustumPlanes);
					}
				}

				{
					GPU_BLOCK(commandBuffer, timestampsQueryPool, GenerateDraws);

					fillBuffer(commandBuffer, device, drawBuffers.drawCountBuffer, 0,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

					executePass(commandBuffer, {
						.pipeline = generateDrawsPipeline,
						.bindings = {
							geometryBuffers.meshesBuffer,
							drawBuffers.drawsBuffer,
							drawBuffers.drawCommandsBuffer,
							drawBuffers.drawCountBuffer },
						.pushConstants = {
							.size = sizeof(perFrameData),
							.pData = &perFrameData } },
							[&]()
						{
							vkCmdDispatch(commandBuffer, divideRoundingUp(kMaxDrawCount, kShaderGroupSizeNV), 1, 1);
						});

					bufferBarrier(commandBuffer, device, drawBuffers.drawCountBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

					bufferBarrier(commandBuffer, device, drawBuffers.drawCommandsBuffer,
						VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
				}

				{
					GPU_BLOCK(commandBuffer, timestampsQueryPool, Geometry);

					textureBarrier(commandBuffer, swapchain.textures[currentSwapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
						VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

					executePass(commandBuffer, {
						.pipeline = bMeshShadingPipelineEnabled ? geometryMeshletPipeline : geometryPipeline,
						.viewport = {
							.offset = { 0.0f, 0.0f },
							.extent = { swapchain.extent.width, swapchain.extent.height }},
						.scissor = {
							.offset = { 0, 0 },
							.extent = { swapchain.extent.width, swapchain.extent.height }},
						.colorAttachments = {{
							.texture = swapchain.textures[currentSwapchainImageIndex],
							.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
							.clear = { { 34.0f / 255.0f, 34.0f / 255.0f, 29.0f / 255.0f, 1.0f } } }},
						.depthStencilAttachment = {
							.texture = depthTexture,
							.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
							.clear = { 0.0f, 0 } },
						.bindings = bMeshShadingPipelineEnabled ?
							Bindings({
								drawBuffers.drawsBuffer,
								drawBuffers.drawCommandsBuffer,
								geometryBuffers.meshletBuffer,
								geometryBuffers.meshesBuffer,
								geometryBuffers.meshletVerticesBuffer,
								geometryBuffers.meshletTrianglesBuffer,
								geometryBuffers.vertexBuffer }) :
							Bindings({
								geometryBuffers.vertexBuffer,
								drawBuffers.drawsBuffer,
								drawBuffers.drawCommandsBuffer }),
						.pushConstants = {
							.size = sizeof(perFrameData),
							.pData = &perFrameData } },
							[&]()
						{
							if (bMeshShadingPipelineEnabled)
							{
								vkCmdDrawMeshTasksIndirectCountNV(commandBuffer, drawBuffers.drawCommandsBuffer.resource,
									offsetof(DrawCommand, taskCount), drawBuffers.drawCountBuffer.resource, 0, kMaxDrawCount, sizeof(DrawCommand));
							}
							else
							{
								vkCmdBindIndexBuffer(commandBuffer, geometryBuffers.indexBuffer.resource, 0, VK_INDEX_TYPE_UINT32);

								vkCmdDrawIndexedIndirectCount(commandBuffer, drawBuffers.drawCommandsBuffer.resource,
									offsetof(DrawCommand, indexCount), drawBuffers.drawCountBuffer.resource, 0, kMaxDrawCount, sizeof(DrawCommand));
							}
						});

					gui::drawFrame(commandBuffer, frameIndex, swapchain.textures[currentSwapchainImageIndex]);

					textureBarrier(commandBuffer, swapchain.textures[currentSwapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
						VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
				}
			}

			VK_CALL(vkEndCommandBuffer(commandBuffer));
		}

		submitAndPresent(commandBuffer, device, swapchain, currentSwapchainImageIndex, framePacingState);

		updateQueryPoolResults(device, timestampsQueryPool);
		updateQueryPoolResults(device, statisticsQueryPool);

		GPU_BLOCK_RESULT(timestampsQueryPool, GenerateDraws, physicalDeviceProperties.limits, guiState.generateDrawsGpuTime);
		GPU_BLOCK_RESULT(timestampsQueryPool, Geometry, physicalDeviceProperties.limits, guiState.geometryGpuTime);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyVertices, guiState.inputAssemblyVertices);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::InputAssemblyPrimitives, guiState.inputAssemblyPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::VertexShaderInvocations, guiState.vertexShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingInvocations, guiState.clippingInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ClippingPrimitives, guiState.clippingPrimitives);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::FragmentShaderInvocations, guiState.fragmentShaderInvocations);
		GPU_STATS_RESULT(statisticsQueryPool, Main, StatType::ComputeShaderInvocations, guiState.computeShaderInvocations);

		frameIndex = (frameIndex + 1) % kMaxFramesInFlightCount;
	}

	{
		EASY_BLOCK("DeviceWaitIdle");
		VK_CALL(vkDeviceWaitIdle(device.device));
	}

	{
		EASY_BLOCK("Cleanup");

		gui::terminate();

		destroyQueryPool(device, timestampsQueryPool);
		destroyQueryPool(device, statisticsQueryPool);

		for (FramePacingState& framePacingState : framePacingStates)
		{
			destroyFramePacingState(device, framePacingState);
		}

		{
			if (device.bMeshShadingPipelineAllowed)
			{
				destroyBuffer(device, geometryBuffers.meshletBuffer);
				destroyBuffer(device, geometryBuffers.meshletVerticesBuffer);
				destroyBuffer(device, geometryBuffers.meshletTrianglesBuffer);
			}

			destroyBuffer(device, geometryBuffers.vertexBuffer);
			destroyBuffer(device, geometryBuffers.indexBuffer);
			destroyBuffer(device, geometryBuffers.meshesBuffer);
		}

		{
			destroyBuffer(device, drawBuffers.drawsBuffer);
			destroyBuffer(device, drawBuffers.drawCommandsBuffer);
			destroyBuffer(device, drawBuffers.drawCountBuffer);
		}

		if (device.bMeshShadingPipelineAllowed)
		{
			destroyPipeline(device, geometryMeshletPipeline);
		}

		destroyPipeline(device, geometryPipeline);
		destroyPipeline(device, generateDrawsPipeline);

		destroyTexture(device, depthTexture);
		destroySwapchain(device, swapchain);
		destroyDevice(device);
		destroyWindow(pWindow);
	}

	{
		const char* profileCaptureFileName = "cpu_profile_capture.prof";
		profiler::dumpBlocksToFile(profileCaptureFileName);
		printf("CPU profile capture saved to %s file.\n", profileCaptureFileName);
	}

	return 0;
}
