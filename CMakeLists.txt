cmake_minimum_required(VERSION 3.22)
project(progetto)

set(CMAKE_CXX_STANDARD 20)

#set(CMAKE_CXX_FLAGS "-O2 -Ofast -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")

# Snowball stemmer
find_library(STEMMER_LIB stemmer)

# Intel Hyperscan
find_library(PCRE_LIB pcrecpp)

# ICU
find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)

# Lib for utf8 validation
set(SIMDUTF_TESTS OFF)
set(SIMDUTF_BENCHMARKS OFF)
set(SIMDUTF_ICONV OFF)
add_subdirectory(simdutf)

add_subdirectory(tests)

add_executable(prova src/prova_main.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)

target_link_libraries(prova PRIVATE "${STEMMER_LIB}" PRIVATE "${PCRE_LIB}" simdutf)
target_include_directories(prova PUBLIC "simdutf/include")

add_executable(builder src/builder.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)

target_link_libraries(builder PRIVATE "${STEMMER_LIB}" PRIVATE "${PCRE_LIB}" simdutf)
target_include_directories(builder PUBLIC "simdutf/include")


add_executable(engine src/engine.cpp
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
)

target_link_libraries(engine PRIVATE "${STEMMER_LIB}" PRIVATE "${PCRE_LIB}" simdutf)
target_include_directories(engine PUBLIC "simdutf/include")
