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
