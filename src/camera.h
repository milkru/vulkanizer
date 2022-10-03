#pragma once

struct Camera
{
	float fov = 60.0f;
	float aspect = 1.0f;
	float near = 0.01f;
	float moveSpeed = 1.0f;
	float sensitivity = 16.0f;

	float pitch = 0.0f;
	float yaw = 0.0f;
	glm::vec3 position{};

	glm::mat4 view{};
	glm::mat4 projection{};
};

void updateCamera(
	GLFWwindow* _pWindow,
	float _deltaTime,
	Camera& _rCamera);

void getFrustumPlanes(
	Camera _camera,
	glm::vec4* _pFrustumPlanes);
