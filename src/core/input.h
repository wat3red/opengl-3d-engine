#pragma once

#include <glm/glm.hpp>
#include "key_codes.h"
#include "mouse_codes.h"

class Input {
public:
	static bool IsKeyPressed(KeyCode key);
	static bool IsMouseButtonPressed(MouseCode button);

	static glm::vec2 GetMousePosition();
	static float GetMouseX();
	static float GetMouseY();

	static glm::vec2 GetMouseDelta();

	static void SetCursorMode(CursorMode mode);
	static CursorMode GetCursorMode();

	// Engine internal hooks
	static void Init(void* nativeWindow);
	static void Update();
};
