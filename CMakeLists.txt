cmake_minimum_required(VERSION 3.22)
project(progetto)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -fno-exceptions")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Ofast ")

option(USE_STEMMER "Enable or disable Snowball's stemmer and stopword removal" ON)
option(FIX_MSMARCO_LATIN1 "Enable or disable heuristic and encoding fix for certain wronly encoded docs in MSMARCO" OFF)

if(USE_STEMMER)
    add_compile_definitions(SEARCHENGINECPP_STEMMER_ENABLE)
endif()

if(FIX_MSMARCO_LATIN1)
    add_compile_definitions(FIX_MSMARCO_LATIN1)
endif()

# Snowball stemmer
find_library(STEMMER_LIB stemmer)

# PCRE regexe
find_library(PCRE_LIB pcrecpp)

# Intel Hyperscan regex
find_library(HYPERSCAN_LIB hs)

# ICU
find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)

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
        src/codes/unary.hpp
        src/normalizer/PunctuationRemover.cpp
        src/normalizer/PunctuationRemover.hpp
        src/normalizer/stop_words.cpp
        src/normalizer/stop_words.hpp
)

target_link_libraries(libprogetto PUBLIC "${STEMMER_LIB}" "${HYPERSCAN_LIB}" "${PCRE_LIB}")

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
