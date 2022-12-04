#pragma once

enum class QueryPoolStatus : u8
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
	u32 queryCapacity = 0;
	u32 allocatedQueries = 0;
	std::vector<u64> queryResults{};
};

struct QueryPoolDesc
{
	VkQueryType type = VK_QUERY_TYPE_MAX_ENUM;  // Query type allocated in the pool.
	u32 queryCount = 0u;					    // Query pool capacity.
};

QueryPool createQueryPool(
	Device& _rDevice,
	QueryPoolDesc _desc);

void destroyQueryPool(
	Device& _rDevice,
	QueryPool& _rQueryPool);

void resetQueryPool(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool);

struct Queries
{
	u32 first;
	u32 count;
};

Queries allocateQueries(
	QueryPool& _rQueryPool,
	u32 _count);

void writeTimestamp(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	VkPipelineStageFlagBits _pipelineStage,
	u32 _query);

void beginQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	u32 _query);

void endQuery(
	VkCommandBuffer _commandBuffer,
	QueryPool& _rQueryPool,
	u32 _query);

void updateQueryPoolResults(
	Device& _rDevice,
	QueryPool& _rQueryPool);

u64 getQueryResult(
	QueryPool& _rQueryPool,
	u32 _query,
	u32 _statisticsOffset = 0);

enum class StatType : u8
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
