#pragma once

#include "api.h"
#include <glm/glm.hpp>
#include <cstdint>

// Re-export or include KeyCode, MouseCode, CursorMode
#include "core/key_codes.h"
#include "core/mouse_codes.h"

namespace Ignis {

class Input {
public:
    static bool IsKeyPressed(KeyCode key)
    {
        if (GetContext() && GetContext()->Input.IsKeyPressed)
            return GetContext()->Input.IsKeyPressed(static_cast<int>(key));
        return false;
    }

    static bool IsMouseButtonPressed(MouseCode button)
    {
        if (GetContext() && GetContext()->Input.IsMouseButtonPressed)
            return GetContext()->Input.IsMouseButtonPressed(static_cast<int>(button));
        return false;
    }

    static glm::vec2 GetMouseDelta()
    {
        if (GetContext() && GetContext()->Input.GetMouseDelta)
            return GetContext()->Input.GetMouseDelta();
        return glm::vec2(0.0f);
    }

    static void SetCursorMode(CursorMode mode)
    {
        if (GetContext() && GetContext()->Input.SetCursorMode)
            GetContext()->Input.SetCursorMode(static_cast<int>(mode));
    }
};

} // namespace Ignis

// Global convenience alias for game scripts
using Input = Ignis::Input;
