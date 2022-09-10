#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

// A structure has a scalar alignment equal to the largest scalar alignment of any of its members.
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/chap15.html#interfaces-resources-layout
struct Vertex
{
	float16_t position[3];
	uint8_t normal[2];
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

// TODO-MILKRU: Separate PerFrameData and CullData.
struct PerFrameData
{
	// TODO-MILKRU: Move model to MeshBuffer once it gets created together with bounds etc.
	mat4 model;
	mat4 viewProjection;
	vec4 frustumPlanes[6];
	vec3 cameraPosition;
	uint maxDrawCount;
	uint enableMeshFrustumCulling;
	uint enableMeshletConeCulling;
	uint enableMeshletFrustumCulling;
};

#endif // SHADER_COMMON_H
