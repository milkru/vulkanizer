#pragma once

std::vector<uint8_t> readFile(
	const char* _pFilePath);

glm::mat4 getInfinitePerspectiveMatrix(
	float _fov,
	float _aspect,
	float _near);
