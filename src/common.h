#pragma once

#include <volk.h>
#include <meshoptimizer.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <array>
#include <string>

#ifndef VK_CALL
#define VK_CALL(_call) \
	do { \
		VkResult result_ = _call; \
		assert(result_ == VK_SUCCESS); \
	} \
	while (0)
#endif // VK_CALL

#ifndef MIN
#define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif // MIN

#ifndef MAX
#define MAX(_a, _b) (((_b) < (_a)) ? (_a) : (_b))
#endif // MAX

#ifndef CLAMP
#define CLAMP(_val, _min, _max) ((_val) < (_min) ? (_min) : (_val) > (_max) ? (_max) : (_val))
#endif // CLAMP

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) ((int32_t)(sizeof(_arr) / sizeof(_arr[0])))
#endif // ARRAY_SIZE

#ifndef TO_STRING
#define TO_STRING(_name) #_name
#endif // TO_STRING

#ifndef GPU_QUERY_PROFILING
#define GPU_QUERY_PROFILING 1
#endif // GPU_QUERY_PROFILING
