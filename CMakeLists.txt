cmake_minimum_required(VERSION 3.20)
project(ttf2ugui C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ================================================================
# Extract FreeType source into build directory
# ================================================================
set(FREETYPE_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/deps/freetype-2.14.3.tar.gz")
set(FREETYPE_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/freetype-src")

if(NOT EXISTS "${FREETYPE_SRC_DIR}/CMakeLists.txt")
    message(STATUS "Extracting FreeType source...")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/temp-extract")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${FREETYPE_ARCHIVE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/temp-extract"
        RESULT_VARIABLE extract_result
        OUTPUT_QUIET
        ERROR_VARIABLE extract_error
    )
    if(NOT extract_result EQUAL 0)
        message(FATAL_ERROR "Failed to extract FreeType: ${extract_error}")
    endif()
    file(GLOB extracted_dirs "${CMAKE_CURRENT_BINARY_DIR}/temp-extract/freetype-*")
    list(GET extracted_dirs 0 extracted_dir)
    if(NOT extracted_dir)
        message(FATAL_ERROR "FreeType directory not found")
    endif()
    file(RENAME "${extracted_dir}" "${FREETYPE_SRC_DIR}")
    file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/temp-extract")
endif()

# Copy custom config headers
set(FREETYPE_CONFIG_TARGET "${FREETYPE_SRC_DIR}/include/freetype/config")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/deps/ftoption.h"
     DESTINATION "${FREETYPE_CONFIG_TARGET}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/deps/ftmodule.h"
     DESTINATION "${FREETYPE_CONFIG_TARGET}")

# FreeType build options
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(${FREETYPE_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/freetype-build EXCLUDE_FROM_ALL)

if(MINGW)
    target_compile_options(freetype PRIVATE -Os -fno-lto)
endif()

# ================================================================
# Target
# ================================================================
add_executable(ttf2ugui ttf2ugui.c ugui.c)

target_include_directories(ttf2ugui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FREETYPE_SRC_DIR}/include
)

target_link_libraries(ttf2ugui PRIVATE freetype)

if(UNIX AND NOT WIN32)
    target_link_libraries(ttf2ugui PRIVATE m)
endif()

if(WIN32 AND MINGW)
    target_compile_options(ttf2ugui PRIVATE
        -Os -fno-lto -fdata-sections -ffunction-sections
    )
    target_link_options(ttf2ugui PRIVATE
        -static -Wl,--gc-sections -Wl,-s
    )
elseif(UNIX AND NOT WIN32)
    target_compile_options(ttf2ugui PRIVATE
        -Os -fdata-sections -ffunction-sections
    )
    target_link_options(ttf2ugui PRIVATE
        -Wl,--gc-sections -Wl,-s
    )
endif()

# ================================================================
# Custom targets
# ================================================================
add_custom_target(rebuild
    COMMAND ${CMAKE_COMMAND} --build . --target clean
    COMMAND ${CMAKE_COMMAND} --build . --target ttf2ugui
    COMMENT "Rebuilding ttf2ugui"
)

add_custom_target(stripall
    COMMAND ${CMAKE_STRIP} $<TARGET_FILE:ttf2ugui>
    COMMENT "Stripping ttf2ugui"
)