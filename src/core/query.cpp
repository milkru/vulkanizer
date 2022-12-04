#include "device.h"
#include "query.h"

static const VkQueryPipelineStatisticFlagBits kPipelineStats[] =
{
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,      // InputAssemblyVertices
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,    // InputAssemblyPrimitives
	VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,    // VertexShaderInvocations
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,         // ClippingInvocations
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,          // ClippingPrimitives
	VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,  // FragmentShaderInvocations
	VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT,   // ComputeShaderInvocations
};
static_assert(ARRAY_SIZE(kPipelineStats) == i32(StatType::Count));

static u32 getQueryResultElementCount(
	VkQueryType _type)
{
	return (_type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? ARRAY_SIZE(kPipelineStats) : 1u) + 1u;
}

QueryPool createQueryPool(
	Device& _rDevice,
	QueryPoolDesc _desc)
{
	assert(
		_desc.type == VK_QUERY_TYPE_PIPELINE_STATISTICS ||
		_desc.type == VK_QUERY_TYPE_TIMESTAMP);

	QueryPool queryPool{};
	queryPool.type = _desc.type;
	queryPool.queryCapacity = _desc.queryCount;

	VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolCreateInfo.queryType = _desc.type;
	queryPoolCreateInfo.queryCount = _desc.queryCount;

	if (_desc.type == VK_QUERY_TYPE_PIPELINE_STATISTICS)
	{
		for (u32 statisticIndex = 0u; statisticIndex < ARRAY_SIZE(kPipelineStats); ++statisticIndex)
		{
			queryPoolCreateInfo.pipelineStatistics |= kPipelineStats[statisticIndex];
		}
	}

	VK_CALL(vkCreateQueryPool(_rDevice.device, &queryPoolCreateInfo, nullptr, &queryPool.queryPoolVk));

	u32 resultElementCount = getQueryResultElementCount(queryPool.type);
	queryPool.queryResults.resize(size_t(resultElementCount) * _desc.queryCount);

	return queryPool;
}

void destroyQueryPool(
	Device& _rDevice,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.queryPoolVk != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(_rDevice.device, _rQueryPool.queryPoolVk, nullptr);
	}

	_rQueryPool.queryResults.clear();
}

void resetQueryPool(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.status == QueryPoolStatus::Available)
	{
		vkCmdResetQueryPool(_commandBuffer, _rQueryPool.queryPoolVk, 0u, _rQueryPool.queryCapacity);
		_rQueryPool.status = QueryPoolStatus::Reset;
	}
}

Queries allocateQueries(
	QueryPool& _rQueryPool,
	u32 _count)
{
	assert(_count > 0u);
	assert(_rQueryPool.allocatedQueries + _count <= _rQueryPool.queryCapacity);

	Queries queries{};
	queries.first = _rQueryPool.allocatedQueries;
	queries.count = _count;

	_rQueryPool.allocatedQueries += _count;

	return queries;
}

void writeTimestamp(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	VkPipelineStageFlagBits _pipelineStage,
	u32 _query)
{
	assert(_rQueryPool.type == VK_QUERY_TYPE_TIMESTAMP);
	assert(_query < _rQueryPool.allocatedQueries);

	if (_rQueryPool.status == QueryPoolStatus::Reset)
	{
		vkCmdWriteTimestamp(_commandBuffer, _pipelineStage, _rQueryPool.queryPoolVk, _query);
	}
}

void beginQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	u32 _query)
{
	assert(_query < _rQueryPool.allocatedQueries);

	if (_rQueryPool.status == QueryPoolStatus::Reset)
	{
		vkCmdBeginQuery(_commandBuffer, _rQueryPool.queryPoolVk, _query, 0u);
	}
}

void endQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	u32 _query)
{
	assert(_query < _rQueryPool.allocatedQueries);

	if (_rQueryPool.status == QueryPoolStatus::Reset)
	{
		vkCmdEndQuery(_commandBuffer, _rQueryPool.queryPoolVk, _query);
	}
}

void updateQueryPoolResults(
	Device& _rDevice,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.allocatedQueries == 0u)
	{
		return;
	}

	u32 resultElementCount = getQueryResultElementCount(_rQueryPool.type);

	// TODO-MILKRU: Theres something wrong with getting query results in some situations.
	// Freeze camera completely breaks this. That might be a good clue. Was this present before? Check old code.
	VkResult result = vkGetQueryPoolResults(_rDevice.device, _rQueryPool.queryPoolVk, 0u, _rQueryPool.allocatedQueries,
		_rQueryPool.queryResults.size() * sizeof(u64), _rQueryPool.queryResults.data(), resultElementCount * sizeof(u64),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

	_rQueryPool.status = result == VK_SUCCESS ? QueryPoolStatus::Available : QueryPoolStatus::Issued;
}

u64 getQueryResult(
	QueryPool& _rQueryPool,
	u32 _query,
	u32 _statisticsOffset)
{
	assert(_rQueryPool.status == QueryPoolStatus::Available);
	assert(_statisticsOffset < ARRAY_SIZE(kPipelineStats));

	u32 resultElementCount = getQueryResultElementCount(_rQueryPool.type);
	return _rQueryPool.queryResults[size_t(resultElementCount) * _query + _statisticsOffset];
}
