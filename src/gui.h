#pragma once

// TODO-MILKRU: Should this stay here?
struct Settings
{
	const char* deviceName = "Unknown Device";
	std::map<std::string, f64> gpuTimes;
	u64 inputAssemblyVertices = 0;
	u64 inputAssemblyPrimitives = 0;
	u64 vertexShaderInvocations = 0;
	u64 clippingInvocations = 0;
	u64 clippingPrimitives = 0;
	u64 fragmentShaderInvocations = 0;
	u64 computeShaderInvocations = 0;
	i32 forcedLod = 0;
	bool bForceMeshLodEnabled = false;
	bool bFreezeCameraEnabled = false;
	bool bMeshShadingPipelineSupported = false;
	bool bMeshShadingPipelineEnabled = false;
	bool bMeshFrustumCullingEnabled = false;
	bool bMeshOcclusionCullingEnabled = false;
	bool bMeshletConeCullingEnabled = false;
	bool bMeshletFrustumCullingEnabled = false;
};

namespace gui
{
	void initialize(
		Device& _rDevice,
		VkFormat _colorFormat,
		VkFormat _depthFormat,
		f32 _width,
		f32 _height);

	void terminate();

	void newFrame(
		GLFWwindow* _pWindow,
		Settings& _rSettings);

	void drawFrame(
		VkCommandBuffer _commandBuffer,
		u32 _frameIndex,
		Texture& _rAttachment);

	void updateGpuPerformanceState(
		VkPhysicalDeviceLimits _deviceLimits,
		Settings& _rSettings);
}
