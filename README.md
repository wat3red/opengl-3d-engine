# Ignis Engine

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++23" />
  <img src="https://img.shields.io/badge/OpenGL-4.3%20Core-5586A4?style=for-the-badge&logo=opengl&logoColor=white" alt="OpenGL 4.3" />
  <img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows x64" />
  <img src="https://img.shields.io/badge/Build-CMake%20%7C%20MSBuild-064F8C?style=for-the-badge&logo=cmake&logoColor=white" alt="CMake & MSBuild" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License" />
</p>

<p align="center">
  <strong>Ignis</strong> is a modern, high-performance 3D rendering engine built with <strong>C++23</strong>, <strong>OpenGL 4.3 Core</strong>, and <strong>Dear ImGui</strong>. It features a high-dynamic-range (HDR) color pipeline, filmic ACES tone mapping, Cook-Torrance GGX Physically Based Rendering (PBR), tangent-space normal mapping, cascaded shadow maps (CSM), and an integrated scene editor.
</p>

---

## 🎬 Showcase

<!-- ================================================================================= -->
<!-- DEMO / SHOWCASE MEDIA PLACEHOLDER                                                 -->
<!-- Place your demonstration GIF or video recording at: docs/showcase.gif             -->
<!-- You can also link an external URL (e.g. hosted on GitHub Releases, Imgur, etc.)   -->
<!-- ================================================================================= -->

<p align="center">
  <img src="docs/showcase.gif" alt="Ignis Engine Real-Time PBR & Editor Showcase" width="90%" />
</p>
<p align="center">
  <em>Real-time Cook-Torrance PBR shading, dynamic shadows, and interactive scene editing in Ignis.</em>
</p>

<details open>
<summary><b>📸 Additional Screenshots & Demonstrations</b></summary>
<br>

<p align="center">
  <!-- Secondary preview placeholder: Replace docs/editor_viewport.png with your screenshot -->
  <img src="docs/editor_viewport.png" alt="Ignis Editor & Hierarchy" width="48%" />
  &nbsp;
  <!-- Secondary preview placeholder: Replace docs/shadows_csm.png with your screenshot -->
  <img src="docs/shadows_csm.png" alt="Cascaded Shadow Mapping Debug View" width="48%" />
</p>

</details>

---

## ✨ Features

### 🌟 Physically Based Rendering (PBR) & Material Pipeline
- **Cook-Torrance Microfacet BRDF**:
  - **Normal Distribution ($D$)**: Trowbridge-Reitz GGX.
  - **Geometric Shadowing ($G$)**: Smith-Schlick GGX with direct lighting formulation.
  - **Fresnel Reflectance ($F$)**: Fresnel-Schlick with conductor/dielectric interpolation ($F_0 = \text{mix}(0.04, \text{albedo}, \text{metallic})$).
  - **Energy Conservation**: Lambertian diffuse component strictly balanced with specular reflection ($k_d = (1 - k_s)(1 - \text{metallic})$).
- **Tangent-Space Normal Mapping**: Accurate per-pixel perturbation computed via vertex TBN matrices with Gram-Schmidt orthogonalization.
- **glTF 2.0 PBR Asset Pipeline**: Full material ingestion for:
  - Base Color / Albedo (`aiTextureType_BASE_COLOR` / `aiTextureType_DIFFUSE`)
  - Metallic-Roughness (glTF packed channel standard: Green = Roughness, Blue = Metallic)
  - Tangent Normal maps
  - Ambient Occlusion (AO)
  - Dynamic scalar factors (`baseColorFactor`, `metallicFactor`, `roughnessFactor`)
- **Smart Fallback Texture Architecture**: Automated 1x1 default textures (White albedo, flat normal, standard metallic-roughness, white AO) guaranteeing valid texture units across all draw passes.

