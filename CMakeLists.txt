cmake_minimum_required(VERSION 3.21)
project(FunctionVizTool3D VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

qt_standard_project_setup()

qt_add_executable(FunctionVizTool3D
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SurfaceWidget.h
    src/SurfaceWidget.cpp
    src/ObjectiveFunction.h
    src/ObjectiveFunction.cpp
)

target_link_libraries(FunctionVizTool3D PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if (MSVC)
  target_compile_options(FunctionVizTool3D PRIVATE /W4 /permissive-)
else()
  target_compile_options(FunctionVizTool3D PRIVATE -Wall -Wextra -Wpedantic)
endif()
