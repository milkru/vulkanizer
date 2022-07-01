#include "common.h"
#include "mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <meshoptimizer.h>

Mesh loadMesh(
	const char* _pFilePath)
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

	Mesh mesh;
	mesh.vertices.reserve(indexCount);

	// Using various mesh optimizations from meshoptimizer.
	// https://github.com/zeux/meshoptimizer
	for (const auto& index : shapeMesh.indices)
	{
		Vertex vertex{};

		vertex.position[0] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 0]);
		vertex.position[1] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 1]);
		vertex.position[2] = meshopt_quantizeHalf(attrib.vertices[3 * index.vertex_index + 2]);

		// TODO-MILKRU: It's possible to compute normals in the mesh shader instead,
		// but the trade off might not be worth it.
		vertex.normal[0] = uint8_t(meshopt_quantizeUnorm(index.normal_index < 0 ? 0.0f :
			0.5f + 0.5f * attrib.normals[3 * index.normal_index + 0], 8));
		vertex.normal[1] = uint8_t(meshopt_quantizeUnorm(index.normal_index < 0 ? 0.0f :
			0.5f + 0.5f * attrib.normals[3 * index.normal_index + 1], 8));
		vertex.normal[2] = uint8_t(meshopt_quantizeUnorm(index.normal_index < 0 ? 0.0f :
			0.5f + 0.5f * attrib.normals[3 * index.normal_index + 2], 8));

		vertex.texCoord[0] = meshopt_quantizeHalf(index.texcoord_index < 0 ? 0.0f :
			attrib.texcoords[2 * index.texcoord_index + 0]);
		vertex.texCoord[1] = meshopt_quantizeHalf(index.texcoord_index < 0 ? 0.0f :
			1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);

		mesh.vertices.push_back(vertex);
	}

	std::vector<uint32_t> remapTable(indexCount);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, indexCount,
		mesh.vertices.data(), mesh.vertices.size(), sizeof(Vertex));

	mesh.vertices.resize(vertexCount);
	mesh.indices.resize(indexCount);

	meshopt_remapVertexBuffer(mesh.vertices.data(), mesh.vertices.data(), indexCount, sizeof(Vertex), remapTable.data());
	meshopt_remapIndexBuffer(mesh.indices.data(), nullptr, indexCount, remapTable.data());

	meshopt_optimizeVertexCache(mesh.indices.data(), mesh.indices.data(), indexCount, vertexCount);
	// TODO-MILKRU: meshopt_optimizeOverdraw does not support 16 bit vertex positions.
	meshopt_optimizeVertexFetch(mesh.vertices.data(), mesh.indices.data(), indexCount, mesh.vertices.data(), vertexCount, sizeof(Vertex));

	return mesh;
}