### 🌈 HDR Floating-Point Pipeline & Filmic Tone Mapping
- **16-bit Float Framebuffer (`GL_RGBA16F`)**: Preserves radiant light intensities $> 1.0$ without clamping or banding.
- **Post-Processing Pass**:
  - **ACES Filmic Curve**: Standard academy filmic color curve with natural highlight roll-off and deep contrast.
  - **Reinhard Operator**: Alternative classic tone reproduction.
  - **Real-Time Camera Exposure**: Interactive scalar exposure control ($0.01\times$ to $10.0\times$).
  - **Gamma Correction**: Accurate sRGB display conversion ($pow(\text{color}, 1.0 / 2.2)$).

### 🌑 Dynamic Shadow Mapping Subsystem
- **Cascaded Shadow Mapping (CSM)**:
  - 4 directional light cascade splits fitted dynamically to the camera view frustum.
  - **Poisson 16-sample PCF**: Stochastic disk multisampling for soft, artifact-free penumbra.
  - Configurable shadow bias and cascade split lambda.
  - **Frustum Debug Mode**: Color-coded visualization of cascade levels in real time.
- **Omnidirectional Point Light Shadows**:
  - Dynamic cubemap depth rendering (`GL_TEXTURE_CUBE_MAP`).
  - Distance attenuation with far-plane depth comparison.
- **Spotlights**: Realistic dual-cone angle cutoffs and smooth penumbra falloff.

### 🛠️ Integrated Scene Editor (Dear ImGui)
- **Scene Viewport**: Off-screen framebuffer target with interactive aspect ratio preservation and dynamic window resizing.
- **Scene Hierarchy**: Interactive tree view of all loaded entities and lights.
- **Entity Inspector**: Real-time manipulation of:
  - Transform components (Position, Rotation, Scale).
  - Light parameters (Color, Intensity, Direction, Attenuation, Cutoffs).
- **Lighting & Atmosphere Controls**:
  - Directional Sun orientation, color, and radiant intensity.
  - Skybox cubemap toggle.
  - Shadow resolution quality presets (Very Low to Ultra).
  - Post-processing tonemapping operators, exposure, and gamma sliders.
- **Performance Telemetry**: Live FPS, CPU frame time (ms), and GPU render time (ms).
- **Fly Camera**: Smooth 6-DoF editor navigation with configurable movement speed and field-of-view (FoV).

---

## 🏗️ Architecture

```mermaid
flowchart TD
    subgraph Scene ["Scene Graph"]
        Entities["Model Entities & Meshes"]
        Mats["glTF 2.0 PBR Materials\n(Albedo, Normal, Metallic/Roughness, AO)"]
        Lights["Directional, Point, Spot Lights"]
    end

    subgraph Shadows ["Shadow Pipeline"]
        CSM["Cascaded Shadow Mapping\n(4 Splits + Poisson 16 PCF)"]
        PointShadows["Omnidirectional Point Shadows\n(Dynamic Cubemaps)"]
    end

    subgraph RenderPass ["HDR Lighting Pass (GL_RGBA16F)"]
        PBR["Cook-Torrance GGX Shader\n- Normal Distribution (D)\n- Smith Geometry (G)\n- Fresnel-Schlick (F)\n- Tangent-Space Normal Mapping"]
        HDRFBO["HDR Framebuffer\n(GL_RGBA16F + Depth24Stencil8)"]
    end

    subgraph PostProcess ["Post-Processing Pass"]
        ToneShader["Tone Mapping Shader\n- ACES Filmic Curve\n- Camera Exposure\n- Gamma 2.2 sRGB Correction"]
        LDRFBO["LDR Framebuffer / ImGui Scene Viewport"]
    end

    Scene --> Shadows
    Shadows --> RenderPass
    Scene --> RenderPass
    RenderPass --> HDRFBO
    HDRFBO --> PostProcess
    PostProcess --> LDRFBO
```

---

## 🚀 Getting Started

### Prerequisites
- **Operating System**: Windows 10/11 (x64)
- **Compiler**: Visual Studio 2022 / 2026 (MSVC with **C++23** support: `/std:c++23preview` or `/std:c++23`)
- **CMake**: Version 3.20 or newer (optional, if building with CMake)
- **Graphics Hardware**: GPU supporting **OpenGL 4.3 Core** or higher

