# Test configuration
find_package(GTest QUIET)

if(GTest_FOUND)
    # Test executable
    add_executable(YMulatorSynthAU_Tests
        test_main.cpp
        # Additional test files will be added here
    )
    
    target_link_libraries(YMulatorSynthAU_Tests
        PRIVATE
            gtest
            gtest_main
    )
    
    # Register tests
    include(GoogleTest)
    gtest_discover_tests(YMulatorSynthAU_Tests)
else()
    message(STATUS "Google Test not found. Tests will not be built.")
endif()