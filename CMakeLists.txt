# Splinter Cell Double Agent (original Xbox), statically recompiled

cmake_minimum_required(VERSION 3.25)
project(scda LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zydis GIT_REPOSITORY https://github.com/zyantific/zydis.git GIT_TAG v4.1.1 GIT_SHALLOW TRUE)
FetchContent_Declare(tomlplusplus GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git GIT_TAG v3.4.0 GIT_SHALLOW TRUE)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(zydis tomlplusplus imgui)

# x86 to C++ recompiler
file(GLOB RECOMP_SOURCES CONFIGURE_DEPENDS tools/recomp/*.cpp)
add_executable(xbe_recomp ${RECOMP_SOURCES})
target_include_directories(xbe_recomp PRIVATE src tools/recomp)
target_link_libraries(xbe_recomp PRIVATE Zydis tomlplusplus::tomlplusplus)

# default.xbe to generated/, the partition count is set in config/scda_v2.toml
set(GEN ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set(GEN_SOURCES ${GEN}/x86_func_mapping.cpp)
foreach(i RANGE 127)
    list(APPEND GEN_SOURCES ${GEN}/x86_recomp.${i}.cpp)
endforeach()
add_custom_command(
    OUTPUT ${GEN}/codegen.stamp
    BYPRODUCTS ${GEN_SOURCES} ${GEN}/x86_recomp_shared.h
    COMMAND xbe_recomp config/scda_v2.toml
    COMMAND ${CMAKE_COMMAND} -E touch ${GEN}/codegen.stamp
    DEPENDS xbe_recomp ${CMAKE_CURRENT_SOURCE_DIR}/config/scda_v2.toml ${CMAKE_CURRENT_SOURCE_DIR}/config/xdk_symbols.toml
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xbe
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Recompiling default.xbe"
    VERBATIM)
add_custom_target(codegen DEPENDS ${GEN}/codegen.stamp)
set_source_files_properties(${GEN_SOURCES} PROPERTIES GENERATED TRUE
    COMPILE_OPTIONS "-O2;-fno-strict-aliasing;-Wno-unused-label;-Wno-parentheses-equality;-Wno-unused-variable")

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)

file(GLOB HOST_SOURCES CONFIGURE_DEPENDS src/*/*.cpp)
add_executable(scda WIN32 ${HOST_SOURCES} ${GEN_SOURCES} ${IMGUI_SOURCES})
add_dependencies(scda codegen)
set_target_properties(scda PROPERTIES OUTPUT_NAME SCDA)
target_include_directories(scda PRIVATE src ${GEN} ${imgui_SOURCE_DIR})
target_compile_options(scda PRIVATE -fno-strict-aliasing)
target_compile_definitions(scda PRIVATE UNICODE _UNICODE)
target_link_libraries(scda PRIVATE tomlplusplus::tomlplusplus d3d12 dxgi dxguid d3dcompiler xaudio2 xinput ole32 dbghelp winmm dwmapi)
# lld keeps the recompiled weak functions overridable by the host ones
target_link_options(scda PRIVATE -fuse-ld=lld -g)
