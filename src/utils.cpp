#include "utils.h"

#include <stdio.h>

std::vector<char> readFile(
	const char* _pFilePath)
{
	FILE* file = fopen(_pFilePath, "rb");
	assert(file != nullptr);

	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0L, SEEK_SET);

	std::vector<char> fileContents(fileSize);
	size_t bytesToRead = fread(fileContents.data(), sizeof(u8), fileSize, file);
	fclose(file);

	return fileContents;
}

m4 getInfinitePerspectiveMatrix(
	f32 _fov,
	f32 _aspect,
	f32 _near)
{
	f32 f = 1.0f / tanf(_fov / 2.0f);
	return m4(
		f / _aspect, 0.0f, 0.0f, 0.0f,
		0.0f, -f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, _near, 0.0f);
}

u32 divideRoundingUp(
	u32 _dividend,
	u32 _divisor)
{
	return (_dividend + _divisor - 1) / _divisor;
}

u32 roundUpToPowerOfTwo(
	f32 _value)
{
	f32 valueBase = glm::log2(_value);

	if (glm::fract(valueBase) == 0.0f)
	{
		return _value;
	}

	return glm::pow(2.0f, glm::trunc(valueBase + 1.0f));
}
