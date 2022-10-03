#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <string>
#include <functional>

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
#define ARRAY_SIZE(_arr) ((int32_t)(sizeof(_arr) / sizeof(_arr[0])))
#endif // ARRAY_SIZE

#ifndef TO_STRING
#define TO_STRING(_name) #_name
#endif // TO_STRING

#ifndef GPU_QUERY_PROFILING
#define GPU_QUERY_PROFILING 1
#endif // GPU_QUERY_PROFILING

#ifndef DECL_ENUM_FLAG
#define DECL_ENUM_FLAG(_enum) \
    inline _enum operator&(_enum a, _enum b) \
    { \
        return static_cast<_enum>(static_cast<std::underlying_type<_enum>::type>(a) & \
			static_cast<std::underlying_type<_enum>::type>(b) ); \
    } \
    inline _enum& operator&=(_enum& a, _enum b) \
    { \
        return a = a & b; \
    } \
    inline _enum operator|(_enum a, _enum b) \
    { \
        return static_cast<_enum>( static_cast<std::underlying_type<_enum>::type>(a) | \
			static_cast<std::underlying_type<_enum>::type>(b)); \
    } \
    inline _enum& operator|=(_enum& a, _enum b) \
    { \
        return a = a | b; \
    }
#endif // DECL_ENUM_FLAG
