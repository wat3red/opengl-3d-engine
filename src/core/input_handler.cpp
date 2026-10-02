// input_handler.cpp
#include "input_handler.h"
#include <iostream>
#include <imgui/imgui.h>

InputHandler::InputHandler(Camera& camera)
	: m_Camera(camera),
	  m_LastX(800.0f),
	  m_LastY(450.0f),
	  m_LastFrameTime(0.0f),
	  m_FirstMouse(true)
{
}

InputHandler::InputHandler(Camera& camera, glm::ivec2* windowSize)
	: InputHandler(camera)
{
	if (windowSize)
	{
		m_LastX = windowSize->x * 0.5f;
		m_LastY = windowSize->y * 0.5f;
	}
}

void InputHandler::Update(GLFWwindow* window, float deltaTime)
{
	if (ImGui::GetIO().WantCaptureKeyboard && !m_Camera.isCapturingMovements)
	{
		return;
	}

#define IS_PRESSED(key) (glfwGetKey(window, key) == GLFW_PRESS)

	if (IS_PRESSED(GLFW_KEY_ESCAPE))
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	if (IS_PRESSED(GLFW_KEY_W))
		m_Camera.ProcessKeyboard(FORWARD, deltaTime);
	if (IS_PRESSED(GLFW_KEY_S))
		m_Camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (IS_PRESSED(GLFW_KEY_A))
		m_Camera.ProcessKeyboard(LEFT, deltaTime);
	if (IS_PRESSED(GLFW_KEY_D))
		m_Camera.ProcessKeyboard(RIGHT, deltaTime);
	if (IS_PRESSED(GLFW_KEY_SPACE))
		m_Camera.ProcessKeyboard(UP, deltaTime);
	if (IS_PRESSED(GLFW_KEY_LEFT_CONTROL))
		m_Camera.ProcessKeyboard(DOWN, deltaTime);

#undef IS_PRESSED
}

void InputHandler::Update(GLFWwindow* window)
{
	float currentFrameTime = static_cast<float>(glfwGetTime());
	float deltaTime = currentFrameTime - m_LastFrameTime;
	m_LastFrameTime = currentFrameTime;

	Update(window, deltaTime);
}

void InputHandler::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
}

void InputHandler::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		if (!m_Camera.isCapturingMovements)
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported())
			{
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}

			double xpos = 0.0;
			double ypos = 0.0;
			glfwGetCursorPos(window, &xpos, &ypos);
			m_LastX = static_cast<float>(xpos);
			m_LastY = static_cast<float>(ypos);
			m_FirstMouse = true;
		}
		else
		{
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			m_FirstMouse = true;
		}
		m_Camera.isCapturingMovements = !m_Camera.isCapturingMovements;
	}
}

void InputHandler::MouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
	if (!m_Camera.isCapturingMovements)
	{
		return;
	}

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	if (m_FirstMouse)
	{
		m_LastX = xpos;
		m_LastY = ypos;
		m_FirstMouse = false;
	}

	float xoffset = xpos - m_LastX;
	float yoffset = m_LastY - ypos;

	m_LastX = xpos;
	m_LastY = ypos;

	m_Camera.ProcessMouseMovement(xoffset, yoffset);
}

void InputHandler::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (!m_Camera.isCapturingMovements)
	{
		return;
	}

	m_Camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputHandler::SetCallbacks(GLFWwindow* window)
{
	glfwSetScrollCallback(window, [](GLFWwindow* win, double xoff, double yoff) {
		auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(win));
		if (handler)
		{
			handler->ScrollCallback(win, xoff, yoff);
		}
	});

	glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
		auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(win));
		if (handler)
		{
			handler->MouseCallback(win, xpos, ypos);
		}
	});

	glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
		auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(win));
		if (handler)
		{
			handler->MouseButtonCallback(win, button, action, mods);
		}
	});

	glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		auto* handler = static_cast<InputHandler*>(glfwGetWindowUserPointer(win));
		if (handler)
		{
			handler->KeyCallback(win, key, scancode, action, mods);
		}
	});

	glfwSetWindowUserPointer(window, this);
}
