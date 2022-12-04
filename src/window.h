#pragma once

struct WindowDesc
{
	u32 width = 0u;				          // Window width in pixels.
	u32 height = 0u;				      // Window height in pixels.
	const char* title = "Unknown Title";  // Window top bar title.
};

GLFWwindow* createWindow(
	WindowDesc _desc);

void destroyWindow(
	GLFWwindow* _pWindow);
