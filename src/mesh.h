#pragma once

struct Vertex
{
	uint16_t position[3];
	uint8_t normal[2];
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

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Meshlet> meshlets;
	std::vector<uint32_t> meshletVertices;
	std::vector<uint8_t> meshletTriangles;
};

Mesh loadMesh(
	const char* _pFilePath,
	bool _bMeshShadingSupported);
