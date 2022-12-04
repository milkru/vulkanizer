#pragma once

struct alignas(16) PerDrawData
{
	m4 model{};
	u32 meshIndex = 0u;
};

struct DrawCommand
{
	u32 indexCount = 0u;
	u32 instanceCount = 0u;
	u32 firstIndex = 0u;
	u32 vertexOffset = 0u;
	u32 firstInstance = 0u;

	u32 taskCount = 0u;
	u32 firstTask = 0u;

	u32 drawIndex = 0u;
	u32 lodIndex = 0u;
};

struct DrawBuffers
{
	Buffer drawsBuffer{};
	Buffer drawCommandsBuffer{};
	Buffer drawCountBuffer{};
	Buffer visibilityBuffer{};
};

DrawBuffers createDrawBuffers(
	Device& _rDevice,
	u32 _meshCount,
	u32 _maxDrawCount,
	u32 _spawnCubeSize);
