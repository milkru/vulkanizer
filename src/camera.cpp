#include "camera.h"
#include "utils.h"
#include "window.h"
#include "shaders/shader_interop.h"

static v2 getLookDirection(
	GLFWwindow* _pWindow)
{
	v2 direction(0.0f, 0.0f);
	v2 right(0.0f, 1.0f);
	v2 up(1.0f, 0.0f);

	if (glfwGetKey(_pWindow, GLFW_KEY_UP) == GLFW_PRESS)
	{
		direction += up;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		direction -= up;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_LEFT) == GLFW_PRESS)
	{
		direction -= right;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
		direction += right;
	}


	if (glm::length(direction) != 0.0f)
	{
		direction = glm::normalize(direction);
	}

	return direction;
}

static v3 getMoveDirection(
	GLFWwindow* _pWindow,
	v3 _forward,
	v3 _up)
{
	v3 direction(0.0f, 0.0f, 0.0f);

	if (glfwGetKey(_pWindow, GLFW_KEY_W) == GLFW_PRESS)
	{
		direction += _forward;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_S) == GLFW_PRESS)
	{
		direction -= _forward;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_A) == GLFW_PRESS)
	{
		direction -= glm::normalize(glm::cross(_forward, _up));
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_D) == GLFW_PRESS)
	{
		direction += glm::normalize(glm::cross(_forward, _up));
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_E) == GLFW_PRESS)
	{
		direction += _up;
	}

	if (glfwGetKey(_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
	{
		direction -= _up;
	}

	if (glm::length(direction) != 0.0f)
	{
		direction = glm::normalize(direction);
	}

	return direction;
}

void updateCamera(
	GLFWwindow* _pWindow,
	f32 _deltaTime,
	Camera& _rCamera)
{
	i32 framebufferWidth;
	i32 framebufferHeight;
	glfwGetFramebufferSize(_pWindow, &framebufferWidth, &framebufferHeight);

	_rCamera.aspect = f32(framebufferWidth) / f32(framebufferHeight);

	v2 lookDirection = getLookDirection(_pWindow);
	_rCamera.yaw += lookDirection.y * _deltaTime * _rCamera.sensitivity;
	_rCamera.pitch += lookDirection.x * _deltaTime * _rCamera.sensitivity;
	_rCamera.pitch = glm::clamp(_rCamera.pitch, -89.99f, 89.99f);

	v3 forward;
	forward.x = glm::cos(glm::radians(_rCamera.yaw)) * glm::cos(glm::radians(_rCamera.pitch));
	forward.y = glm::sin(glm::radians(_rCamera.pitch));
	forward.z = glm::sin(glm::radians(_rCamera.yaw)) * glm::cos(glm::radians(_rCamera.pitch));

	f32 moveSpeed = glfwGetKey(_pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? _rCamera.boostMoveSpeed : _rCamera.moveSpeed;

	v3 up(0.0f, 1.0f, 0.0f);
	v3 moveDirection = getMoveDirection(_pWindow, forward, up);
	_rCamera.position += moveDirection * _deltaTime * moveSpeed;

	_rCamera.view = glm::lookAt(_rCamera.position, _rCamera.position + forward, up);
	_rCamera.projection = getInfinitePerspectiveMatrix(glm::radians(_rCamera.fov), _rCamera.aspect, _rCamera.near);
}

void getFrustumPlanes(
	Camera _camera,
	v4* _pFrustumPlanes)
{
	m4 viewProjectionTransposed = glm::transpose(_camera.projection * _camera.view);

	_pFrustumPlanes[0] = viewProjectionTransposed[3] + viewProjectionTransposed[0];
	_pFrustumPlanes[1] = viewProjectionTransposed[3] - viewProjectionTransposed[0];
	_pFrustumPlanes[2] = viewProjectionTransposed[3] + viewProjectionTransposed[1];
	_pFrustumPlanes[3] = viewProjectionTransposed[3] - viewProjectionTransposed[1];
	_pFrustumPlanes[4] = viewProjectionTransposed[3] - viewProjectionTransposed[2];

	for (u32 frustumIndex = 0; frustumIndex < kFrustumPlaneCount; ++frustumIndex)
	{
		v4& plane = _pFrustumPlanes[frustumIndex];
		plane = plane / glm::length(v3(plane));
	}
}
