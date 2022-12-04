#pragma once

struct Camera
{
	f32 fov = 60.0f;
	f32 aspect = 1.0f;
	f32 near = 0.01f;
	f32 moveSpeed = 1.0f;
	f32 boostMoveSpeed = 1.0f;
	f32 sensitivity = 16.0f;

	f32 pitch = 0.0f;
	f32 yaw = 0.0f;
	v3 position{};

	m4 view{};
	m4 projection{};
};

void updateCamera(
	GLFWwindow* _pWindow,
	f32 _deltaTime,
	Camera& _rCamera);

void getFrustumPlanes(
	Camera _camera,
	v4* _pFrustumPlanes);
