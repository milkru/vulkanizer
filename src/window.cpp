#include "common.h"
#include "window.h"

GLFWwindow* createWindow(
	WindowDesc _desc)
{
	glfwSetErrorCallback(
		[](int32_t _error, const char* _description)
		{
			fprintf(stderr, "GLFW error: %s\n", _description);
		}
	);

	if (!glfwInit())
	{
		assert(!"GLFW not initialized properly!");
	}

	if (!glfwVulkanSupported())
	{
		assert(!"Vulkan is not supported!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWwindow* pWindow = glfwCreateWindow(_desc.width, _desc.height, _desc.title, nullptr, nullptr);
	if (pWindow == nullptr)
	{
		glfwTerminate();
		assert(!"Window creation failed!");
	}

	return pWindow;
}

void destroyWindow(
	GLFWwindow* _pWindow)
{
	glfwDestroyWindow(_pWindow);
	glfwTerminate();
}
