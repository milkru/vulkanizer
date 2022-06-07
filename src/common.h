#pragma once

#include <volk.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>

#ifndef VK_CALL
#define VK_CALL(_call) \
	do { \
		VkResult result_ = _call; \
		assert(result_ == VK_SUCCESS); \
	} \
	while (0)
#endif

#ifndef MIN
#define MIN(_a, _b) (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef MAX
#define MAX(_a, _b) (((_b) < (_a)) ? (_a) : (_b))
#endif

#ifndef CLAMP
#define CLAMP(_val, _min, _max) ((_val) < (_min) ? (_min) : (_val) > (_max) ? (_max) : (_val))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_arr) ((int)(sizeof(_arr) / sizeof(_arr[0])))
#endif

#ifndef TO_STRING
#define TO_STRING(_name) #_name
#endif
