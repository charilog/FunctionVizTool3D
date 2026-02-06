FunctionVizTool 3D (standalone)

Qt6 + QOpenGLWidget tool that visualizes an objective function as a 3D surface.

- Expression: math string with variables x0, x1, ...
- Dimension n: defines how many variables exist
- Per-variable bounds (lower/upper) and fixed value
- Select X axis variable and Y axis variable: shows a 2D slice of an n-D function as a 3D surface.

Controls
- Left drag: rotate
- Right drag: pan
- Mouse wheel: zoom
- Wireframe checkbox: wireframe/solid

Build (Windows / VS2022) PowerShell:

cmake -S . -B build_vs `
  -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\msvc2022_64"

cmake --build build_vs --config Release
