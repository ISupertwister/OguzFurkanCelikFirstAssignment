📝 DX12 Editor / Mini Scene Viewer (Assignment 2)

🎯 Project Overview

This project is a DirectX 12–based mini editor and scene viewer developed as part of the **Advanced Programming for AAA Games** course at **UPC**.

The goal of this assignment is to extend a basic DX12 renderer into an editor-style application featuring offscreen rendering, scene visualization, object manipulation, and real-time debugging tools.

The application demonstrates a modern DirectX 12 rendering pipeline combined with editor-style camera controls and ImGui-based UI.

---

🖼️ Main Features

✔️ **DirectX 12 Rendering Pipeline**
- Device & adapter selection
- Swap chain with double buffering
- Command queue, allocator & command lists
- Fence-based GPU synchronization
- RTV / DSV descriptor heaps
- Root signature & multiple PSOs
- Depth buffer support

✔️ **Offscreen Rendering (Render To Texture)**
- Scene is rendered into a custom render target
- The render target is displayed inside an ImGui "Scene" window
- Enables editor-style viewport rendering

✔️ **Textured Ground Quad**
- Procedural checkerboard texture
- UVs extended beyond [0–1] range to visualize sampler behavior
- Real-time sampler switching via ImGui

✔️ **Sampler Modes (Static Samplers)**
Four sampler modes selectable at runtime:
- Linear / Wrap
- Point / Wrap
- Linear / Clamp
- Point / Clamp

Implemented using **static samplers inside the root signature**.

✔️ **Phong-Shaded Mesh Rendering**
- GLTF mesh loading (Duck model)
- Per-pixel Phong lighting
- Adjustable material & light parameters
- World, View, Projection matrices
- Camera position passed to shader

✔️ **Editor-Style Camera System**
- FPS Mode (WASD + mouse look)
- Orbit Mode (Alt + LMB)
- Focus Mode (F key)
- Mouse wheel zoom
- Smooth movement & rotation

✔️ **ImGui Debug Interface**
- FPS counter
- Camera position display
- Grid & axis toggles
- Sampler selection combo box
- Lighting & material controls
- Transform controls (position / rotation / scale)

✔️ **ImGuizmo Integration**
- Translate / Rotate / Scale gizmos
- Object manipulation inside Scene viewport
- World-space transformations

---

⌨️ Controls

| Action | Input |
|------|------|
| Look Around (FPS) | Right Mouse Button + Move |
| Move | W / A / S / D |
| Up / Down | E / Q |
| Fast Move | Shift |
| Orbit Camera | Alt + Left Mouse Button |
| Zoom | Mouse Wheel |
| Focus Object | F |
| Gizmo Translate | ImGui |
| Gizmo Rotate | ImGui |
| Gizmo Scale | ImGui |

---

📁 Project Structure

DX12Editor/

├─ ImGui/

├─ src/

│ ├─ App/

│ │ ├─ Main.cpp

│ │ ├─ Window.cpp

│ │ └─ Window.h

│ ├─ Core/

│ │ ├─ Camera.cpp / .h

│ │ ├─ DXDevice.cpp / .h

│ │ ├─ DXRenderer.cpp / .h

│ │ ├─ RenderTarget.cpp / .h

│ │ ├─ DXModelMesh.cpp / .h

│ │ └─ FrameTimer.cpp / .h

│ └─ Shaders/

│ ├─ ColorVS.hlsl

│ ├─ ColorPS.hlsl

│ ├─ PhongVS.hlsl

│ └─ PhongPS.hlsl

├─ DX12Editor.sln

└─ README.md

⚙️ Build Instructions

**Requirements**
- Windows 10 / 11
- Visual Studio 2022
- Windows SDK
- DirectX 12 capable GPU
- x64 build configuration

**Steps**
1. Open `DX12Editor.sln`
2. Select `Release | x64`
3. Build the solution
4. Run `DX12Editor.exe`

---

👑 Author

**Oğuz Furkan Çelik**  
Advanced Programming for AAA Games  
Universitat Politècnica de Catalunya (UPC)
