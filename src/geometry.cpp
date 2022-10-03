#include "common.h"
#include "device.h"
#include "buffer.h"
#include "shaders/shader_constants.h"
#include "geometry.h"

#include <fast_obj.h>
#include <meshoptimizer.h>

struct RawVertex
{
	float position[3];
	float normal[3];
	float texCoord[2];
};

// TODO-MILKRU: Implement a more conservative way of calculating bounding sphere?
static glm::vec4 calculateMeshBounds(
	std::vector<RawVertex>& _rVertices)
{
	glm::vec4 meshBounds(0.0f);
	for (RawVertex& vertex : _rVertices)
	{
		meshBounds += glm::vec4(vertex.position[0], vertex.position[1], vertex.position[2], 0.0f);
	}
	meshBounds /= float(_rVertices.size());

	for (RawVertex& vertex : _rVertices)
	{
		meshBounds.w = glm::max(meshBounds.w, glm::distance(glm::vec3(meshBounds),
			glm::vec3(vertex.position[0], vertex.position[1], vertex.position[2])));
	}

	return meshBounds;
}

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

static Meshlet buildMeshlet(
	meshopt_Meshlet _meshlet,
	meshopt_Bounds _bounds)
{
	Meshlet meshlet{};

	meshlet.vertexOffset = _meshlet.vertex_offset;
	meshlet.triangleOffset = _meshlet.triangle_offset;
	meshlet.vertexCount = _meshlet.vertex_count;
	meshlet.triangleCount = _meshlet.triangle_count;
	
	meshlet.center[0] = _bounds.center[0];
	meshlet.center[1] = _bounds.center[1];
	meshlet.center[2] = _bounds.center[2];
	meshlet.radius = _bounds.radius;

	meshlet.coneAxis[0] = _bounds.cone_axis_s8[0];
	meshlet.coneAxis[1] = _bounds.cone_axis_s8[1];
	meshlet.coneAxis[2] = _bounds.cone_axis_s8[2];
	meshlet.coneCutoff = _bounds.cone_cutoff_s8;

	return meshlet;
}

void loadMesh(
	Geometry& _rGeometry,
	const char* _pFilePath,
	bool _bMeshShadingSupported)
{
	fastObjMesh* objMesh = fast_obj_read(_pFilePath);
	assert(objMesh);

	std::vector<RawVertex> vertices;
	vertices.reserve(objMesh->index_count);

	for (uint32_t i = 0; i < objMesh->index_count; ++i)
	{
		fastObjIndex vertexIndex = objMesh->indices[i];

		RawVertex vertex{};

		vertex.position[0] = objMesh->positions[3 * size_t(vertexIndex.p) + 0];
		vertex.position[1] = objMesh->positions[3 * size_t(vertexIndex.p) + 1];
		vertex.position[2] = objMesh->positions[3 * size_t(vertexIndex.p) + 2];

		// TODO-MILKRU: We can calculate normals from depth buffer after first geometry phase.
		// See Wicked engine article about this.
		vertex.normal[0] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 0];
		vertex.normal[1] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 1];

		vertex.texCoord[0] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 0];
		vertex.texCoord[1] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 1];

		vertices.push_back(vertex);
	}

	// TODO-MILKRU: All mesh preprocessing can be done offline in CMake for example.

	std::vector<uint32_t> remapTable(objMesh->index_count);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, objMesh->index_count,
		vertices.data(), vertices.size(), sizeof(RawVertex));

	vertices.resize(vertexCount);
	std::vector<uint32_t> indices(objMesh->index_count);

	meshopt_remapVertexBuffer(vertices.data(), vertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
	meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position[0], vertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
	meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(RawVertex));

	glm::vec4 meshBounds = calculateMeshBounds(vertices);

	Mesh mesh = {};
	mesh.center[0] = meshBounds.x;
	mesh.center[1] = meshBounds.y;
	mesh.center[2] = meshBounds.z;
	mesh.radius = meshBounds.w;

	mesh.vertexOffset = uint32_t(_rGeometry.vertices.size());
	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + vertices.size());

	for (const RawVertex& vertex : vertices)
	{
		_rGeometry.vertices.push_back(quantizeVertex(vertex));
	}

	mesh.lodCount = 0;

	for (uint32_t lodIndex = 0u; lodIndex < kMaxMeshLods; ++lodIndex)
	{
		mesh.lods[lodIndex].firstIndex = uint32_t(_rGeometry.indices.size());
		mesh.lods[lodIndex].indexCount = uint32_t(indices.size());
		_rGeometry.indices.insert(_rGeometry.indices.end(), indices.begin(), indices.end());

		if (_bMeshShadingSupported)
		{
			size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
			std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
			std::vector<uint32_t> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
			std::vector<uint8_t> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);

			// TODO-MILKRU: After per-meshlet frustum/occlusion culling gets implemented, try playing around with cone_weight. You might get better performance.
			size_t meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(), indices.data(), indices.size(),
				&vertices[0].position[0], vertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.7f);

			const meshopt_Meshlet& lastMeshlet = meshlets[meshletCount - 1];

			meshletVertices.resize(lastMeshlet.vertex_offset + size_t(lastMeshlet.vertex_count));
			meshletTriangles.resize(lastMeshlet.triangle_offset + ((size_t(lastMeshlet.triangle_count) * 3 + 3) & ~3));
			meshlets.resize(meshletCount);

			mesh.lods[lodIndex].meshletOffset = _rGeometry.meshlets.size();
			mesh.lods[lodIndex].meshletCount = meshletCount;

			uint32_t globalMeshletVerticesOffset = _rGeometry.meshletVertices.size();
			uint32_t globalMeshletTrianglesOffset = _rGeometry.meshletTriangles.size();

			_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
			_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());
			_rGeometry.meshlets.reserve(_rGeometry.meshlets.size() + meshletCount);

			for (uint32_t meshletIndex = 0; meshletIndex < meshlets.size(); ++meshletIndex)
			{
				meshopt_Meshlet& meshlet = meshlets[meshletIndex];
				meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset],
					meshlet.triangle_count, &vertices[0].position[0], vertices.size(), sizeof(RawVertex));

				meshlet.vertex_offset += globalMeshletVerticesOffset;
				meshlet.triangle_offset += globalMeshletTrianglesOffset;

				_rGeometry.meshlets.push_back(buildMeshlet(meshlet, bounds));
			}
		}

		++mesh.lodCount;

		if (lodIndex >= kMaxMeshLods - 1)
		{
			break;
		}

		float threshold = 0.6f;
		size_t targetIndexCount = size_t(indices.size() * threshold);
		float targetError = 1e-2f;

		size_t newIndexCount = meshopt_simplify(indices.data(), indices.data(), indices.size(),
			&vertices[0].position[0], vertices.size(), sizeof(RawVertex), targetIndexCount, targetError);

		if (indices.size() == newIndexCount)
		{
			break;
		}

		indices.resize(newIndexCount);
		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	}

	_rGeometry.meshes.push_back(mesh);
}
