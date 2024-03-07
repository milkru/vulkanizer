#include "core/device.h"
#include "core/buffer.h"

#include "shaders/shader_interop.h"
#include "geometry.h"

#include <fast_obj.h>
#include <meshoptimizer.h>

// TODO-MILKRU: Implement occlusion culling freeze for the book visualization
// TODO-MILKRU: Implement meshlet occlusion culling
// TODO-MILKRU: New mesh shading pipeline and test RenderDoc
// TODO-MILKRU: Delete old pipeline

struct RawVertex
{
	f32 position[3] = {};
	f32 normal[3] = {};
	f32 texCoord[2] = {};

#ifdef VERTEX_COLOR
	f32 color[3] = {};
#endif // VERTEX_COLOR
};

// TODO-MILKRU: Implement a more conservative way of calculating bounding sphere
static v4 calculateMeshBounds(
	std::vector<RawVertex>& _rVertices)
{
	v4 meshBounds(0.0f);
	for (RawVertex& rVertex : _rVertices)
	{
		meshBounds += v4(rVertex.position[0], rVertex.position[1], rVertex.position[2], 0.0f);
	}
	meshBounds /= f32(_rVertices.size());

	for (RawVertex& rVertex : _rVertices)
	{
		meshBounds.w = glm::max(meshBounds.w, glm::distance(v3(meshBounds),
			v3(rVertex.position[0], rVertex.position[1], rVertex.position[2])));
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

// TODO-MILKRU: All mesh preprocessing can be done offline in CMake for example.
void loadMesh(
	const char* _pFilePath,
	bool _bMeshShadingSupported,
	_Out_ Geometry& _rGeometry)
{
	fastObjMesh* pObjMesh = fast_obj_read(_pFilePath);
	assert(pObjMesh);

	// TODO-MILKRU: Objects not supported yet
	assert(pObjMesh->object_count == 1);

	Mesh mesh = { .subsetCount = pObjMesh->group_count };
	assert(mesh.subsetCount <= kMaxMeshSubsets);

	u32 globalVertexOffset = _rGeometry.vertices.size();
	u32 meshVertexCount = 0;

	std::vector<RawVertex> meshVertices;

	for (u32 subsetIndex = 0; subsetIndex < mesh.subsetCount; ++subsetIndex)
	{
		u32 indexOffset = pObjMesh->groups[subsetIndex].index_offset;
		u32 indexCount = subsetIndex == mesh.subsetCount - 1
			? pObjMesh->index_count - indexOffset
			: pObjMesh->groups[subsetIndex + 1].index_offset - indexOffset;

		std::vector<RawVertex> subsetVertices;
		subsetVertices.reserve(indexCount);

		for (u32 indexIterator = indexOffset; indexIterator < indexOffset + indexCount; ++indexIterator)
		{
			fastObjIndex vertexIndex = pObjMesh->indices[indexIterator];

			RawVertex vertex{};

			vertex.position[0] = pObjMesh->positions[3 * size_t(vertexIndex.p) + 0];
			vertex.position[1] = pObjMesh->positions[3 * size_t(vertexIndex.p) + 1];
			vertex.position[2] = pObjMesh->positions[3 * size_t(vertexIndex.p) + 2];

			vertex.normal[0] = 0.5f + 0.5f * pObjMesh->normals[3 * size_t(vertexIndex.n) + 0];
			vertex.normal[1] = 0.5f + 0.5f * pObjMesh->normals[3 * size_t(vertexIndex.n) + 1];
			vertex.normal[2] = 0.5f + 0.5f * pObjMesh->normals[3 * size_t(vertexIndex.n) + 2];

			vertex.texCoord[0] = pObjMesh->texcoords[2 * size_t(vertexIndex.t) + 0];
			vertex.texCoord[1] = pObjMesh->texcoords[2 * size_t(vertexIndex.t) + 1];

			subsetVertices.push_back(vertex);
		}

		std::vector<u32> remapTable(indexCount);
		size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, indexCount,
			subsetVertices.data(), subsetVertices.size(), sizeof(RawVertex));

		size_t originalVertexCount = subsetVertices.size();
		subsetVertices.resize(vertexCount);
		meshVertexCount += vertexCount;

		std::vector<u32> indices(indexCount);

		meshopt_remapVertexBuffer(subsetVertices.data(), subsetVertices.data(), originalVertexCount, sizeof(RawVertex), remapTable.data());
		meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), subsetVertices.size());
		meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &subsetVertices[0].position[0], subsetVertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
		meshopt_optimizeVertexFetch(subsetVertices.data(), indices.data(), indices.size(), subsetVertices.data(), subsetVertices.size(), sizeof(RawVertex));

		MeshSubset& rSubset = mesh.subsets[subsetIndex];

		rSubset.vertexOffset = u32(_rGeometry.vertices.size());
		_rGeometry.vertices.reserve(_rGeometry.vertices.size() + subsetVertices.size());
		meshVertices.insert(meshVertices.end(), subsetVertices.begin(), subsetVertices.end());

		for (RawVertex& rVertex : subsetVertices)
		{
			_rGeometry.vertices.push_back(quantizeVertex(rVertex));
		}

		rSubset.lodCount = 0;

		for (u32 lodIndex = 0; lodIndex < kMaxMeshLods; ++lodIndex)
		{
			rSubset.lods[lodIndex].firstIndex = u32(_rGeometry.indices.size());
			rSubset.lods[lodIndex].indexCount = u32(indices.size());
			_rGeometry.indices.insert(_rGeometry.indices.end(), indices.begin(), indices.end());

			if (_bMeshShadingSupported)
			{
				size_t maxMeshlets = meshopt_buildMeshletsBound(indices.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
				std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
				std::vector<u32> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
				std::vector<u8> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);

				// TODO-MILKRU: After per-meshlet frustum/occlusion culling gets implemented, try playing around with cone_weight. You might get better performance.
				size_t meshletCount = meshopt_buildMeshlets(meshlets.data(), meshletVertices.data(), meshletTriangles.data(), indices.data(), indices.size(),
					&subsetVertices[0].position[0], subsetVertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.7f);

				meshopt_Meshlet& rLastMeshlet = meshlets[meshletCount - 1];

				meshletVertices.resize(rLastMeshlet.vertex_offset + size_t(rLastMeshlet.vertex_count));
				meshletTriangles.resize(rLastMeshlet.triangle_offset + ((size_t(rLastMeshlet.triangle_count) * 3 + 3) & ~3));
				meshlets.resize(meshletCount);

				rSubset.lods[lodIndex].meshletOffset = _rGeometry.meshlets.size();
				rSubset.lods[lodIndex].meshletCount = meshletCount;

				u32 globalMeshletVerticesOffset = _rGeometry.meshletVertices.size();
				u32 globalMeshletTrianglesOffset = _rGeometry.meshletTriangles.size();

				_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
				_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());
				_rGeometry.meshlets.reserve(_rGeometry.meshlets.size() + meshletCount);

				for (u32 meshletIndex = 0; meshletIndex < meshlets.size(); ++meshletIndex)
				{
					meshopt_Meshlet& rMeshlet = meshlets[meshletIndex];
					meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshletVertices[rMeshlet.vertex_offset], &meshletTriangles[rMeshlet.triangle_offset],
						rMeshlet.triangle_count, &subsetVertices[0].position[0], subsetVertices.size(), sizeof(RawVertex));

					rMeshlet.vertex_offset += globalMeshletVerticesOffset;
					rMeshlet.triangle_offset += globalMeshletTrianglesOffset;

					_rGeometry.meshlets.push_back(buildMeshlet(rMeshlet, bounds));
				}
			}

			++rSubset.lodCount;

			if (lodIndex >= kMaxMeshLods - 1)
			{
				break;
			}

			f32 threshold = 0.6f;
			size_t targetIndexCount = size_t(indices.size() * threshold);
			f32 targetError = 1e-2f;

			size_t newIndexCount = meshopt_simplify(indices.data(), indices.data(), indices.size(),
				&subsetVertices[0].position[0], subsetVertices.size(), sizeof(RawVertex), targetIndexCount, targetError);

			if (indices.size() == newIndexCount)
			{
				break;
			}

			indices.resize(newIndexCount);
			meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), subsetVertices.size());
		}
	}

