cmake_minimum_required(VERSION 3.20)

project(
    homework
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Point CMake to your custom Boost installation
# Use CMAKE_PREFIX_PATH or BOOST_ROOT as hints for find_package
set(BOOST_ROOT "$ENV{HOME}/Toolbox/boost_1_90_0")
set(Boost_NO_SYSTEM_PATHS ON) # Prevent using older system-wide Boost

# Find Boost (Add COMPONENTS like 'system' or 'filesystem' if needed)
find_package(Boost 1.90.0 REQUIRED)


include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Make Boost available to all subdirectories
# For header-only parts of Boost:
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

add_subdirectory(src)
