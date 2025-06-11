# Third-party dependencies

# ymfm is included directly in src/CMakeLists.txt as source files
# Check if ymfm directory exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ymfm/src/ymfm.h)
    message(STATUS "ymfm found and will be included as source files.")
else()
    message(STATUS "ymfm not found. Please run: git submodule update --init --recursive")
endif()