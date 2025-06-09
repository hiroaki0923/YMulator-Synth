cmake_minimum_required(VERSION 3.22)
project(ChipSynthAU VERSION 0.0.2)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_STANDALONE "Build standalone application" OFF)

# Prevent system-wide installation by default
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

# Find packages
find_package(PkgConfig REQUIRED)

# Add subdirectories
add_subdirectory(cmake)
add_subdirectory(third_party)
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# CPack configuration for installer
set(CPACK_PACKAGE_NAME "ChipSynth AU")
set(CPACK_PACKAGE_VENDOR "Hiroaki")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FM Synthesis Audio Unit")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_GENERATOR "DragNDrop")
include(CPack)