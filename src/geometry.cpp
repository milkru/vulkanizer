#include "core/device.h"
#include "core/buffer.h"

#include "shaders/shader_constants.h"
#include "geometry.h"

#include <fast_obj.h>
#include <meshoptimizer.h>
#include <metis.h>

// TODO-MILKRU: Add _Out_ everywhere

// TODO-MILKRU: Integrate https://github.com/wolfpld/tracy

// TODO-MILKRU: meshopt supports compression/decompression. Useful for cooking. Also EXT_meshopt_compression exists

// TODO-MILKRU: Implement vector_span data structure
// TODO-MILKRU: Reorder all functions here

const u32 kTargetGroupSize = 20;

struct RawVertex
{
	f32 position[3];
	f32 normal[3];
	f32 texCoord[2];
};

static Meshlet buildMeshlet(
	meshopt_Meshlet _rawMeshlet,
	meshopt_Bounds _bounds)
{
	Meshlet meshlet{};

	meshlet.vertexOffset = _rawMeshlet.vertex_offset;
	meshlet.triangleOffset = _rawMeshlet.triangle_offset;
	meshlet.vertexCount = _rawMeshlet.vertex_count;
	meshlet.triangleCount = _rawMeshlet.triangle_count;

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

// TODO-MILKRU: Temp namespace name
namespace pgd
{
	// TODO-MILKRU: Do we really need a triangle edge struct? Can we use triangle instead?
	typedef std::pair<u32, u32> TriangleEdge;
	typedef std::array<TriangleEdge, 3> TriangleEdges;
	typedef std::vector<TriangleEdges> MeshletTriangleEdges;
	typedef std::vector<TriangleEdge> MeshletBoundary;

	static bool isSharedEdge(
		TriangleEdge& _rFirst,
		TriangleEdge& _rSecond)
	{
		if (_rFirst.first == _rSecond.first &&
			_rFirst.second == _rSecond.second)
		{
			return true;
		}

		if (_rFirst.first == _rSecond.second &&
			_rFirst.second == _rSecond.first)
		{
			return true;
		}

		return false;
	}

	static u32 getSharedMeshletEdgeCount(
		MeshletBoundary& _rFirst,
		MeshletBoundary& _rSecond)
	{
		u32 sharedEdgeCount = 0;

		for (u32 firstIndex = 0; firstIndex < _rFirst.size(); ++firstIndex)
		{
			for (u32 secondIndex = 0; secondIndex < _rSecond.size(); ++secondIndex)
			{
				sharedEdgeCount += (isSharedEdge(_rFirst[firstIndex], _rSecond[secondIndex]) ? 1 : 0);
			}
		}

		return sharedEdgeCount;
	}

	static void partitionNodes(
		idx_t _partitionCount,
		std::vector<idx_t>& _rAdjacencies,
		std::vector<idx_t>& _rAdjacencyOffsets,
		std::vector<idx_t>& _rAdjacencyWeights,
		_Out_ std::vector<idx_t>& _rResultPartitions)
	{
		idx_t nodeCount = _rAdjacencyOffsets.size() - 1;
		idx_t constraintCount = 1;

		idx_t options[METIS_NOPTIONS];
		METIS_SetDefaultOptions(options);
		// TODO-MILKRU: Fill options if needed

		// https://ivan-pi.github.io/fmetis/interface/metis_partgraphkway.html
		// TODO-MILKRU: Test with METIS_PartGraphRecursive
		// TODO-MILKRU: If this doesn't return METIS_OK, try playing with partition weights, just like in FGraphPartitioner::BisectGraph
		idx_t resultEdgeCut;

		i32 result = METIS_PartGraphKway(
			&nodeCount,
			&constraintCount,
			_rAdjacencyOffsets.data(),
			_rAdjacencies.data(),
			nullptr, nullptr,
			_rAdjacencyWeights.data(),
			&_partitionCount,
			nullptr, nullptr,
			options,
			&resultEdgeCut,
			_rResultPartitions.data());

		assert(result == METIS_OK);
	}

	typedef std::vector<u32> GroupIndicies;

	static void groupMeshlets( // TODO-MILKRU: We need a new name for this and separate these two functions? use DRY more?
		Geometry& _rGeometry,
		u32 _meshletOffset,
		u32 _meshletCount,
		_Out_ std::vector<GroupIndicies>& _rGroups)
	{
		std::vector<MeshletBoundary> meshletBoundaries(_meshletCount);
		{
			// Store every every edge of each triangles here, which means some of them will be repeated
			std::vector<MeshletTriangleEdges> triangleEdgesPerMeshlet(_meshletCount);

			for (u32 meshletIndex = 0; meshletIndex < _meshletCount; ++meshletIndex)
			{
				u32 globalMeshletIndex = _meshletOffset + meshletIndex;
				Meshlet& rMeshlet = _rGeometry.meshlets[globalMeshletIndex];

				MeshletTriangleEdges& rEdgesPerTriangle = triangleEdgesPerMeshlet[meshletIndex];
				rEdgesPerTriangle.reserve(rMeshlet.triangleCount);

				for (u32 triangleIndex = 0; triangleIndex < rMeshlet.triangleCount; ++triangleIndex)
				{
					u8 localVertexIndex0 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 0];
					u8 localVertexIndex1 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 1];
					u8 localVertexIndex2 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 2];

					assert(localVertexIndex0 < rMeshlet.vertexCount);
					assert(localVertexIndex1 < rMeshlet.vertexCount);
					assert(localVertexIndex2 < rMeshlet.vertexCount);

					u32 vertexIndex0 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex0];
					u32 vertexIndex1 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex1];
					u32 vertexIndex2 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex2];

					TriangleEdges triangleEdges;
					triangleEdges[0] = std::make_pair(vertexIndex0, vertexIndex1);
					triangleEdges[1] = std::make_pair(vertexIndex1, vertexIndex2);
					triangleEdges[2] = std::make_pair(vertexIndex2, vertexIndex0);

					rEdgesPerTriangle.push_back(triangleEdges);
				}
			}

			// Filter only unique triangle edges in a meshlet, also known as boundary
			for (u32 meshletIndex = 0; meshletIndex < _meshletCount; ++meshletIndex)
			{
				MeshletBoundary& rMeshletBoundary = meshletBoundaries[meshletIndex];
				MeshletTriangleEdges& rEdgesPerTriangle = triangleEdgesPerMeshlet[meshletIndex];

				u32 triangleCount = rEdgesPerTriangle.size();
				for (u32 triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex)
				{
					TriangleEdges& rEdges = rEdgesPerTriangle[triangleIndex];

					for (u32 edgeIndex = 0; edgeIndex < rEdges.size(); ++edgeIndex)
					{
						TriangleEdge& rEdge = rEdges[edgeIndex];

						bool bSharedEdge = false;

						for (u32 otherTriangleIndex = 0; otherTriangleIndex < triangleCount; ++otherTriangleIndex)
						{
							if (triangleIndex == otherTriangleIndex)
							{
								continue;
							}

							TriangleEdges& rOtherEdges = rEdgesPerTriangle[otherTriangleIndex];

							for (u32 otherEdgeIndex = 0; otherEdgeIndex < rOtherEdges.size(); ++otherEdgeIndex)
							{
								TriangleEdge& rOtherEdge = rOtherEdges[otherEdgeIndex];

								if (isSharedEdge(rEdge, rOtherEdge))
								{
									bSharedEdge = true;
									break;
								}
							}
						}

						if (!bSharedEdge)
						{
							rMeshletBoundary.push_back(rEdge);
						}
					}
				}
			}
		}

		// TODO-MILKRU: Reserve some space for these
		std::vector<idx_t> adjacencies;
		std::vector<idx_t> adjacencyOffsets;
		std::vector<idx_t> adjacencyWeights;

		u32 adjacancyOffset = 0;
		adjacencyOffsets.push_back(adjacancyOffset);

		for (u32 meshletIndex = 0; meshletIndex < _meshletCount; ++meshletIndex)
		{
			MeshletBoundary& rMeshletBoundary = meshletBoundaries[meshletIndex];

			for (u32 otherMeshletIndex = 0; otherMeshletIndex < _meshletCount; ++otherMeshletIndex)
			{
				if (meshletIndex == otherMeshletIndex)
				{
					continue;
				}

				MeshletBoundary& rOtherMeshletBoundary = meshletBoundaries[otherMeshletIndex];

				u32 sharedEdgeCount = getSharedMeshletEdgeCount(rMeshletBoundary, rOtherMeshletBoundary);
				if (sharedEdgeCount == 0)
				{
					continue;
				}

				adjacencies.push_back(otherMeshletIndex);
				adjacencyWeights.push_back(sharedEdgeCount);
				++adjacancyOffset;
			}

			adjacencyOffsets.push_back(adjacancyOffset);
		}

		std::vector<idx_t> resultingPartitions(_meshletCount);
		partitionNodes(_rGroups.size(), adjacencies, adjacencyOffsets, adjacencyWeights, resultingPartitions);

		for (u32 meshletIndex = 0; meshletIndex < _meshletCount; ++meshletIndex)
		{
			idx_t groupIndex = resultingPartitions[meshletIndex];
			GroupIndicies& rGroup = _rGroups[groupIndex];

			// TODO-MILKRU: Opt
			//if (rGroup.size() == 0)
			//{
			//	rGroup.reserve(kTargetGroupSize);
			//}

			u32 globalMeshletIndex = _meshletOffset + meshletIndex;
			Meshlet& rMeshlet = _rGeometry.meshlets[globalMeshletIndex];

			for (u32 triangleIndex = 0; triangleIndex < rMeshlet.triangleCount; ++triangleIndex)
			{
				u8 localVertexIndex0 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 0];
				u8 localVertexIndex1 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 1];
				u8 localVertexIndex2 = _rGeometry.meshletTriangles[rMeshlet.triangleOffset + 3 * triangleIndex + 2];

				assert(localVertexIndex0 < rMeshlet.vertexCount);
				assert(localVertexIndex1 < rMeshlet.vertexCount);
				assert(localVertexIndex2 < rMeshlet.vertexCount);

				u32 vertexIndex0 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex0];
				u32 vertexIndex1 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex1];
				u32 vertexIndex2 = _rGeometry.meshletVertices[rMeshlet.vertexOffset + localVertexIndex2];

				rGroup.push_back(vertexIndex0);
				rGroup.push_back(vertexIndex1);
				rGroup.push_back(vertexIndex2);
			}
		}
	}
}

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

	return vertex;
}

