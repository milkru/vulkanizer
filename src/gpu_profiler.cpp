#include "core/device.h"
#include "core/query.h"

#include "gpu_profiler.h"

namespace gpu::profiler
{
	struct Context
	{
		typedef std::map<std::string, Queries> NamedQueries;

		NamedQueries namedBlockQueries;
		NamedQueries namedStatsQueries;

		QueryPool timestampsQueryPool;
		QueryPool statisticsQueryPool;
	} static gContext;

	void initialize(
		Device& _rDevice)
	{
		gContext = {
			.timestampsQueryPool = createQueryPool(_rDevice, {
				.type = VK_QUERY_TYPE_TIMESTAMP,
				.queryCount = 12 }),
			.statisticsQueryPool = createQueryPool(_rDevice, {
				.type = VK_QUERY_TYPE_PIPELINE_STATISTICS,
				.queryCount = 1 }) };
	}

	void beginFrame(
		VkCommandBuffer _commandBuffer)
	{
		resetQueryPool(_commandBuffer, gContext.timestampsQueryPool);
		resetQueryPool(_commandBuffer, gContext.statisticsQueryPool);
	}

	void endFrame(
		Device& _rDevice)
	{
		updateQueryPoolResults(_rDevice, gContext.timestampsQueryPool);
		updateQueryPoolResults(_rDevice, gContext.statisticsQueryPool);
	}

	std::vector<const char*> getBlockNames()
	{
		std::vector<const char*> names;

		for (auto& rMapEntry : gContext.namedBlockQueries)
		{
			names.push_back(rMapEntry.first.c_str());
		}

		return names;
	}

	void terminate(
		Device& _rDevice)
	{
		destroyQueryPool(_rDevice, gContext.timestampsQueryPool);
		gContext.namedBlockQueries.clear();

		destroyQueryPool(_rDevice, gContext.statisticsQueryPool);
		gContext.namedStatsQueries.clear();

		gContext = {};
	}

	ScopedBlock::ScopedBlock(
		VkCommandBuffer _commandBuffer,
		const char* _name)
		: commandBuffer(_commandBuffer)
	{
#ifdef DEBUG_
		VkDebugUtilsLabelEXT debugUtilsLabel = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
		debugUtilsLabel.pLabelName = _name;

		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugUtilsLabel);
#endif // DEBUG_

		if (gContext.namedBlockQueries.find(_name) == gContext.namedBlockQueries.end())
		{
			gContext.namedBlockQueries[_name] = allocateQueries(gContext.timestampsQueryPool, /*queryCount*/ 2u);
		}

		queries = gContext.namedBlockQueries[_name];
		writeTimestamp(commandBuffer, gContext.timestampsQueryPool, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries.first);
	}

	ScopedBlock::~ScopedBlock()
	{
#ifdef DEBUG_
		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
#endif // DEBUG_

		writeTimestamp(commandBuffer, gContext.timestampsQueryPool, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries.first + 1);
	}

	bool tryGetBlockResult(
		const char* _name,
		VkPhysicalDeviceLimits _limits,
		f64& _rResult)
	{
		assert(gContext.namedBlockQueries.find(_name) != gContext.namedBlockQueries.end());

		if (gContext.timestampsQueryPool.status != QueryPoolStatus::Available)
		{
			return false;
		}

		Queries queries = gContext.namedBlockQueries[_name];

		u64 beginQueryResult = getQueryResult(gContext.timestampsQueryPool, queries.first);
		u64 endQueryResult = getQueryResult(gContext.timestampsQueryPool, queries.first + 1);

		f64 timePeriodMillis = f64(_limits.timestampPeriod) * 1e-6;
		_rResult = (endQueryResult - beginQueryResult) * timePeriodMillis;

		return true;
	}

	ScopedStats::ScopedStats(
		VkCommandBuffer _commandBuffer,
		const char* _name)
		: commandBuffer(_commandBuffer)
	{
		if (gContext.namedStatsQueries.find(_name) == gContext.namedStatsQueries.end())
		{
			gContext.namedStatsQueries[_name] = allocateQueries(gContext.statisticsQueryPool, /*queryCount*/ 1);
		}

		queries = gContext.namedStatsQueries[_name];
		beginQuery(commandBuffer, gContext.statisticsQueryPool, queries.first);
	}

	ScopedStats::~ScopedStats()
	{
		endQuery(commandBuffer, gContext.statisticsQueryPool, queries.first);
	}

	bool tryGetStatsResult(
		const char* _name,
		StatType _type,
		u64& _rResult)
	{
		if (gContext.statisticsQueryPool.status != QueryPoolStatus::Available)
		{
			return false;
		}

		assert(gContext.namedStatsQueries.find(_name) != gContext.namedStatsQueries.end());

		Queries queries = gContext.namedStatsQueries[_name];
		_rResult = getQueryResult(gContext.statisticsQueryPool, queries.first, u32(_type));

		return true;
	}
}
