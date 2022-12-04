#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <easy/profiler.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <string>
#include <functional>
#include <map>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef glm::vec2 v2;
typedef glm::vec3 v3;
typedef glm::vec4 v4;

typedef glm::ivec2 iv2;
typedef glm::ivec3 iv3;
typedef glm::ivec4 iv4;

typedef glm::uvec2 uv2;
typedef glm::uvec3 uv3;
typedef glm::uvec4 uv4;

typedef glm::mat2 m2;
typedef glm::mat3 m3;
typedef glm::mat4 m4;

#ifndef LAMBDA
#define LAMBDA(...) std::function<void(__VA_ARGS__)> const&
#endif // LAMBDA

#ifndef VK_CALL
#define VK_CALL(_call) \
	do { \
		VkResult result_ = _call; \
		assert(result_ == VK_SUCCESS); \
	} \
	while (0)
#endif // VK_CALL

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) ((i32)(sizeof(_arr) / sizeof(_arr[0])))
#endif // ARRAY_SIZE

#ifndef TOKEN_JOIN
#define TOKEN_JOIN(_x, _y) _x ## _y
#endif // TOKEN_JOIN

#ifndef GPU_QUERY_PROFILING
#define GPU_QUERY_PROFILING 1
#endif // GPU_QUERY_PROFILING
