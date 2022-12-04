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

struct Mesh
{
	u32 vertexOffset;
	f32 center[3];
	f32 radius;
	u32 lodCount;
	MeshLod lods[kMaxMeshLods];
};

struct Geometry
{
	std::vector<Meshlet> meshlets;
	std::vector<u32> meshletVertices;
	std::vector<u8> meshletTriangles;

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
};

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	u32 _meshCount,
	const char** _meshPaths);
