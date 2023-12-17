cmake_minimum_required(VERSION 3.22)
project(progetto)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-g -Wall -Wextra -fno-exceptions")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Ofast -march=native -DNDEBUG")

if(APPLE)
    execute_process(
            COMMAND brew --prefix
            RESULT_VARIABLE BREW_EX
            OUTPUT_VARIABLE BREW_EX_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (BREW_EX EQUAL 0 AND EXISTS "${BREW_EX_PREFIX}")
        message(STATUS "Found the damned brew at: ${BREW_EX_PREFIX}")
    else()
        message(WARNING "Brew not found I MAY NOT BE ABLE TO COMPILE WITH SUCH CONFIGURATION.")
        message(WARNING "Assuming brew is at /usr/local")
        set(BREW_EX_PREFIX "/usr/local")
    endif()

    list(APPEND CMAKE_PREFIX_PATH ${BREW_EX_PREFIX})
    include_directories(${BREW_EX_PREFIX}/include)
endif()

option(USE_STEMMER "Enable or disable Snowball's stemmer and stopword removal" ON)
option(FIX_MSMARCO_LATIN1 "Enable or disable heuristic and encoding fix for certain wronly encoded docs in MSMARCO" OFF)
option(TEXT_FULL_LATIN1_CASE "Enable or disable lower case `function str_to_lwr_uft8_latin1`. If it's disable we'll use std::tolower(c);" OFF)
option(USE_FAST_LOG "Enable or disable integer implementation of log2 instead of cmath's float impl." OFF)

if(USE_STEMMER)
    add_compile_definitions(SEARCHENGINECPP_STEMMER_ENABLE)
endif()

if(FIX_MSMARCO_LATIN1)
    add_compile_definitions(FIX_MSMARCO_LATIN1)
endif()

if(TEXT_FULL_LATIN1_CASE)
    add_compile_definitions(TEXT_FULL_LATIN1_CASE)
endif()

if(USE_FAST_LOG)
    add_compile_definitions(USE_FAST_LOG)
endif()

# Snowball stemmer
find_library(STEMMER_LIB stemmer REQUIRED)

# ICU
#find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)

find_library(HYPERSCAN_LIB hs REQUIRED)

add_library(libprogetto
        src/codes/variable_blocks.hpp
        src/normalizer/WordNormalizer.cpp
        src/normalizer/WordNormalizer.hpp
        src/normalizer/utf8_utils.cpp
        src/normalizer/utf8_utils.hpp
        src/index/types.hpp
        src/index/Index.hpp
        src/index/Index.template.hpp
        src/index/Index.cpp
        src/indexBuilder/IndexBuilder.hpp
        src/indexBuilder/IndexBuilder.cpp
        src/codes/unary.hpp
        src/normalizer/PunctuationRemover.cpp
        src/normalizer/PunctuationRemover.hpp
        src/normalizer/stop_words.cpp
        src/normalizer/stop_words.hpp
        src/meta.hpp
        src/util/thread_pool.cpp
        src/util/thread_pool.hpp
        src/codes/diskmap/diskmap.hpp
        src/codes/diskmap/reader.hpp
        src/codes/diskmap/builder.hpp
        src/util/memory.cpp
        src/util/memory.hpp
        src/index/query_scorer.cpp
        src/index/query_scorer.hpp
        src/util/engine_options.cpp
        src/util/engine_options.hpp
)

target_link_libraries(libprogetto PUBLIC "${STEMMER_LIB}" "${HYPERSCAN_LIB}")

add_subdirectory(tests)

add_executable(prova src/prova_main.cpp)
target_link_libraries(prova PRIVATE libprogetto)

add_executable(builder src/builder.cpp)
target_link_libraries(builder PRIVATE libprogetto)

add_executable(engine src/engine.cpp)
target_link_libraries(engine PRIVATE libprogetto)
