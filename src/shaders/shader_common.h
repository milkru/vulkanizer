#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

#include "shader_constants.h"

// A structure has a scalar alignment equal to the largest scalar alignment of any of its members.
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
struct Vertex
{
	float16_t position[3];
	uint8_t normal[4];
	float16_t texCoord[2];
};

struct Meshlet
{
	uint vertexOffset;
	uint triangleOffset;
	uint vertexCount;
	uint triangleCount;

	float center[3];
	float radius;

	int8_t coneAxis[3];
	int8_t coneCutoff;
};

struct PerFrameData
{
	mat4 viewProjection;
	vec4 frustumPlanes[6];
	vec3 cameraPosition;
	uint maxDrawCount;
	float lodTransitionBase;
	float lodTransitionStep;
	int forcedLod;
	uint enableMeshFrustumCulling;
	uint enableMeshletConeCulling;
	uint enableMeshletFrustumCulling;
};

struct MeshLod
{
	uint indexCount;
	uint firstIndex;

	uint meshletOffset;
	uint meshletCount;
};

struct Mesh
{
	uint vertexOffset;

	float center[3];
	float radius;

	uint lodCount;
	MeshLod lods[kMaxMeshLods];
};

struct PerDrawData
{
	mat4 model;
	uint meshIndex;
};

struct DrawCommand
{
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;

	uint taskCount;
	uint firstTask;

	uint drawIndex;
	uint lodIndex;
};

#endif // SHADER_COMMON_H
