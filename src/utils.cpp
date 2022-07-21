#include "common.h"
#include "utils.h"

#include <stdio.h>

std::vector<uint8_t> readFile(
	const char* _pFilePath)
{
	FILE* file = fopen(_pFilePath, "rb");
	assert(file != nullptr);

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	std::vector<uint8_t> fileContents(fileSize);
	size_t bytesToRead = fread(fileContents.data(), sizeof(uint8_t), fileSize, file);
	fclose(file);

	return fileContents;
}

glm::mat4 getInfinitePerspectiveMatrix(
	float _fov,
	float _aspect,
	float _near)
{
	float f = 1.0f / tanf(_fov / 2.0f);
	return glm::mat4(
		f / _aspect, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, _near, 0.0f);
}
