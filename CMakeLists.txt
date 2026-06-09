# CMake minimum version
cmake_minimum_required(VERSION 3.14)

project(MarkdownEditor LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt – try Qt6 first (optional), then fall back to Qt5 (required)
find_package(Qt6 COMPONENTS Widgets PrintSupport QUIET)
if (Qt6_FOUND)
    set(QtLibs Qt6::Widgets Qt6::PrintSupport)
else()
    find_package(Qt5 COMPONENTS Widgets PrintSupport REQUIRED)
    set(QtLibs Qt5::Widgets Qt5::PrintSupport)
endif()

set(CMAKE_AUTOMOC ON)

add_executable(mdedit-gui src/main.cpp)
target_link_libraries(mdedit-gui PRIVATE ${QtLibs})

# Optional install rule
install(TARGETS mdedit-gui DESTINATION bin)
