#include "input.h"
#include <GLFW/glfw3.h>

namespace {
	GLFWwindow* s_Window = nullptr;
	glm::vec2 s_LastMousePos{ 0.0f, 0.0f };
	glm::vec2 s_CurrentMousePos{ 0.0f, 0.0f };
	glm::vec2 s_MouseDelta{ 0.0f, 0.0f };
	bool s_FirstMouse = true;
	CursorMode s_CursorMode = CursorMode::Normal;
}

void Input::Init(void* nativeWindow)
{
	s_Window = static_cast<GLFWwindow*>(nativeWindow);
	s_FirstMouse = true;
	if (s_Window)
	{
		double x, y;
		glfwGetCursorPos(s_Window, &x, &y);
		s_CurrentMousePos = { static_cast<float>(x), static_cast<float>(y) };
		s_LastMousePos = s_CurrentMousePos;
		s_MouseDelta = { 0.0f, 0.0f };
	}
}

void Input::Update()
{
	if (!s_Window) return;

	double x, y;
	glfwGetCursorPos(s_Window, &x, &y);
	glm::vec2 current = { static_cast<float>(x), static_cast<float>(y) };

	if (s_FirstMouse)
	{
		s_LastMousePos = current;
		s_FirstMouse = false;
	}

	s_MouseDelta = current - s_LastMousePos;
	s_LastMousePos = current;
	s_CurrentMousePos = current;
}

bool Input::IsKeyPressed(KeyCode key)
{
	if (!s_Window) return false;
	int state = glfwGetKey(s_Window, static_cast<int>(key));
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsMouseButtonPressed(MouseCode button)
{
	if (!s_Window) return false;
	int state = glfwGetMouseButton(s_Window, static_cast<int>(button));
	return state == GLFW_PRESS;
}

glm::vec2 Input::GetMousePosition()
{
	return s_CurrentMousePos;
}

float Input::GetMouseX()
{
	return s_CurrentMousePos.x;
}

float Input::GetMouseY()
{
	return s_CurrentMousePos.y;
}

glm::vec2 Input::GetMouseDelta()
{
	return s_MouseDelta;
}

void Input::SetCursorMode(CursorMode mode)
{
	if (!s_Window) return;
	s_CursorMode = mode;
	int glfwMode = GLFW_CURSOR_NORMAL;
	if (mode == CursorMode::Hidden) glfwMode = GLFW_CURSOR_HIDDEN;
	else if (mode == CursorMode::Locked) glfwMode = GLFW_CURSOR_DISABLED;

	glfwSetInputMode(s_Window, GLFW_CURSOR, glfwMode);
}

CursorMode Input::GetCursorMode()
{
	return s_CursorMode;
}
