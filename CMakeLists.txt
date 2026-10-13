# Splinter Cell Double Agent (original Xbox), statically recompiled

cmake_minimum_required(VERSION 3.25)
project(scda LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zydis GIT_REPOSITORY https://github.com/zyantific/zydis.git GIT_TAG v4.1.1 GIT_SHALLOW TRUE)
FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.0 GIT_SHALLOW TRUE)
# SDL3 for controllers and the headset's audio, static so it links into the exe
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.16 GIT_SHALLOW TRUE)
# Opus for the wideband voice link, static, without the stack protector and fortify flags that need GCC's libssp
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_STACK_PROTECTOR OFF CACHE BOOL "" FORCE)
set(OPUS_FORTIFY_SOURCE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(opus GIT_REPOSITORY https://github.com/xiph/opus.git GIT_TAG v1.5.2 GIT_SHALLOW TRUE)
# dxc for the ray traced ambient occlusion shaders: shader model 6.5, past what the runtime's D3DCompile builds
FetchContent_Declare(dxc
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip
    URL_HASH SHA256=9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(zydis tomlplusplus imgui SDL3 opus dxc)

# x86 to C++ recompiler
file(GLOB RECOMP_SOURCES CONFIGURE_DEPENDS tools/recomp/*.cpp)
add_executable(xbe_recomp ${RECOMP_SOURCES})
target_include_directories(xbe_recomp PRIVATE src tools/recomp)
target_compile_definitions(xbe_recomp PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(xbe_recomp PRIVATE Zydis tomlplusplus::tomlplusplus)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)

file(GLOB HOST_SOURCES CONFIGURE_DEPENDS src/*/*.cpp)
list(FILTER HOST_SOURCES EXCLUDE REGEX "/src/(game|versus)/")
# SDL's and Opus's headers go to the input and voice code alone (both are linked LINK_ONLY), so the generated sources'
# compile line stays the same
set_source_files_properties(src/core/input.cpp src/audio/voice.cpp src/audio/voice_link.cpp PROPERTIES
    INCLUDE_DIRECTORIES "${sdl3_SOURCE_DIR}/include;${opus_SOURCE_DIR}/include")
file(GLOB RECOMP_CONFIGS CONFIGURE_DEPENDS config/*.toml)

# Generate build_info.h and track it as a dependency of version-related
find_package(Git QUIET)
set(BUILD_INFO ${CMAKE_CURRENT_BINARY_DIR}/build_info.h)
add_custom_target(build_info
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${BUILD_INFO} -DGIT=${GIT_EXECUTABLE}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_info.cmake
    BYPRODUCTS ${BUILD_INFO}
    VERBATIM)
set_source_files_properties(src/app/system_info.cpp src/app/version.rc PROPERTIES
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR} OBJECT_DEPENDS ${BUILD_INFO})

# ray traced ambient occlusion's shaders as bytecode headers for d3d12.cpp
set(DXC ${dxc_SOURCE_DIR}/bin/x64/dxc.exe)
set(RTAO_HLSL ${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/occlusion_rt.hlsl)
set(RTAO_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(RTAO_HEADERS ${RTAO_DIR}/occlusion_rt_vs.h ${RTAO_DIR}/occlusion_rt_ps.h ${RTAO_DIR}/occlusion_rt_ps_ms.h)
add_custom_command(
    OUTPUT ${RTAO_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RTAO_DIR}
    COMMAND ${DXC} -nologo -T vs_6_5 -E vs -Vn kRtaoVs -Fh ${RTAO_DIR}/occlusion_rt_vs.h ${RTAO_HLSL}
    COMMAND ${DXC} -nologo -T ps_6_5 -E rtao -Vn kRtaoPs -Fh ${RTAO_DIR}/occlusion_rt_ps.h ${RTAO_HLSL}
    COMMAND ${DXC} -nologo -T ps_6_5 -E rtao -D MS=1 -Vn kRtaoPsMs -Fh ${RTAO_DIR}/occlusion_rt_ps_ms.h ${RTAO_HLSL}
    DEPENDS ${RTAO_HLSL} ${DXC}
    COMMENT "Compiling the ray traced ambient occlusion shaders"
    VERBATIM)
add_custom_target(rtao_shaders DEPENDS ${RTAO_HEADERS})
set_source_files_properties(src/gpu/d3d12.cpp PROPERTIES
    INCLUDE_DIRECTORIES ${RTAO_DIR} OBJECT_DEPENDS "${RTAO_HEADERS}")

# One executable per recompiled image: the image's code from its config (partitions as that config sets them),
# the shared host, and the game hooks only that image has.
function(scda_image target xbe config partitions game_dir)
    set(GEN ${CMAKE_CURRENT_SOURCE_DIR}/generated/${game_dir})
    set(GEN_SOURCES ${GEN}/x86_func_mapping.cpp)
    math(EXPR last "${partitions} - 1")
    foreach(i RANGE ${last})
        list(APPEND GEN_SOURCES ${GEN}/x86_recomp.${i}.cpp)
    endforeach()
    add_custom_command(
        OUTPUT ${GEN}/codegen.stamp
        BYPRODUCTS ${GEN_SOURCES} ${GEN}/x86_recomp_shared.h ${GEN}/image_symbols.h
        COMMAND xbe_recomp config/${config}
        COMMAND ${CMAKE_COMMAND} -E touch ${GEN}/codegen.stamp
        DEPENDS xbe_recomp ${RECOMP_CONFIGS} ${CMAKE_CURRENT_SOURCE_DIR}/assets/${xbe}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Recompiling ${xbe}"
        VERBATIM)
    add_custom_target(recompile_${target} DEPENDS ${GEN}/codegen.stamp)
    set_source_files_properties(${GEN_SOURCES} PROPERTIES GENERATED TRUE
        COMPILE_OPTIONS "-O2;-fno-strict-aliasing;-Wno-unused-label;-Wno-parentheses-equality;-Wno-unused-variable")
    file(GLOB GAME_SOURCES CONFIGURE_DEPENDS src/${game_dir}/*.cpp)
    # the xbe's SHA-1: the executable checks the game it finds is the release it was made from
    set(xbe_file ${CMAKE_CURRENT_SOURCE_DIR}/assets/${xbe})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${xbe_file})
    file(SHA1 ${xbe_file} xbe_sha1)
    set(XBE_HASH ${CMAKE_CURRENT_BINARY_DIR}/${target}_xbe.cpp)
    file(CONFIGURE OUTPUT ${XBE_HASH} CONTENT "extern const char kImageSha1[] = \"${xbe_sha1}\";\n")
    add_executable(${target} WIN32 ${HOST_SOURCES} ${GAME_SOURCES} ${GEN_SOURCES} ${IMGUI_SOURCES} ${XBE_HASH}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app/version.rc)
    add_dependencies(${target} recompile_${target} build_info rtao_shaders)
    target_include_directories(${target} PRIVATE src ${GEN})
    target_include_directories(${target} SYSTEM PRIVATE ${imgui_SOURCE_DIR})
    target_compile_options(${target} PRIVATE -fno-strict-aliasing)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(${target} PRIVATE tomlplusplus::tomlplusplus $<LINK_ONLY:SDL3::SDL3-static> $<LINK_ONLY:opus> d3d12 dxgi dxguid d3dcompiler xaudio2 ole32 windowscodecs dbghelp winmm dwmapi ws2_32)
    # lld keeps the recompiled weak functions overridable by the host ones
    target_link_options(${target} PRIVATE -fuse-ld=lld -g)
endfunction()

scda_image(SCDA default.xbe scda_v2.toml 128 game)        # story and co-op
scda_image(SCDA_Versus sc4_adv.xbe sc4_adv.toml 64 versus)  # versus
