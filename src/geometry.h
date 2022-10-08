#pragma once

struct Vertex
{
	uint16_t position[3];
	uint8_t normal[4];
	uint16_t texCoord[2];
};

struct Meshlet
{
	uint32_t vertexOffset;
	uint32_t triangleOffset;
	uint32_t vertexCount;
	uint32_t triangleCount;

	float center[3];
	float radius;

	int8_t coneAxis[3];
	int8_t coneCutoff;
};

struct MeshLod
{
	uint32_t indexCount;
	uint32_t firstIndex;

	uint32_t meshletOffset;
	uint32_t meshletCount;
};

struct Mesh
{
	uint32_t vertexOffset;

	float center[3];
	float radius;

	uint32_t lodCount;
	MeshLod lods[kMaxMeshLods];
};

struct Geometry
{
	std::vector<Meshlet> meshlets;
	std::vector<uint32_t> meshletVertices;
	std::vector<uint8_t> meshletTriangles;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

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
	Geometry& _rGeometry,
	const char* _pFilePath,
	bool _bMeshShadingSupported);
