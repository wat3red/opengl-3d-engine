cmake_minimum_required(VERSION 3.20)
project(OpenGL3DEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure MSVC runtime library to static /MT
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
    # Remove /RTC1 (Basic Runtime Checks) from Debug flags to match /MT static libraries
    string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

# Source files
set(ENGINE_SOURCES
    asset_manager.cpp
    cubemap.cpp
    editor.cpp
    framebuffer.cpp
    game.cpp
    index_buffer.cpp
    input_handler.cpp
    light.cpp
    main.cpp
    model.cpp
    renderer.cpp
    scene.cpp
    shader.cpp
    shadow_renderer.cpp
    vertex_array.cpp
    vertex_buffer.cpp
    vertex_buffer_layout.cpp
    physics/rigidbody.cpp
    
    # Vendor sources
    vendor/glm/detail/glm.cpp
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_demo.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_impl_glfw.cpp
    vendor/imgui/imgui_impl_opengl3.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/std_image/stb_image.cpp
)

# Header files (for IDE visibility in Solution Explorer)
set(ENGINE_HEADERS
    asset_manager.h
    camera.h
    cubemap.h
    editor.h
    entity.h
    error_handler.h
    framebuffer.h
    game.h
    index_buffer.h
    input_handler.h
    light.h
    mesh.h
    model.h
    model_entity.h
    renderer.h
    scene.h
    shader.h
    shadow_renderer.h
    transform.h
    vertex_array.h
    vertex_buffer.h
    vertex_buffer_layout.h
    physics/rigidbody.h
)

# Define executable target
add_executable(Engine
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

# Organize files into folders inside Visual Studio Solution Explorer
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ENGINE_SOURCES} ${ENGINE_HEADERS})

# Include directories
target_include_directories(Engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLEW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

# Preprocessor definitions
target_compile_definitions(Engine PRIVATE
    GLEW_STATIC
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>
)

# Link libraries
target_link_libraries(Engine PRIVATE
    opengl32
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLEW/lib/Release/x64/glew32s.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/GLFW/lib-vc2022/glfw3_mt.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/assimp/lib/assimp-vc143-mt.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/assimp/lib/zlibstatic.lib
)

# Set working directory for Visual Studio debugger so shaders & assets load properly
set_target_properties(Engine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
