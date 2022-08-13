#pragma once

void initializeGUI(
	Device _device,
	VkFormat _colorFormat,
	VkFormat _depthFormat,
	float _width,
	float _height);

void terminateGUI();

struct InfoGUI
{
	const char* deviceName = "Unknown Device";
	double gpuTime = 0.0f;
	uint64_t inputAssemblyVertices = 0ull;
	uint64_t inputAssemblyPrimitives = 0ull;
	uint64_t vertexShaderInvocations = 0ull;
	uint64_t clippingInvocations = 0ull;
	uint64_t clippingPrimitives = 0ull;
	uint64_t fragmentShaderInvocations = 0ull;
	uint64_t computeShaderInvocations = 0ull;
	bool bMeshShadingPipelineSupported = false;
	bool bMeshShadingPipelineEnabled = false;
};

void newFrameGUI(
	GLFWwindow* _pWindow,
	InfoGUI& _rInfo);

void drawFrameGUI(
	VkCommandBuffer _commandBuffer,
	uint32_t _frameIndex);
