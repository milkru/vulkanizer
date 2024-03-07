#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

#include "shader_interop.h"

// TODO-MILKRU: Move these structs to interop as well

// A structure has a scalar alignment equal to the largest scalar alignment of any of its members.
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
struct Vertex
{
	float16_t position[3];
	uint8_t normal[4];
	float16_t texCoord[2];

#ifdef VERTEX_COLOR
	float16_t color[3];
#endif // VERTEX_COLOR
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
	mat4 view;
	mat4 projection;
	vec4 frustumPlanes[kFrustumPlaneCount];
	vec3 cameraPosition;
	uint maxDrawCount;
	float lodTransitionBase;
	float lodTransitionStep;
	int forcedLod;
	uint hzbSize;
	int8_t bPrepass;
	int8_t bEnableMeshFrustumCulling;
	int8_t bEnableMeshOcclusionCulling;
	int8_t bEnableMeshletConeCulling;
	int8_t bEnableMeshletFrustumCulling;

	int padding0;
	int padding1;
};

struct MeshLod
{
	uint indexCount;
	uint firstIndex;
	uint meshletOffset;
	uint meshletCount;
};

struct MeshSubset
{
	uint vertexOffset;
	uint lodCount;
	MeshLod lods[kMaxMeshLods];
};

struct Mesh
{
	float center[3];
	float radius;
	uint subsetCount;
	MeshSubset subsets[kMaxMeshSubsets];
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
	uint subsetIndex;
	uint lodIndex;
};

#endif // SHADER_COMMON_H
