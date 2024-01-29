#include "core/device.h"
#include "core/buffer.h"

#include "shaders/shader_interop.h"
#include "geometry.h"

#include <fast_obj.h>
#include <meshoptimizer.h>

// TODO-MILKRU: Implement occlusion culling freeze for the book visualization

struct RawVertex
{
	f32 position[3];
	f32 normal[3];
	f32 texCoord[2];

#ifdef VERTEX_COLOR
	f32 color[3];
#endif // VERTEX_COLOR
};

// TODO-MILKRU: Implement a more conservative way of calculating bounding sphere?
static v4 calculateMeshBounds(
	std::vector<RawVertex>& _rVertices)
{
	v4 meshBounds(0.0f);
	for (RawVertex& vertex : _rVertices)
	{
		meshBounds += v4(vertex.position[0], vertex.position[1], vertex.position[2], 0.0f);
	}
	meshBounds /= f32(_rVertices.size());

	for (RawVertex& vertex : _rVertices)
	{
		meshBounds.w = glm::max(meshBounds.w, glm::distance(v3(meshBounds),
			v3(vertex.position[0], vertex.position[1], vertex.position[2])));
	}

	return meshBounds;
}

static Vertex quantizeVertex(
	RawVertex& _rRawVertex)
{
	Vertex vertex{};

	// TODO-MILKRU: To snorm.
	vertex.position[0] = meshopt_quantizeHalf(_rRawVertex.position[0]);
	vertex.position[1] = meshopt_quantizeHalf(_rRawVertex.position[1]);
	vertex.position[2] = meshopt_quantizeHalf(_rRawVertex.position[2]);

	vertex.normal[0] = u8(meshopt_quantizeUnorm(_rRawVertex.normal[0], 8));
	vertex.normal[1] = u8(meshopt_quantizeUnorm(_rRawVertex.normal[1], 8));
	vertex.normal[2] = u8(meshopt_quantizeUnorm(_rRawVertex.normal[2], 8));

	// TODO-MILKRU: To unorm.
	vertex.texCoord[0] = meshopt_quantizeHalf(_rRawVertex.texCoord[0]);
	vertex.texCoord[1] = meshopt_quantizeHalf(_rRawVertex.texCoord[1]);

#ifdef VERTEX_COLOR
	vertex.color[0] = meshopt_quantizeHalf(1.0f);
	vertex.color[1] = meshopt_quantizeHalf(0.0f);
	vertex.color[2] = meshopt_quantizeHalf(1.0f);
#endif // VERTEX_COLOR

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

static void loadMesh(
	Geometry& _rGeometry,
	const char* _pFilePath,
	bool _bMeshShadingSupported)
{
	fastObjMesh* objMesh = fast_obj_read(_pFilePath);
	assert(objMesh);

	std::vector<RawVertex> vertices;
	vertices.reserve(objMesh->index_count);

	for (u32 i = 0; i < objMesh->index_count; ++i)
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
		vertex.normal[2] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 2];

		vertex.texCoord[0] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 0];
		vertex.texCoord[1] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 1];

		vertices.push_back(vertex);
	}

	// TODO-MILKRU: All mesh preprocessing can be done offline in CMake for example.

	std::vector<u32> remapTable(objMesh->index_count);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, objMesh->index_count,
		vertices.data(), vertices.size(), sizeof(RawVertex));

	vertices.resize(vertexCount);
	std::vector<u32> indices(objMesh->index_count);

	meshopt_remapVertexBuffer(vertices.data(), vertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
	meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position[0], vertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
	meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(RawVertex));

	v4 meshBounds = calculateMeshBounds(vertices);

	Mesh mesh = {};
	mesh.center[0] = meshBounds.x;
	mesh.center[1] = meshBounds.y;
	mesh.center[2] = meshBounds.z;
	mesh.radius = meshBounds.w;

	mesh.vertexOffset = u32(_rGeometry.vertices.size());
	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + vertices.size());

	for (RawVertex& rVertex : vertices)
	{
		_rGeometry.vertices.push_back(quantizeVertex(rVertex));
	}

	mesh.lodCount = 0;

	for (u32 lodIndex = 0u; lodIndex < kMaxMeshLods; ++lodIndex)
	{
		mesh.lods[lodIndex].firstIndex = u32(_rGeometry.indices.size());
		mesh.lods[lodIndex].indexCount = u32(indices.size());
		_rGeometry.indices.insert(_rGeometry.indices.end(), indices.begin(), indices.end());

		if (_bMeshShadingSupported)
		{
			size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
			std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
			std::vector<u32> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
			std::vector<u8> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);

			// TODO-MILKRU: After per-meshlet frustum/occlusion culling gets implemented, try playing around with cone_weight. You might get better performance.
			size_t meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(), indices.data(), indices.size(),
				&vertices[0].position[0], vertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.7f);

			meshopt_Meshlet& rLastMeshlet = meshlets[meshletCount - 1];

			meshletVertices.resize(rLastMeshlet.vertex_offset + size_t(rLastMeshlet.vertex_count));
			meshletTriangles.resize(rLastMeshlet.triangle_offset + ((size_t(rLastMeshlet.triangle_count) * 3 + 3) & ~3));
			meshlets.resize(meshletCount);

			mesh.lods[lodIndex].meshletOffset = _rGeometry.meshlets.size();
			mesh.lods[lodIndex].meshletCount = meshletCount;

			u32 globalMeshletVerticesOffset = _rGeometry.meshletVertices.size();
			u32 globalMeshletTrianglesOffset = _rGeometry.meshletTriangles.size();

			_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
			_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());
			_rGeometry.meshlets.reserve(_rGeometry.meshlets.size() + meshletCount);

			for (u32 meshletIndex = 0; meshletIndex < meshlets.size(); ++meshletIndex)
			{
				meshopt_Meshlet& rMeshlet = meshlets[meshletIndex];
				meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[rMeshlet.vertex_offset], &meshletTriangles[rMeshlet.triangle_offset],
					rMeshlet.triangle_count, &vertices[0].position[0], vertices.size(), sizeof(RawVertex));

				rMeshlet.vertex_offset += globalMeshletVerticesOffset;
				rMeshlet.triangle_offset += globalMeshletTrianglesOffset;

				_rGeometry.meshlets.push_back(buildMeshlet(rMeshlet, bounds));
			}
		}

		++mesh.lodCount;

		if (lodIndex >= kMaxMeshLods - 1)
		{
			break;
		}

		f32 threshold = 0.6f;
		size_t targetIndexCount = size_t(indices.size() * threshold);
		f32 targetError = 1e-2f;

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

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	u32 _meshCount,
	const char** _meshPaths)
{
	EASY_BLOCK("InitializeGeometry");

	Geometry geometry{};

	for (u32 meshIndex = 0; meshIndex < _meshCount; ++meshIndex)
	{
		const char* meshPath = _meshPaths[meshIndex];
		loadMesh(geometry, meshPath, _rDevice.bMeshShadingPipelineAllowed);
	}

	return {
		.meshletBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(Meshlet) * geometry.meshlets.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = geometry.meshlets.data() }) : Buffer(),

		.meshletVerticesBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(u32) * geometry.meshletVertices.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = geometry.meshletVertices.data() }) : Buffer(),

		.meshletTrianglesBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(u8) * geometry.meshletTriangles.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = geometry.meshletTriangles.data() }) : Buffer(),

		.vertexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Vertex) * geometry.vertices.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = geometry.vertices.data() }),

		.indexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32) * geometry.indices.size(),
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			.pContents = geometry.indices.data() }),

		.meshesBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Mesh) * geometry.meshes.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = geometry.meshes.data() }) };
}
