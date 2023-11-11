# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

# 'Google_Tests_run' is the target name
# 'test1.cpp test2.cpp' are source files with tests
add_executable(Google_Tests_run
        test_codes_variable_blocks.cpp
        test_normalizer.cpp
        test_codes_unary.cpp
        test_index_builder.cpp
)
target_link_libraries(Google_Tests_run PRIVATE gtest_main libprogetto)
target_include_directories(Google_Tests_run PUBLIC "../src")

