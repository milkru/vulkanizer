#include "common.h"
#include "device.h"
#include "query.h"

static const VkQueryPipelineStatisticFlagBits kPipelineStats[] =
{
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT,
	VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT,
	VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT,
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT,
	VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT,
	VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
	VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT
};

static uint32_t getQueryResultElementCount(
	VkQueryType _type)
{
	return (_type == VK_QUERY_TYPE_PIPELINE_STATISTICS ? ARRAY_SIZE(kPipelineStats) : 1u) + 1u;
}

QueryPool createQueryPool(
	Device _device,
	VkQueryType _type,
	uint32_t _queryCount)
{
	assert(
		_type == VK_QUERY_TYPE_PIPELINE_STATISTICS ||
		_type == VK_QUERY_TYPE_TIMESTAMP);

	QueryPool queryPool{};
	queryPool.type = _type;
	queryPool.queryCapacity = _queryCount;

	VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolCreateInfo.queryType = _type;
	queryPoolCreateInfo.queryCount = _queryCount;

	if (_type == VK_QUERY_TYPE_PIPELINE_STATISTICS)
	{
		for (uint32_t statisticIndex = 0; statisticIndex < ARRAY_SIZE(kPipelineStats); ++statisticIndex)
		{
			queryPoolCreateInfo.pipelineStatistics |= kPipelineStats[statisticIndex];
		}
	}

	VK_CALL(vkCreateQueryPool(_device.deviceVk, &queryPoolCreateInfo, nullptr, &queryPool.queryPoolVk));

	uint32_t resultElementCount = getQueryResultElementCount(queryPool.type);
	queryPool.queryResults.resize(size_t(resultElementCount) * _queryCount);

	return queryPool;
}

void destroyQueryPool(
	Device _device,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.queryPoolVk != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(_device.deviceVk, _rQueryPool.queryPoolVk, nullptr);
	}

	_rQueryPool.queryResults.clear();
}

void resetQueryPool(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.status == QueryPoolStatus::Available)
	{
		vkCmdResetQueryPool(_commandBuffer, _rQueryPool.queryPoolVk, 0, _rQueryPool.queryCapacity);
		_rQueryPool.status = QueryPoolStatus::Reset;
	}
}

Queries allocateQueries(
	QueryPool& _rQueryPool,
	uint32_t _count)
{
	assert(_count > 0);
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
	uint32_t _query)
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
	uint32_t _query)
{
	assert(_query < _rQueryPool.allocatedQueries);

	if (_rQueryPool.status == QueryPoolStatus::Reset)
	{
		vkCmdBeginQuery(_commandBuffer, _rQueryPool.queryPoolVk, _query, 0);
	}
}

void endQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	uint32_t _query)
{
	assert(_query < _rQueryPool.allocatedQueries);

	if (_rQueryPool.status == QueryPoolStatus::Reset)
	{
		vkCmdEndQuery(_commandBuffer, _rQueryPool.queryPoolVk, _query);
	}
}

void updateQueryPoolResults(
	Device _device,
	QueryPool& _rQueryPool)
{
	if (_rQueryPool.allocatedQueries == 0)
	{
		return;
	}

	uint32_t resultElementCount = getQueryResultElementCount(_rQueryPool.type);

	VkResult result = vkGetQueryPoolResults(_device.deviceVk, _rQueryPool.queryPoolVk, 0, _rQueryPool.allocatedQueries,
		_rQueryPool.queryResults.size() * sizeof(uint64_t), _rQueryPool.queryResults.data(), resultElementCount * sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

	_rQueryPool.status = result == VK_SUCCESS ? QueryPoolStatus::Available : QueryPoolStatus::Issued;
}

uint64_t getQueryResult(
	QueryPool& _rQueryPool,
	uint32_t _query,
	uint32_t _statisticsOffset)
{
	assert(_rQueryPool.status == QueryPoolStatus::Available);
	assert(_statisticsOffset < ARRAY_SIZE(kPipelineStats));

	uint32_t resultElementCount = getQueryResultElementCount(_rQueryPool.type);
	return _rQueryPool.queryResults[size_t(resultElementCount) * _query + _statisticsOffset];
}

#if GPU_QUERY_PROFILING

#include <map>

static std::map<std::string, Queries> declaredNameQueries;

static Queries getOrCreateDeclarationQueries(
	std::string _name,
	QueryPool& _rQueryPool,
	uint32_t _queryCount)
{
	if (declaredNameQueries.find(_name) != declaredNameQueries.end())
	{
		return declaredNameQueries[_name];
	}

	Queries queries = allocateQueries(_rQueryPool, _queryCount);
	declaredNameQueries[_name] = queries;

	return queries;
}

ScopedGpuBlock::ScopedGpuBlock(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	const char* _name)
	: commandBuffer(_commandBuffer)
	, rQueryPool(_rQueryPool)
{
	assert(_rQueryPool.type == VK_QUERY_TYPE_TIMESTAMP);
	queries = getOrCreateDeclarationQueries(_name, _rQueryPool, /*queryCount*/ 2u);
	writeTimestamp(commandBuffer, rQueryPool, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries.first);
}

ScopedGpuBlock::~ScopedGpuBlock()
{
	writeTimestamp(commandBuffer, rQueryPool, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries.first + 1u);
}

ScopedGpuStats::ScopedGpuStats(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	const char* _name)
	: commandBuffer(_commandBuffer)
	, rQueryPool(_rQueryPool)
{
	assert(_rQueryPool.type == VK_QUERY_TYPE_PIPELINE_STATISTICS);
	queries = getOrCreateDeclarationQueries(_name, _rQueryPool, /*queryCount*/ 1u);
	beginQuery(commandBuffer, rQueryPool, queries.first);
}

ScopedGpuStats::~ScopedGpuStats()
{
	endQuery(commandBuffer, rQueryPool, queries.first);
}

bool tryGetBlockResult(
	QueryPool& _rQueryPool,
	const char* _name,
	VkPhysicalDeviceLimits _limits,
	double& _rResult)
{
	assert(_rQueryPool.type == VK_QUERY_TYPE_TIMESTAMP);

	if (_rQueryPool.status != QueryPoolStatus::Available)
	{
		return false;
	}

	assert(declaredNameQueries.find(_name) != declaredNameQueries.end());

	Queries queries = declaredNameQueries[_name];

	uint64_t beginQueryResult = getQueryResult(_rQueryPool, queries.first);
	uint64_t endQueryResult = getQueryResult(_rQueryPool, queries.first + 1u);

	double timePeriodMillis = double(_limits.timestampPeriod) * 1e-6;
	_rResult = (endQueryResult - beginQueryResult) * timePeriodMillis;

	return true;
}

bool tryGetStatsResult(
	QueryPool& _rQueryPool,
	const char* _name,
	StatType _type,
	uint64_t& _rResult)
{
	assert(_rQueryPool.type == VK_QUERY_TYPE_PIPELINE_STATISTICS);

	if (_rQueryPool.status != QueryPoolStatus::Available)
	{
		return false;
	}

	assert(declaredNameQueries.find(_name) != declaredNameQueries.end());

	Queries queries = declaredNameQueries[_name];
	_rResult = getQueryResult(_rQueryPool, queries.first, uint32_t(_type));

	return true;
}

#endif // GPU_QUERY_PROFILING
