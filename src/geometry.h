#pragma once

struct Vertex
{
	u16 position[3];
	u8 normal[4];
	u16 texCoord[2];
};

struct Meshlet
{
	u32 vertexOffset;
	u32 triangleOffset;
	u32 vertexCount;
	u32 triangleCount;

	u32 groupIndex; // TODO-MILKRU: Temp

	f32 center[3];
	f32 radius;
	i8 coneAxis[3];
	i8 coneCutoff;
};

struct MeshLod
{
	u32 indexCount;
	u32 firstIndex;
	u32 meshletOffset;
	u32 meshletCount;
};

struct LodDagNode
{
	u32 meshletIndex;

	f32 simplifyError;
	f32 simplifyParentError;
};

struct Mesh
{
	u32 vertexOffset;

	f32 center[3];
	f32 radius;

	u32 lodCount;
	MeshLod lods[kMaxMeshLods];

	u32 lodDagNodeOffset;
	u32 lodDagNodeCount;
};

struct Geometry
{
	std::vector<Meshlet> meshlets;
	std::vector<u32> meshletVertices;
	std::vector<u8> meshletTriangles;
	std::vector<LodDagNode> lodDagNodes;

	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	std::vector<Mesh> meshes;
};

struct GeometryBuffers
{
	Buffer meshletBuffer{};
	Buffer meshletVerticesBuffer{};
	Buffer meshletTrianglesBuffer{};
	Buffer vertexBuffer{};
	Buffer indexBuffer{};
	Buffer meshesBuffer{};
	Buffer lodDagNodesBuffer{};
};

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	u32 _meshCount,
	const char** _ppMeshPaths);

// TODO-MIKRU: List:
// make sure GPU structures are the same as CPU ones
