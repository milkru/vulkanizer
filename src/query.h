#pragma once

struct QueryPoolDesc
{
	VkQueryType type = VK_QUERY_TYPE_MAX_ENUM;
	uint32_t queryCount = 0u;
};

enum class QueryPoolStatus : uint8_t
{
	Reset,
	Issued,
	Available,
};

struct QueryPool
{
	VkQueryPool queryPoolVk = VK_NULL_HANDLE;
	VkQueryType type = VK_QUERY_TYPE_MAX_ENUM;
	QueryPoolStatus status = QueryPoolStatus::Available;
	uint32_t queryCapacity = 0;
	uint32_t allocatedQueries = 0;
	std::vector<uint64_t> queryResults{};
};

QueryPool createQueryPool(
	Device _device,
	QueryPoolDesc _desc);

void destroyQueryPool(
	Device _device,
	QueryPool& _rQueryPool);

void resetQueryPool(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool);

struct Queries
{
	uint32_t first;
	uint32_t count;
};

Queries allocateQueries(
	QueryPool& _rQueryPool,
	uint32_t _count);

void writeTimestamp(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	VkPipelineStageFlagBits _pipelineStage,
	uint32_t _query);

void beginQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	uint32_t _query);

void endQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	uint32_t _query);

void updateQueryPoolResults(
	Device _device,
	QueryPool& _rQueryPool);

uint64_t getQueryResult(
	QueryPool& _rQueryPool,
	uint32_t _query,
	uint32_t _statisticsOffset = 0);

#if GPU_QUERY_PROFILING

#define SCOPED_GPU_NAME(_name) ScopedGpu##_name

#define DECL_SCOPED_GPU_WRAPPER(_name) \
		struct SCOPED_GPU_NAME(_name) \
		{ \
			SCOPED_GPU_NAME(_name)( \
				VkCommandBuffer _commandBuffer, \
				QueryPool& _rQueryPool, \
				const char* _name); \
			~SCOPED_GPU_NAME(_name)(); \
			VkCommandBuffer commandBuffer; \
			QueryPool& rQueryPool; \
			Queries queries; \
		};

DECL_SCOPED_GPU_WRAPPER(Block)
DECL_SCOPED_GPU_WRAPPER(Stats)

#define GPU_BLOCK_NAME(_name) TO_STRING(_name##Block)
#define GPU_STATS_NAME(_name) TO_STRING(_name##Stats)

#define GPU_BLOCK(_commandBuffer, _rQueryPool, _name) \
		SCOPED_GPU_NAME(Block) _name##Block(_commandBuffer, _rQueryPool, GPU_BLOCK_NAME(_name))

#define GPU_STATS(_commandBuffer, _rQueryPool, _name) \
		SCOPED_GPU_NAME(Stats) _name##Stats(_commandBuffer, _rQueryPool, GPU_STATS_NAME(_name))

enum class StatType : uint8_t
{
	InputAssemblyVertices,
	InputAssemblyPrimitives,
	VertexShaderInvocations,
	ClippingInvocations,
	ClippingPrimitives,
	FragmentShaderInvocations,
	ComputeShaderInvocations,

	Count
};

bool tryGetBlockResult(
	QueryPool& _rQueryPool,
	const char* _name,
	VkPhysicalDeviceLimits _limits,
	double& _rResult);

bool tryGetStatsResult(
	QueryPool& _rQueryPool,
	const char* _name,
	StatType _type,
	uint64_t& _rResult);

#define GPU_BLOCK_RESULT(_rQueryPool, _name, _limits, _rResult) \
	tryGetBlockResult(_rQueryPool, GPU_BLOCK_NAME(_name), _limits, _rResult)

#define GPU_STATS_RESULT(_rQueryPool, _name, _type, _rResult) \
	tryGetStatsResult(_rQueryPool, GPU_STATS_NAME(_name), _type, _rResult)

#else

#define GPU_BLOCK(_commandBuffer, _rQueryPool, _name)
#define GPU_STATS(_commandBuffer, _rQueryPool, _name)
#define GPU_BLOCK_RESULT(_rQueryPool, _name, _limits, _rResult)
#define GPU_STATS_RESULT(_rQueryPool, _name, _type, _rResult)

#endif // GPU_QUERY_PROFILING
