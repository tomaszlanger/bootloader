cmake_minimum_required(VERSION 3.10)

project(MyApp VERSION 1.0.0)

find_library(glfw3 NAMES glfw3 PATHS "../imgui/examples/libs/glfw/lib-vc2010-32")

find_package(stb REQUIRED PATHS "../stb/lib/cmake/stb")
get_target_property(STB_INCLUDES stb::image INTERFACE_INCLUDE_DIRECTORIES)

# In order to install libraries into correct locations on all platforms.
#include(GNUInstallDirs)

#--------------------------------------------------------------------
# Hide the console window in visual studio projects - Release
#--------------------------------------------------------------------
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Headers
include_directories(  
    "../imgui/"
    "../imgui/examples"
    "../imgui/examples/libs"
    "../imgui/examples/libs/glfw/include"
)

include_directories("${STB_INCLUDES}/stb")

# App source files
set(APP_SOURCES
    main.cpp
    imgui_demo.cpp
)

# Create the executable
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# Enable C++17 features
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

# Link with libraries
target_link_libraries(${PROJECT_NAME} ${glfw3} stb::image ImGui opengl32 gdi32 shell32)

#set_target_properties(
#    MyApp PROPERTIES
#    VS_DEBUGGER_WORKING_DIRECTORY $(TargetDir))

# copy needed resources
add_custom_command(TARGET MyApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${MyApp_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:MyApp>/resources")
