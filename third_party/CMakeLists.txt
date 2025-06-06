# Third-party dependencies

# ymfm will be added as a git submodule
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ymfm/CMakeLists.txt)
    add_subdirectory(ymfm)
else()
    message(STATUS "ymfm not found. Will be added as git submodule.")
endif()