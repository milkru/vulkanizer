#pragma once

struct WindowDesc
{
	uint32_t width = 0u;
	uint32_t height = 0u;
	const char* title = "Unknown Title";
};

GLFWwindow* createWindow(
	WindowDesc _desc);

void destroyWindow(
	GLFWwindow* _pWindow);
