# Ignis Engine

**Ignis** is a modular, high-performance C++17 3D rendering engine built on modern OpenGL 4.3 Core, GLFW, and Dear ImGui.

## Features
- **Cascaded Shadow Mapping (CSM)**: Directional light shadow cascades with PCF filtering and customizable bias/split lambda.
- **Omnidirectional Point Light Shadows**: Dynamic depth cubemaps for point light attenuation and soft shadowing.
- **PBR & Blinn-Phong Shading**: Multi-light forward rendering supporting Directional, Point, and Spotlight sources with alpha-cutout testing.
- **Integrated Scene Editor**:
  - Interactive Viewport panel with off-screen Framebuffer rendering.
  - Entity Inspector (Transform, Mesh, and Light component editing).
  - Scene Hierarchy.
  - Environment & Atmosphere controls (Dynamic Sun, Skybox toggling, Shadow resolution & quality presets).
  - Real-time performance metrics (Framerate, Frame Time, GPU Render Time).
- **Asset Pipeline**: Fast model and material loading via Assimp (glTF 2.0 / GLB, OBJ, embedded textures) with smart caching.
- **Modular OOP Architecture**: Clean decoupling between core Engine runtime, Window context, Renderer passes, and Editor tooling.

## Building
### Visual Studio (MSBuild)
Open `Ignis.sln` and build with configuration `Release|x64` or `Debug|x64`.

### CMake
```bash
cmake --preset x64-release
cmake --build --preset x64-release
```