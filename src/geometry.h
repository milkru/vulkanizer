#pragma once

struct Vertex
{
	u16 position[3] = {};
	u8 normal[4] = {};
	u16 texCoord[2] = {};

#ifdef VERTEX_COLOR
	u16 color[3] = {};
#endif // VERTEX_COLOR
};

struct Meshlet
{
	u32 vertexOffset = 0;
	u32 triangleOffset = 0;
	u32 vertexCount = 0;
	u32 triangleCount = 0;

	f32 center[3] = {};
	f32 radius = 0.0f;
	i8 coneAxis[3] = {};
	i8 coneCutoff = 0;
};

struct MeshLod
{
	u32 indexCount = 0;
	u32 firstIndex = 0;
	u32 meshletOffset = 0;
	u32 meshletCount = 0;
};

struct MeshSubset
{
	u32 vertexOffset = 0;
	u32 lodCount = 0;
	MeshLod lods[kMaxMeshLods] = {};
};

struct Mesh
{
	f32 center[3] = {};
	f32 radius = 0.0f;
	u32 subsetCount = 0;
	MeshSubset subsets[kMaxMeshSubsets] = {};
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

void loadMesh(
	const char* _pFilePath,
	bool _bMeshShadingSupported,
	_Out_ Geometry& _rGeometry);

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	Geometry& _rGeometry,
	u32 _meshCount,
	const char** _meshPaths);
