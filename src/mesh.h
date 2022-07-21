#pragma once

struct Vertex
{
	uint16_t position[3];
	uint8_t normal[2];
	uint16_t texCoord[2];
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<meshopt_Meshlet> meshlets;
	std::vector<uint32_t> meshletVertices;
	std::vector<uint8_t> meshletTriangles;
};

Mesh loadMesh(
	const char* _pFilePath,
	bool _bMeshShadingSupported);
