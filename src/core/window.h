#pragma once

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

struct GLFWwindow;

struct WindowProperties {
	std::string Title = "Ignis Engine";
	uint32_t Width = 1600;
	uint32_t Height = 900;
	bool VSync = true;
};

class Window {
public:
	explicit Window(const WindowProperties& properties = WindowProperties{});
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	Window(Window&& other) noexcept;
	Window& operator=(Window&& other) noexcept;

	void OnUpdate();
	void SwapBuffers() const;
	void PollEvents() const;

	[[nodiscard]] bool ShouldClose() const;
	void SetShouldClose(bool close);

	[[nodiscard]] uint32_t GetWidth() const;
	[[nodiscard]] uint32_t GetHeight() const;
	[[nodiscard]] glm::ivec2 GetSize() const;

	[[nodiscard]] bool IsVSync() const { return m_Properties.VSync; }
	void SetVSync(bool enabled);

	[[nodiscard]] bool IsFullscreen() const { return m_IsFullscreen; }
	void SetFullscreen(bool fullscreen);
	void ToggleFullscreen() { SetFullscreen(!m_IsFullscreen); }
	void SetTitle(const std::string& title);

	[[nodiscard]] GLFWwindow* GetNativeWindow() const { return m_Window; }

private:
	void Initialize();
	void Shutdown();

	GLFWwindow* m_Window = nullptr;
	WindowProperties m_Properties;

	bool m_IsFullscreen = false;
	glm::ivec2 m_WindowedPos{ 100, 100 };
	glm::ivec2 m_WindowedSize{ 1600, 900 };

	static int s_GLFWWindowCount;
};
