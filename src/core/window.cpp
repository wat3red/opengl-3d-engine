#include "window.h"
#include "error_handler.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

int Window::s_GLFWWindowCount = 0;

Window::Window(const WindowProperties& properties)
	: m_Properties(properties)
{
	Initialize();
}

Window::~Window()
{
	Shutdown();
}

Window::Window(Window&& other) noexcept
	: m_Window(other.m_Window), m_Properties(std::move(other.m_Properties))
{
	other.m_Window = nullptr;
}

Window& Window::operator=(Window&& other) noexcept
{
	if (this != &other)
	{
		Shutdown();
		m_Window = other.m_Window;
		m_Properties = std::move(other.m_Properties);
		other.m_Window = nullptr;
	}
	return *this;
}

void Window::Initialize()
{
	if (s_GLFWWindowCount == 0)
	{
		if (!glfwInit())
		{
			throw std::runtime_error("Failed to initialize GLFW");
		}
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

	m_Window = glfwCreateWindow(
		static_cast<int>(m_Properties.Width),
		static_cast<int>(m_Properties.Height),
		m_Properties.Title.c_str(),
		nullptr,
		nullptr
	);

	if (!m_Window)
	{
		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
		throw std::runtime_error("Failed to create GLFW window");
	}

	++s_GLFWWindowCount;

	glfwMakeContextCurrent(m_Window);

	glewExperimental = GL_TRUE;
	GLenum glewStatus = glewInit();
	if (glewStatus != GLEW_OK)
	{
		Shutdown();
		throw std::runtime_error(std::string("Failed to initialize GLEW: ") + reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
	}

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GLDebugMessageCallback, nullptr);

	SetVSync(m_Properties.VSync);
}

void Window::Shutdown()
{
	if (m_Window)
	{
		glfwDestroyWindow(m_Window);
		m_Window = nullptr;
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0)
		{
			glfwTerminate();
		}
	}
}

void Window::OnUpdate()
{
	SwapBuffers();
	PollEvents();
}

void Window::SwapBuffers() const
{
	if (m_Window)
	{
		glfwSwapBuffers(m_Window);
	}
}

void Window::PollEvents() const
{
	glfwPollEvents();
}

bool Window::ShouldClose() const
{
	return m_Window ? (glfwWindowShouldClose(m_Window) != 0) : true;
}

void Window::SetShouldClose(bool close)
{
	if (m_Window)
	{
		glfwSetWindowShouldClose(m_Window, close ? GLFW_TRUE : GLFW_FALSE);
	}
}

uint32_t Window::GetWidth() const
{
	return static_cast<uint32_t>(GetSize().x);
}

uint32_t Window::GetHeight() const
{
	return static_cast<uint32_t>(GetSize().y);
}

glm::ivec2 Window::GetSize() const
{
	if (m_Window)
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		return glm::ivec2(width, height);
	}
	return glm::ivec2(static_cast<int>(m_Properties.Width), static_cast<int>(m_Properties.Height));
}

void Window::SetVSync(bool enabled)
{
	m_Properties.VSync = enabled;
	glfwSwapInterval(enabled ? 1 : 0);
}
