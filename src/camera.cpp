#include "common.h"
#include "camera.h"
#include "utils.h"

static glm::vec2 getLookDirection(
	GLFWwindow* _pWindow)
{
	glm::vec2 direction(0.0f, 0.0f);
	glm::vec2 right(0.0f, 1.0f);
	glm::vec2 up(1.0f, 0.0f);

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

static glm::vec3 getMoveDirection(
	GLFWwindow* _pWindow,
	glm::vec3 _forward,
	glm::vec3 _up)
{
	glm::vec3 direction(0.0f, 0.0f, 0.0f);

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
	float _deltaTime,
	Camera& _rCamera)
{
	glm::vec2 lookDirection = getLookDirection(_pWindow);
	_rCamera.yaw += lookDirection.y * _deltaTime * _rCamera.sensitivity;
	_rCamera.pitch += lookDirection.x * _deltaTime * _rCamera.sensitivity;
	_rCamera.pitch = glm::clamp(_rCamera.pitch, -89.99f, 89.99f);

	glm::vec3 forward;
	forward.x = glm::cos(glm::radians(_rCamera.yaw)) * glm::cos(glm::radians(_rCamera.pitch));
	forward.y = glm::sin(glm::radians(_rCamera.pitch));
	forward.z = glm::sin(glm::radians(_rCamera.yaw)) * glm::cos(glm::radians(_rCamera.pitch));

	glm::vec3 up(0.0f, 1.0f, 0.0f);
	glm::vec3 moveDirection = getMoveDirection(_pWindow, forward, up);
	_rCamera.position += moveDirection * _deltaTime * _rCamera.moveSpeed;

	_rCamera.view = glm::lookAt(_rCamera.position, _rCamera.position + forward, up);
	_rCamera.projection = getInfinitePerspectiveMatrix(glm::radians(_rCamera.fov), _rCamera.aspect, _rCamera.near);
}

void getFrustumPlanes(
	Camera _camera,
	glm::vec4* _pFrustumPlanes)
{
	glm::mat4 viewProjectionTransposed = glm::transpose(_camera.projection * _camera.view);

	_pFrustumPlanes[0] = viewProjectionTransposed[3] + viewProjectionTransposed[0];
	_pFrustumPlanes[1] = viewProjectionTransposed[3] - viewProjectionTransposed[0];
	_pFrustumPlanes[2] = viewProjectionTransposed[3] + viewProjectionTransposed[1];
	_pFrustumPlanes[3] = viewProjectionTransposed[3] - viewProjectionTransposed[1];
	_pFrustumPlanes[4] = viewProjectionTransposed[3] - viewProjectionTransposed[2];
	_pFrustumPlanes[5] = glm::vec4(0.0);

	for (uint32_t frustumIndex = 0; frustumIndex < 6; ++frustumIndex)
	{
		glm::vec4& plane = _pFrustumPlanes[frustumIndex];
		plane = plane / glm::length(glm::vec3(plane));
	}
}
