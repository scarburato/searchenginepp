cmake_minimum_required(VERSION 3.25)
project(progetto)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(tests)

add_executable(prova src/prova_main.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)

add_executable(builder src/builder.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)

add_executable(engine src/engine.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)