### 🔨 Building the Engine

#### Method 1: Visual Studio Solution (Recommended)
1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/ignis-engine.git
   cd ignis-engine
   ```
2. Open `Ignis.sln` in Visual Studio.
3. Select **Release** and **x64** in the configuration toolbar.
4. Press `Ctrl + Shift + B` (or `Build -> Build Solution`).
5. Run with `F5` (or execute `x64/Release/Ignis.exe`).

#### Method 2: CMake with Presets
Ignis provides pre-configured CMake presets for rapid building:

```bash
# Configure using the release preset
cmake --preset x64-release

# Build the project
cmake --build --preset x64-release
```
The compiled executable will be located in `build/Release/Ignis.exe`.

---

## 🎮 Editor Controls

| Input | Action |
| :--- | :--- |
| **Right Click + Drag** | Look around (Fly camera) |
| **W / A / S / D** | Move camera forward / left / backward / right |
| **Q / E** | Move camera down / up |
| **Mouse Scroll** | Adjust Field of View (Zoom) |
| **Left Click (Hierarchy)** | Select entity to view in Inspector |
| **Left Click (Viewport)** | Focus viewport panel |

---

## 📁 Project Structure

```
ignis-engine/
├── docs/                # Project documentation and showcase media (showcase.gif)
├── res/                 # Scene assets, 3D models (glTF/GLB/FBX), textures, skybox
├── shaders/             # GLSL shader programs
│   ├── Main.shader      # Cook-Torrance GGX PBR + Normal Mapping + CSM & Point Shadows
│   ├── ToneMap.shader   # Fullscreen ACES Filmic tone mapping & exposure pass
│   ├── Light.shader     # Point light visualizer cubes
│   └── ...              # Shadow depth mapping shaders
├── src/
│   ├── core/            # Engine runtime lifecycle, window management, input handler
│   ├── editor/          # Dear ImGui scene hierarchy, inspector, environment controls
│   ├── renderer/        # Framebuffer, Mesh, Model, Primitive, Shader, Renderer, Shadows
│   └── scene/           # Entity graph, Lights, Skybox, Scene abstraction
├── vendor/              # Third-party libraries (Assimp, ImGui, GLM, GLEW, GLFW, STB)
├── CMakeLists.txt       # CMake build configuration
├── CMakePresets.json    # Standardized CMake build presets
└── Ignis.sln            # Visual Studio solution file
```

---

## 🗺️ Roadmap
- [x] Modern C++23 standardization & clean architecture
- [x] HDR Floating-Point pipeline (`GL_RGBA16F`)
- [x] ACES Filmic tone mapping & exposure controls
- [x] Cook-Torrance GGX PBR multi-light forward shading
- [x] Tangent-space normal mapping & glTF 2.0 PBR material parsing
- [x] Cascaded Shadow Mapping (CSM) with Poisson 16 PCF
- [ ] **Image-Based Lighting (IBL)**: Diffuse Irradiance Convolution & Pre-filtered Environment Specular Reflections
- [ ] **Cinematic Post-Processing**: Physically-Based Bloom pyramid & Screen-Space Ambient Occlusion (SSAO)
- [ ] **Performance Culling**: CPU View Frustum Culling via AABB/OBB
- [ ] **Editor Enhancements**: Interactive 3D Transform Gizmos (ImGuizmo) & Runtime Material Editor

---

## 📜 License & Credits

This project is licensed under the [MIT License](LICENSE).

### Third-Party Libraries
- [GLFW](https://www.glfw.org/) - Windowing and input handling
- [GLEW](https://glew.sourceforge.net/) - OpenGL Extension Wrangler
- [GLM](https://github.com/g-truc/glm) - OpenGL Mathematics
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode graphical user interface
- [Assimp](https://github.com/assimp/assimp) - Open Asset Import Library
- [stb_image](https://github.com/nothings/stb) - Image loading library