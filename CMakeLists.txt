cmake_minimum_required(VERSION 3.22)
project(progetto)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Ofast ")

option(USE_STEMMER "Enable or disable Snowball's stemmer and stopword removal" ON)

if(USE_STEMMER)
    add_compile_definitions(SEARCHENGINECPP_STEMMER_ENABLE)
endif()

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

add_library(libprogetto
        src/codes/codes.hpp
        src/codes/variable_blocks.hpp
        src/normalizer/WordNormalizer.cpp
        src/normalizer/WordNormalizer.hpp
        src/normalizer/utf8_utils.cpp
        src/normalizer/utf8_utils.hpp
        src/index/types.hpp
        src/index/Index.cpp
        src/index/Index.hpp
        src/indexBuilder/IndexBuilder.hpp
        src/indexBuilder/IndexBuilder.cpp
)
target_link_libraries(libprogetto PUBLIC "${STEMMER_LIB}" "${PCRE_LIB}" simdutf)

add_subdirectory(tests)

add_executable(prova src/prova_main.cpp)
target_link_libraries(prova PRIVATE libprogetto)
target_include_directories(prova PUBLIC "simdutf/include")

add_executable(builder src/builder.cpp)
target_link_libraries(builder PRIVATE libprogetto)
target_include_directories(builder PUBLIC "simdutf/include")

add_executable(engine src/engine.cpp)
target_link_libraries(engine PRIVATE libprogetto)
target_include_directories(engine PUBLIC "simdutf/include")
