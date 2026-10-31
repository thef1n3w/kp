cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()
project(antivirus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build with MSYS2 UCRT64 on D: when available:
# D:\msys64\ucrt64\bin\cmake.exe -G Ninja -B build -S . -DCMAKE_C_COMPILER=D:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=D:/msys64/ucrt64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=D:/msys64/ucrt64/bin/ninja.exe

set(RAYLIB_ROOT "" CACHE PATH "Directory containing raylib include/ and lib/")
set(FFMPEG_ROOT "" CACHE PATH "Directory containing FFmpeg include/, lib/ and bin/")

find_package(raylib CONFIG QUIET HINTS "${RAYLIB_ROOT}")

if(NOT raylib_FOUND)
    find_library(RAYLIB_LIB raylib HINTS "${RAYLIB_ROOT}/lib" PATHS "D:/msys64/ucrt64/lib" "C:/msys64/ucrt64/lib" "C:/msys64/mingw64/lib")
    find_path(RAYLIB_INC raylib.h HINTS "${RAYLIB_ROOT}/include" PATHS "D:/msys64/ucrt64/include" "C:/msys64/ucrt64/include" "C:/msys64/mingw64/include")
    if(RAYLIB_LIB AND RAYLIB_INC)
        add_library(raylib STATIC IMPORTED)
        set_target_properties(raylib PROPERTIES
            IMPORTED_LOCATION "${RAYLIB_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INC}"
        )
    else()
        message(FATAL_ERROR "raylib not found. Set RAYLIB_ROOT or install mingw-w64-ucrt-x86_64-raylib.")
    endif()
endif()

if(FFMPEG_ROOT)
    find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h HINTS "${FFMPEG_ROOT}/include")
    if(NOT FFMPEG_INCLUDE_DIR)
        message(FATAL_ERROR "FFmpeg headers not found under FFMPEG_ROOT/include.")
    endif()
    add_library(ffmpeg INTERFACE)
    target_include_directories(ffmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
    foreach(component avformat avcodec avutil swscale)
        find_library(FFMPEG_${component}_LIBRARY NAMES ${component} HINTS "${FFMPEG_ROOT}/lib")
        if(NOT FFMPEG_${component}_LIBRARY)
            message(FATAL_ERROR "FFmpeg ${component} library not found under FFMPEG_ROOT/lib.")
        endif()
        target_link_libraries(ffmpeg INTERFACE "${FFMPEG_${component}_LIBRARY}")
    endforeach()
    set(FFMPEG_TARGET ffmpeg)
else()
    set(ENV{PKG_CONFIG_PATH} "D:/msys64/ucrt64/lib/pkgconfig;$ENV{PKG_CONFIG_PATH}")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    set(FFMPEG_TARGET PkgConfig::FFMPEG)
endif()

add_executable(${PROJECT_NAME}
    game/main.cpp
    game/Menu.cpp
    game/Gameplay.cpp
    game/Journal.cpp
    game/Game1.cpp
    game/Game2.cpp
    game/Game3.cpp
    game/Game4.cpp
    game/Game5.cpp
    game/GameState.h
)

target_include_directories(${PROJECT_NAME} PRIVATE game)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib ${FFMPEG_TARGET})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 gdi32 winmm)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/game/res" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
)
