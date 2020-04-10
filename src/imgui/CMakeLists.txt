cmake_minimum_required(VERSION 3.10)

project(ImGui VERSION 1.0.0)

# Headers
include_directories(
    "./"
    "examples"
    "examples/libs"
    "examples/libs/glfw/include"
)

# App source files
set(IMGUI_SOURCES
    examples/imgui_impl_opengl2.cpp
    examples/imgui_impl_glfw.cpp
    imgui.cpp
#    imgui_demo.cpp
    imgui_draw.cpp
    imgui_widgets.cpp  
)

# Compile main NanoGUI library
add_library(${PROJECT_NAME} OBJECT
  ${IMGUI_SOURCES}
)