#if 0
	if (_bMeshShadingSupported)
	{
		auto meshlets = generateMeshlets(mesh);

		// and put them as leaf nodes in the dag
		dag.add_leafs(meshlets);

		// iteratively merge-simplify-split
		while (we can group meshlets)
		{
			for (group in partition(meshlets))
			{
				// merge the meshlets in the group
				auto meshlet = merge(group);

				// track all the borders between the meshlets
				find_borders(meshlets);

				// simplify the merged meshlet
				simplify(meshlet);

				// split the simplified meshlet
				auto parts = split(meshlet);

				// write the result to the dag
				dag.add_parents(group, parts);
			}
		}
}
#endif

	v4 meshBounds = calculateMeshBounds(meshVertices);

	mesh.center[0] = meshBounds.x;
	mesh.center[1] = meshBounds.y;
	mesh.center[2] = meshBounds.z;
	mesh.radius = meshBounds.w;

	_rGeometry.meshes.push_back(mesh);
}

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	Geometry& _rGeometry,
	u32 _meshCount,
	const char** _meshPaths)
{
	EASY_BLOCK("InitializeGeometryBuffers");

	return {
		.meshletBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(Meshlet) * _rGeometry.meshlets.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = _rGeometry.meshlets.data() }) : Buffer(),

		.meshletVerticesBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(u32) * _rGeometry.meshletVertices.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = _rGeometry.meshletVertices.data() }) : Buffer(),

		.meshletTrianglesBuffer = _rDevice.bMeshShadingPipelineAllowed ?
			createBuffer(_rDevice, {
				.byteSize = sizeof(u8) * _rGeometry.meshletTriangles.size(),
				.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.pContents = _rGeometry.meshletTriangles.data() }) : Buffer(),

		.vertexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Vertex) * _rGeometry.vertices.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.vertices.data() }),

		.indexBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(u32) * _rGeometry.indices.size(),
			.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			.pContents = _rGeometry.indices.data() }),

		.meshesBuffer = createBuffer(_rDevice, {
			.byteSize = sizeof(Mesh) * _rGeometry.meshes.size(),
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.pContents = _rGeometry.meshes.data() }) };
}
