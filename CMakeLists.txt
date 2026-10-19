cmake_minimum_required(VERSION 3.21)
project(worms3d VERSION 1.0.0 LANGUAGES CXX)

# WORMS 3D — serpente tridimensionale in un'arena cartesiana.
# Build: cmake -S . -B build && cmake --build build -j
# Run:   ./build/worms3d

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Multimedia)

# Il mondo di gioco e' puro C++: nessun Qt, nessun GL, testabile headless.
add_library(worms_core STATIC
    src/core/world.cpp
    src/core/food.cpp
    src/core/effects.cpp
    src/core/snake.cpp
    src/core/autopilot.cpp
)
target_include_directories(worms_core PUBLIC src)
target_compile_options(worms_core PRIVATE -Wall -Wextra)

add_executable(worms3d
    src/main.cpp
    src/app/game_widget.cpp
    src/app/hud.cpp
    src/audio/engine.cpp
    src/render/renderer.cpp
    src/render/mesh.cpp
    src/render/gl.cpp
    shaders.qrc
)
target_link_libraries(worms3d PRIVATE
    worms_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Multimedia
)
target_compile_options(worms3d PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Test della simulazione: solo C++ standard (ne' Qt ne' display).
add_executable(worms_selftest tests/selftest.cpp)
target_link_libraries(worms_selftest PRIVATE worms_core)
target_compile_options(worms_selftest PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME world COMMAND worms_selftest)

install(TARGETS worms3d RUNTIME DESTINATION bin)
