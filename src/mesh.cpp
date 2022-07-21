#include "common.h"
#include "mesh.h"
#include "shaders/constants.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

struct RawVertex
{
	float position[3];
	float normal[3];
	float texCoord[2];
};

static Vertex quantizeVertex(
	const RawVertex& _rawVertex)
{
	Vertex vertex{};

	// TODO-MILKRU: To snorm.
	vertex.position[0] = meshopt_quantizeHalf(_rawVertex.position[0]);
	vertex.position[1] = meshopt_quantizeHalf(_rawVertex.position[1]);
	vertex.position[2] = meshopt_quantizeHalf(_rawVertex.position[2]);

	vertex.normal[0] = uint8_t(meshopt_quantizeUnorm(_rawVertex.normal[0], 8));
	vertex.normal[1] = uint8_t(meshopt_quantizeUnorm(_rawVertex.normal[1], 8));

	// TODO-MILKRU: To unorm.
	vertex.texCoord[0] = meshopt_quantizeHalf(_rawVertex.texCoord[0]);
	vertex.texCoord[1] = meshopt_quantizeHalf(_rawVertex.texCoord[1]);

	return vertex;
}

Mesh loadMesh(
	const char* _pFilePath,
	bool _bMeshShadingSupported)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warning, error;

	{
		bool bObjLoaded = tinyobj::LoadObj(&attrib, &shapes, &materials, &warning, &error, _pFilePath);
		assert(bObjLoaded);
	}

	// TODO-MILKRU: Meshes with multiple sections (shapes) are not supported yet.
	assert(shapes.size() == 1);

	tinyobj::mesh_t& shapeMesh = shapes[0].mesh;
	size_t indexCount = shapeMesh.indices.size();

	std::vector<RawVertex> vertices;
	vertices.reserve(indexCount);

	// Using various mesh optimizations from meshoptimizer.
	// https://github.com/zeux/meshoptimizer
	for (const tinyobj::index_t& index : shapeMesh.indices)
	{
		RawVertex vertex{};

		vertex.position[0] = attrib.vertices[3 * size_t(index.vertex_index) + 0];
		vertex.position[1] = attrib.vertices[3 * size_t(index.vertex_index) + 1];
		vertex.position[2] = attrib.vertices[3 * size_t(index.vertex_index) + 2];

		// TODO-MILKRU: It's possible to compute normals in the mesh shader instead,
		// but the trade off might not be worth it. This would mean that we wouldn't need to store normals at all.
		vertex.normal[0] = index.normal_index < 0 ? 0.0f : 0.5f + 0.5f * attrib.normals[3 * size_t(index.normal_index) + 0];
		vertex.normal[1] = index.normal_index < 0 ? 0.0f : 0.5f + 0.5f * attrib.normals[3 * size_t(index.normal_index) + 1];

		vertex.texCoord[0] = index.texcoord_index < 0 ? 0.0f : attrib.texcoords[2 * size_t(index.texcoord_index) + 0];
		vertex.texCoord[1] = index.texcoord_index < 0 ? 0.0f : 1.0f - attrib.texcoords[2 * size_t(index.texcoord_index) + 1];

		vertices.push_back(vertex);
	}

	std::vector<uint32_t> remapTable(indexCount);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, indexCount,
		vertices.data(), vertices.size(), sizeof(RawVertex));

	vertices.resize(vertexCount);
	std::vector<uint32_t> indices(indexCount);

	meshopt_remapVertexBuffer(vertices.data(), vertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
	meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position[0], vertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
	meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(RawVertex));

	size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
	std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
	std::vector<uint32_t> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
	std::vector<uint8_t> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);

	size_t meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(), indices.data(), indices.size(),
		&vertices[0].position[0], vertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.0f);

	const meshopt_Meshlet& lastMeshlet = meshlets[meshletCount - 1];

	meshletVertices.resize(lastMeshlet.vertex_offset + size_t(lastMeshlet.vertex_count));
	meshletTriangles.resize(lastMeshlet.triangle_offset + ((size_t(lastMeshlet.triangle_count) * 3 + 3) & ~3));
	meshlets.resize(meshletCount);

	Mesh mesh{};
	{
		mesh.indices = indices;
		mesh.vertices.reserve(vertices.size());

		for (const RawVertex& vertex : vertices)
		{
			mesh.vertices.push_back(quantizeVertex(vertex));
		}

		mesh.meshlets = meshlets;
		mesh.meshletVertices = meshletVertices;
		mesh.meshletTriangles = meshletTriangles;
	}

	return mesh;
}
