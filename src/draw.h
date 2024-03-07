#pragma once

struct alignas(16) PerDrawData
{
	m4 model{};
	u32 meshIndex = 0;
};

struct DrawCommand
{
	u32 indexCount = 0;
	u32 instanceCount = 0;
	u32 firstIndex = 0;
	u32 vertexOffset = 0;
	u32 firstInstance = 0;

	u32 taskCount = 0;
	u32 firstTask = 0;

	u32 drawIndex = 0;
	u32 subsetIndex = 0;
	u32 lodIndex = 0;
};

struct DrawBuffers
{
	u32 maxDrawCommandCount = 0;
	Buffer drawsBuffer{};
	Buffer drawCommandsBuffer{};
	Buffer drawCountBuffer{};
	Buffer visibilityBuffer{};
};

DrawBuffers createDrawBuffers(
	Device& _rDevice,
	Geometry& _rGeometry,
	u32 _meshCount,
	u32 _maxDrawCount,
	u32 _spawnCubeSize);
