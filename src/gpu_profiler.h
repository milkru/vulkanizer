#pragma once

namespace gpu::profiler
{
	void initialize(
		Device& _rDevice);

	void terminate(
		Device& _rDevice);

	void beginFrame(
		VkCommandBuffer _commandBuffer);

	void endFrame(
		Device& _rDevice);

	std::vector<const char*> getBlockNames();

	struct ScopedBlock
	{
		ScopedBlock(
			VkCommandBuffer _commandBuffer,
			const char* _name);
		~ScopedBlock();

		VkCommandBuffer commandBuffer;
		Queries queries;
	};

	bool tryGetBlockResult(
		const char* _name,
		VkPhysicalDeviceLimits _limits,
		f64& _rResult);

	struct ScopedStats
	{
		ScopedStats(
			VkCommandBuffer _commandBuffer,
			const char* _name);
		~ScopedStats();

		VkCommandBuffer commandBuffer;
		Queries queries;
	};

	bool tryGetStatsResult(
		const char* _name,
		StatType _type,
		u64& _rResult);
}

#ifndef UNIQUE_NAME
#define UNIQUE_NAME(_suffix) TOKEN_JOIN(uniqueName, _suffix)
#endif // UNIQUE_NAME

#ifndef GPU_BLOCK
#define GPU_BLOCK(_commandBuffer, _name) \
	gpu::profiler::ScopedBlock UNIQUE_NAME(__LINE__)(_commandBuffer, _name)
#endif // GPU_BLOCK

#ifndef GPU_STATS
#define GPU_STATS(_commandBuffer, _name) \
	gpu::profiler::ScopedStats UNIQUE_NAME(__LINE__)(_commandBuffer, _name)
#endif // GPU_STATS

#ifndef GPU_BLOCK_RESULT
#define GPU_BLOCK_RESULT(_name, _limits, _rResult) \
	gpu::profiler::tryGetBlockResult(_name, _limits, _rResult)
#endif // GPU_BLOCK_RESULT

#ifndef GPU_STATS_RESULT
#define GPU_STATS_RESULT(_name, _type, _rResult) \
	gpu::profiler::tryGetStatsResult(_name, _type, _rResult)
#endif // GPU_STATS_RESULT
