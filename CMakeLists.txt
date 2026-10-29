cmake_minimum_required(VERSION 3.24)
project(HillBlack LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Use MSVC para Windows x64.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/archive/65ee68451d8eb2b5f3a30b410476ab83deb3289b.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui json)
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_lib PUBLIC NOMINMAX)
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS cs2_external/src/*.cpp)
add_executable(HillBlack WIN32 ${CLIENT_SOURCES} cs2_external/src/login.rc)
target_include_directories(HillBlack PRIVATE ${json_SOURCE_DIR}/single_include/nlohmann)
target_compile_options(HillBlack PRIVATE /utf-8 /W3)
target_link_libraries(HillBlack PRIVATE imgui_lib d3d11 dxgi d3dcompiler dwmapi winhttp iphlpapi winsqlite3)
set_target_properties(HillBlack PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist")
option(HILLBLACK_TESTS "Build local authentication tests" OFF)
if(HILLBLACK_TESTS)
  add_executable(auth_local_test tests/auth_local_test.cpp cs2_external/src/auth_local.cpp)
  target_include_directories(auth_local_test PRIVATE cs2_external/src)
  target_compile_definitions(auth_local_test PRIVATE NOMINMAX)
  target_link_libraries(auth_local_test PRIVATE winsqlite3)
endif()
