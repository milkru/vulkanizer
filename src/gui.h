#pragma once

namespace gui
{
	struct State
	{
		const char* deviceName = "Unknown Device";
		double generateDrawsGpuTime = 0.0f;
		double geometryGpuTime = 0.0f;
		uint64_t inputAssemblyVertices = 0ull;
		uint64_t inputAssemblyPrimitives = 0ull;
		uint64_t vertexShaderInvocations = 0ull;
		uint64_t clippingInvocations = 0ull;
		uint64_t clippingPrimitives = 0ull;
		uint64_t fragmentShaderInvocations = 0ull;
		uint64_t computeShaderInvocations = 0ull;
		int32_t forcedLod = 0;
		bool bForceMeshLodEnabled = false;
		bool bFreezeCameraEnabled = false;
		bool bMeshShadingPipelineSupported = false;
		bool bMeshShadingPipelineEnabled = false;
		bool bMeshFrustumCullingEnabled = false;
		bool bMeshletConeCullingEnabled = false;
		bool bMeshletFrustumCullingEnabled = false;
	};

	void initialize(
		Device _device,
		VkFormat _colorFormat,
		VkFormat _depthFormat,
		float _width,
		float _height);

	void terminate();

	void newFrame(
		GLFWwindow* _pWindow,
		State& _rState);

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		uint32_t _frameIndex,
		Texture _attachment);
}
