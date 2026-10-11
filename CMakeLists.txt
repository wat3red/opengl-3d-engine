cmake_minimum_required(VERSION 3.20)
project(Ignis LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure MSVC runtime library to static /MT
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    # Remove /RTC1 (Basic Runtime Checks) from Debug flags to match /MT static libraries
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

# Source files
set(ENGINE_SOURCES
    src/main.cpp

    # Core
    src/core/engine.cpp
    src/core/project_packager.cpp
    src/core/window.cpp
    src/core/input_handler.cpp
    src/core/input.cpp

    # Renderer
    src/renderer/renderer.cpp
    src/renderer/shadow_renderer.cpp
    src/renderer/shader.cpp
    src/renderer/material.cpp
    src/renderer/texture.cpp
    src/renderer/texture_cube.cpp
    src/renderer/vertex_array.cpp
    src/renderer/vertex_buffer.cpp
    src/renderer/vertex_buffer_layout.cpp
    src/renderer/index_buffer.cpp
    src/renderer/framebuffer.cpp
    src/renderer/model.cpp
    src/renderer/primitive.cpp
    src/renderer/skybox.cpp
    src/renderer/bloom_renderer.cpp

    # Scene
    src/scene/scene.cpp
    src/scene/light.cpp
    src/scene/scene_serializer.cpp
    src/scene/script_registry.cpp

    # Assets
    src/assets/asset_manager.cpp

    # Audio
    src/audio/audio_engine.cpp

    # Physics
    src/physics/rigidbody.cpp
    src/physics/collision.cpp
    src/physics/character_controller.cpp

    # Editor
    src/editor/editor.cpp

    # Scene Game Module
    src/scene/game_module.cpp
    
    # Vendor sources
    vendor/glm/detail/glm.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_impl_glfw.cpp
    vendor/imgui/imgui_impl_opengl3.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/ImGuizmo.cpp
    vendor/std_image/stb_image.cpp
)

# Header files (for IDE visibility in Solution Explorer)
set(ENGINE_HEADERS
    # Core
    src/core/engine.h
    src/core/window.h
    src/core/input_handler.h
    src/core/input.h
    src/core/key_codes.h
    src/core/mouse_codes.h
    src/core/error_handler.h

    # Renderer
    src/renderer/renderer.h
    src/renderer/shadow_renderer.h
    src/renderer/shader.h
    src/renderer/texture.h
    src/renderer/texture_cube.h
    src/renderer/vertex_array.h
    src/renderer/vertex_buffer.h
    src/renderer/vertex_buffer_layout.h
    src/renderer/index_buffer.h
    src/renderer/framebuffer.h
    src/renderer/camera.h
    src/renderer/mesh.h
    src/renderer/model.h
    src/renderer/primitive.h
    src/renderer/skybox.h

    # Scene
    src/scene/scene.h
    src/scene/entity.h
    src/scene/components.h
    src/scene/scriptable_entity.h
    src/scene/script_registry.h
    src/scene/model_entity.h
    src/scene/light.h
    src/scene/transform.h
    src/scene/scene_serializer.h

    # Assets
    src/assets/asset_manager.h

    # Audio
    src/audio/audio_engine.h
    vendor/miniaudio/miniaudio.h

    # Physics
    src/physics/rigidbody.h
    src/physics/collision.h

    # Engine Scripts
    src/scripts/fps_controller.h

    # Game Module
    src/scene/game_module.h

    # Editor
    src/editor/editor.h
)

# Define executable target
add_executable(Ignis
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

# Organize files into folders inside Visual Studio Solution Explorer
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Include directories
target_include_directories(Ignis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
    ${CMAKE_CURRENT_SOURCE_DIR}/src/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/physics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLEW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio
)

# Preprocessor definitions
target_compile_definitions(Ignis PRIVATE
    GLEW_STATIC
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

# Link libraries
target_link_libraries(Ignis PRIVATE
    opengl32
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLEW/lib/Release/x64/glew32s.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLFW/lib-vc2022/glfw3_mt.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/assimp/lib/assimp-vc143-mt.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/assimp/lib/zlibstatic.lib
)

# Set working directory for Visual Studio debugger so shaders & assets load properly
set_target_properties(Ignis PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
