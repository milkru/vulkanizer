#pragma once

struct Camera
{
	float fov;
	float aspect;
	float near;
	float moveSpeed;
	float sensitivity;

	float pitch;
	float yaw;
	glm::vec3 position;

	glm::mat4 view;
	glm::mat4 projection;
};

void updateCamera(
	GLFWwindow* _pWindow,
	float _deltaTime,
	Camera& _rCamera);

void getFrustumPlanes(
	Camera _camera,
	glm::vec4* _pFrustumPlanes);