static u32 buildMeshlets(
	Geometry& _rGeometry,
	std::vector<RawVertex>& _rRawVertices,
	std::vector<u32>& _rIndices,
	_Out_ std::vector<u32>& _rMeshletVertices,
	_Out_ std::vector<u8>& _rMeshletTriangles,
	_Out_ std::vector<Meshlet>& _rMeshlets)
{
	std::vector<meshopt_Meshlet> rawMeshlets(_rMeshlets.size());

	// TODO-MILKRU: After per-meshlet frustum/occlusion culling gets implemented, try playing around with cone_weight. You might get better performance.
	size_t meshletCount = meshopt_buildMeshlets(rawMeshlets.data(), _rMeshletVertices.data(), _rMeshletTriangles.data(), _rIndices.data(), _rIndices.size(),
		&_rRawVertices[0].position[0], _rRawVertices.size(), sizeof(RawVertex), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet, /*cone_weight*/ 0.7f);

	meshopt_Meshlet& rLastMeshlet = rawMeshlets[meshletCount - 1];

	// TODO-MILKRU: Not sure if it's a good idea to manage memory like this of passed arguments
	_rMeshletVertices.resize(rLastMeshlet.vertex_offset + size_t(rLastMeshlet.vertex_count));
	_rMeshletTriangles.resize(rLastMeshlet.triangle_offset + ((size_t(rLastMeshlet.triangle_count) * 3 + 3) & ~3));
	_rMeshlets.resize(meshletCount);

	for (u32 meshletIndex = 0; meshletIndex < meshletCount; ++meshletIndex)
	{
		meshopt_Meshlet& rRawMeshlet = rawMeshlets[meshletIndex];
		meshopt_Bounds bounds = meshopt_computeMeshletBounds(&_rMeshletVertices[rRawMeshlet.vertex_offset], &_rMeshletTriangles[rRawMeshlet.triangle_offset],
			rRawMeshlet.triangle_count, &_rRawVertices[0].position[0], _rRawVertices.size(), sizeof(RawVertex));

		rRawMeshlet.vertex_offset += _rGeometry.meshletVertices.size();
		rRawMeshlet.triangle_offset += _rGeometry.meshletTriangles.size();

		Meshlet& rMeshlet = _rMeshlets[meshletIndex];
		rMeshlet = buildMeshlet(rRawMeshlet, bounds);
	}

	return meshletCount;
}

