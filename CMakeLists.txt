cmake_minimum_required(VERSION 3.30)
project(MANIFOLDEngineTest)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(vendor/MANIFOLDEngine EXCLUDE_FROM_ALL)
add_subdirectory(vendor/tinygltf EXCLUDE_FROM_ALL)

file(GLOB_RECURSE SOURCES src/*.cpp src/*.h)
file(GLOB IMGUI_SOURCES vendor/imgui/*.h, vendor/imgui/*.cpp)
set(IMGUI_BACKENDS vendor/imgui/backends/imgui_impl_sdl3.cpp vendor/imgui/backends/imgui_impl_sdl3.h)

add_executable(Test)
target_sources(Test PRIVATE ${SOURCES} ${IMGUI_SOURCES} ${IMGUI_BACKENDS})
target_include_directories(Test PRIVATE vendor/imgui)
mathfu_configure_flags(Test)

ME_configure(Test)
target_link_libraries(Test PRIVATE SDL3::SDL3 Jolt spdlog::spdlog libhl HLVM tinygltf vfspp)

add_custom_target(assets COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
add_dependencies(Test assets)




