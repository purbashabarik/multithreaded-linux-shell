cmake_minimum_required(VERSION 3.16)
project(eds LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generates compile_commands.json for clangd / IDE intellisense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Core library (shared between CLI and future GUI) ---
file(GLOB EDS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
# Remove main.cpp from the library sources
list(FILTER EDS_SOURCES EXCLUDE REGEX "main\\.cpp$")

add_library(eds_core STATIC ${EDS_SOURCES})
target_include_directories(eds_core PUBLIC include)
find_package(Threads REQUIRED)
target_link_libraries(eds_core PUBLIC Threads::Threads)

# --- CLI executable ---
add_executable(eds src/main.cpp)
target_link_libraries(eds PRIVATE eds_core)

# --- Sanitizers in Debug mode ---
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(eds_core PRIVATE -fsanitize=address,undefined -O0 -g)
    target_link_options(eds_core PUBLIC -fsanitize=address,undefined)
endif()

# --- Qt GUI Frontend (optional) ---
option(EDS_BUILD_GUI "Build the Qt GUI frontend" ON)
if(EDS_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        qt_standard_project_setup()
        add_subdirectory(gui)
        message(STATUS "Qt6 found — building eds-gui")
    else()
        message(STATUS "Qt6 not found — skipping GUI build (install Qt6 and re-run cmake)")
    endif()
endif()

# --- Tests ---
enable_testing()
add_subdirectory(tests)