static void loadMesh(
	Geometry& _rGeometry,
	const char* _pFilePath,
	bool _bMeshShadingSupported)
{
	fastObjMesh* objMesh = fast_obj_read(_pFilePath);
	assert(objMesh);

	std::vector<RawVertex> rawVertices;
	rawVertices.reserve(objMesh->index_count);

	for (u32 index = 0; index < objMesh->index_count; ++index)
	{
		fastObjIndex vertexIndex = objMesh->indices[index];

		RawVertex vertex{};

		vertex.position[0] = objMesh->positions[3 * size_t(vertexIndex.p) + 0];
		vertex.position[1] = objMesh->positions[3 * size_t(vertexIndex.p) + 1];
		vertex.position[2] = objMesh->positions[3 * size_t(vertexIndex.p) + 2];

		vertex.normal[0] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 0];
		vertex.normal[1] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 1];
		vertex.normal[2] = 0.5f + 0.5f * objMesh->normals[3 * size_t(vertexIndex.n) + 2];

		vertex.texCoord[0] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 0];
		vertex.texCoord[1] = objMesh->texcoords[2 * size_t(vertexIndex.t) + 1];

		rawVertices.push_back(vertex);
	}

	// TODO-MILKRU: All mesh preprocessing can be done offline in CMake for example.

	std::vector<u32> remapTable(objMesh->index_count);
	size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, objMesh->index_count,
		rawVertices.data(), rawVertices.size(), sizeof(RawVertex));

	rawVertices.resize(vertexCount);
	std::vector<u32> indices(objMesh->index_count);

	meshopt_remapVertexBuffer(rawVertices.data(), rawVertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
	meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

	meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), rawVertices.size());
	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &rawVertices[0].position[0], rawVertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
	meshopt_optimizeVertexFetch(rawVertices.data(), indices.data(), indices.size(), rawVertices.data(), rawVertices.size(), sizeof(RawVertex));

	v4 meshBounds = calculateMeshBounds(rawVertices);

	Mesh mesh = {};
	mesh.center[0] = meshBounds.x;
	mesh.center[1] = meshBounds.y;
	mesh.center[2] = meshBounds.z;
	mesh.radius = meshBounds.w;

	mesh.vertexOffset = u32(_rGeometry.vertices.size());
	_rGeometry.vertices.reserve(_rGeometry.vertices.size() + rawVertices.size());

	for (RawVertex& rVertex : rawVertices)
	{
		_rGeometry.vertices.push_back(quantizeVertex(rVertex));
	}

	mesh.lodCount = 0;

	for (u32 lodIndex = 0u; lodIndex < kMaxMeshLods; ++lodIndex) // TODO-MILKRU: Can be replaced with while(true) since we already have this break inside
	{
		mesh.lods[lodIndex].firstIndex = u32(_rGeometry.indices.size());
		mesh.lods[lodIndex].indexCount = u32(indices.size());
		_rGeometry.indices.insert(_rGeometry.indices.end(), indices.begin(), indices.end());

		++mesh.lodCount;

		if (lodIndex >= kMaxMeshLods - 1)
		{
			break;
		}

		f32 threshold = 0.6f;
		size_t targetIndexCount = size_t(indices.size() * threshold);
		f32 targetError = 1e-2f;

		size_t newIndexCount = meshopt_simplify(indices.data(), indices.data(), indices.size(),
			&rawVertices[0].position[0], rawVertices.size(), sizeof(RawVertex), targetIndexCount, targetError);

		if (indices.size() == newIndexCount)
		{
			break;
		}

		indices.resize(newIndexCount);
		meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), rawVertices.size());
	}

	// TODO-MILKRU: Separate meshlet LODing here

	if (_bMeshShadingSupported)
	{
		std::vector<pgd::GroupIndicies> groups;
		groups.push_back(indices);

		u32 lodIndex = 0;
		while (true)
		{
			u32 globalMeshletOffset = _rGeometry.meshlets.size();
			std::vector<Meshlet> meshletsPerLOD; // TODO-MILKRU: Inner loop vectors are terrible for memory

			u32 groupIndex = 0; // TODO-MILKRU: Temp

			// Generate and merge meshlets from each group
			for (pgd::GroupIndicies& rGroup : groups)
			{
				size_t maxMeshlets = meshopt_buildMeshletsBound(rGroup.size(), kMaxVerticesPerMeshlet, kMaxTrianglesPerMeshlet);
				std::vector<u32> meshletVertices(maxMeshlets * kMaxVerticesPerMeshlet);
				std::vector<u8> meshletTriangles(maxMeshlets * kMaxTrianglesPerMeshlet * 3);
				std::vector<Meshlet> meshlets(maxMeshlets);

				u32 meshletCount = buildMeshlets(_rGeometry, rawVertices, rGroup, meshletVertices, meshletTriangles, meshlets);

				// TODO-MILKRU: Temp
				for (u32 meshletIndex = 0; meshletIndex < meshletCount; ++meshletIndex)
				{
					meshlets[meshletIndex].groupIndex = groupIndex;
				}
				++groupIndex;

				_rGeometry.meshletVertices.insert(_rGeometry.meshletVertices.end(), meshletVertices.begin(), meshletVertices.end());
				_rGeometry.meshletTriangles.insert(_rGeometry.meshletTriangles.end(), meshletTriangles.begin(), meshletTriangles.end());
				_rGeometry.meshlets.insert(_rGeometry.meshlets.end(), meshlets.begin(), meshlets.end());

				meshletsPerLOD.insert(meshletsPerLOD.end(), meshlets.begin(), meshlets.end());
			}

			u32 globalMeshletCount = meshletsPerLOD.size();

			// TODO-MILKRU: Actually use this for testing
			mesh.lods[lodIndex].meshletOffset = globalMeshletOffset;
			mesh.lods[lodIndex].meshletCount = globalMeshletCount;

			if (lodIndex >= kMaxMeshLods - 1)
			{
				break;
			}

			groups.clear();

			u32 meshletCount = meshletsPerLOD.size();
			idx_t groupCount = meshletCount / kTargetGroupSize; // TODO-MILKRU: Maybe always bisect??? In that case groupCount would be 2?
			groups.resize(groupCount);
			pgd::groupMeshlets(_rGeometry, globalMeshletOffset, globalMeshletCount, groups);

			meshletsPerLOD.clear();

			// Simplify existing groups
			for (pgd::GroupIndicies& rGroup : groups)
			{
				meshopt_optimizeVertexCache(rGroup.data(), rGroup.data(), rGroup.size(), rawVertices.size());

				// TODO-MILKRU: This is similar like before. Unify
				f32 threshold = 0.5f; // TODO-MILKRU: We are reducing the number of indices, does this reduce the number of triangles proportionately?
				size_t targetIndexCount = size_t(rGroup.size() * threshold);
				f32 targetError = 1e-2f;

				f32 resultingError; // TODO-MILKRU: Use!
				// TODO-MILKRU: When to break????????
				size_t newIndexCount = meshopt_simplify(rGroup.data(), rGroup.data(), rGroup.size(),
					&rawVertices[0].position[0], rawVertices.size(), sizeof(RawVertex), targetIndexCount, targetError, &resultingError);

				if (rGroup.size() == newIndexCount)
				{
					// TODO-MILKRU: Temp
					// TODO-MILKRU: Break outer loop instead???
					// TODO-MILKRU: Or keep going with different lod levels for different groups???
					assert(0);
				}

				rGroup.resize(newIndexCount);
			}

			++lodIndex;
		}
	}

	_rGeometry.meshes.push_back(mesh);
}

GeometryBuffers createGeometryBuffers(
	Device& _rDevice,
	u32 _meshCount,
	const char** _ppMeshPaths)
{
	EASY_BLOCK("InitializeGeometry");

	Geometry geometry{};

	// TODO-MILKRU: This shouldn't be here
	for (u32 meshIndex = 0; meshIndex < _meshCount; ++meshIndex)
	{
		const char* meshPath = _ppMeshPaths[meshIndex + 1];
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
