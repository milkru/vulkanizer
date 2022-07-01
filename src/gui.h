#pragma once

void initializeGUI(
	VkPhysicalDevice _physicalDevice,
	VkDevice _device,
	VkQueue _copyQueue,
	VkCommandPool _commandPool,
	VkRenderPass _renderPass,
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
};

void newFrameGUI(
	GLFWwindow* _pWindow,
	InfoGUI _info);

void drawFrameGUI(
	VkCommandBuffer _commandBuffer,
	uint32_t _frameIndex);
