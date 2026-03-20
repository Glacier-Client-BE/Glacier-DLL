cmake_minimum_required(VERSION 3.20)
project(GlacierClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Glacier Client requires x64 compilation.")
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Glacier Client is Windows-only.")
endif()

include(FetchContent)

# ── MinHook ────────────────────────────────────────────────────────────────────
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master)
FetchContent_MakeAvailable(minhook)

# ── ImGui ──────────────────────────────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)

# ── IconFontCppHeaders (still used for FA5 glyphs in some overlay text) ────────
FetchContent_Declare(iconfontcppheaders
    GIT_REPOSITORY https://github.com/juliettef/IconFontCppHeaders.git
    GIT_TAG        main)
FetchContent_MakeAvailable(iconfontcppheaders)

# ── FA5 font (for text labels in HUD overlays) ─────────────────────────────────
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/fonts")
set(FA_TTF "${CMAKE_SOURCE_DIR}/fonts/fa-solid-900.ttf")
if(NOT EXISTS "${FA_TTF}")
    file(DOWNLOAD
        "https://github.com/FortAwesome/Font-Awesome/raw/5.x/webfonts/fa-solid-900.ttf"
        "${FA_TTF}" STATUS fa_status SHOW_PROGRESS)
    list(GET fa_status 0 fa_code)
    if(NOT fa_code EQUAL 0)
        message(WARNING "[Glacier] FA font download failed — icons may be missing.")
    endif()
endif()

# ── Collect all sources (GLOB_RECURSE picks up new modules automatically) ──────
file(GLOB_RECURSE CLIENT_SOURCES "src/*.cpp")

add_library(GlacierClient SHARED
    ${CLIENT_SOURCES}
    ${IMGUI_SOURCES})

target_include_directories(GlacierClient PRIVATE
    src/
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${iconfontcppheaders_SOURCE_DIR})

target_link_libraries(GlacierClient PRIVATE
    minhook
    d3d11
    dxgi
    d3dcompiler
    windowscodecs          # WIC — for PNG icon loading
    Psapi                  # Process memory info (SessionInfo)
    Winmm)                 # Multimedia timers

target_compile_definitions(GlacierClient PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
    GLACIER_VERSION="2.0.0")

target_compile_options(GlacierClient PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /MP /permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/wd4100 /wd4189 /wd4244>)

set_target_properties(GlacierClient PROPERTIES
    OUTPUT_NAME "glacier"
    SUFFIX      ".dll")

# ── Post-build: copy font next to DLL ─────────────────────────────────────────
if(EXISTS "${FA_TTF}")
    add_custom_command(TARGET GlacierClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${FA_TTF}" "$<TARGET_FILE_DIR:GlacierClient>/fa-solid-900.ttf"
        COMMENT "[Glacier] Copying FA font...")
endif()

# ── Post-build: create icons/modules/ directory next to DLL ───────────────────
add_custom_command(TARGET GlacierClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:GlacierClient>/icons/modules"
    COMMENT "[Glacier] Creating icons/modules directory...")

message(STATUS "
  ┌──────────────────────────────────────────────┐
  │  Glacier Client v2.0.0                       │
  │                                              │
  │  Build:                                      │
  │    cmake -B build -A x64                     │
  │    cmake --build build --config Release      │
  │                                              │
  │  Output: build/Release/glacier.dll           │
  │                                              │
  │  Icons: place <name>.png files in            │
  │    <dll_dir>/icons/modules/                  │
  │  e.g. armorhud.png, fpscounter.png, ...      │
  └──────────────────────────────────────────────┘
")
