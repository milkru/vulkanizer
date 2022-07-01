cmake_minimum_required(VERSION 3.16)

# Project
project(vulkanizer)

message("Building Project:")

file(GLOB_RECURSE SRC_FILES "src/*.h" "src/*.cpp")

include_directories(src)
add_executable(${PROJECT_NAME} ${SRC_FILES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG_>)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:RELEASE_>)

if (MSVC)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

if (MINGW)
	set(CMAKE_EXE_LINKER_FLAGS " -static")
	target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

# Shaders
message("Compiling Shaders:")

set(GLSL_COMPILER "$ENV{VULKAN_SDK}/Bin/glslc.exe")

file(GLOB_RECURSE GLSL_SRC_FILES "src/shaders/*.vert" "src/shaders/*.frag")
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders/")

foreach(GLSL_FILE ${GLSL_SRC_FILES})
	get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
	set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${GLSL_FILE_NAME}.spv")
	execute_process(COMMAND ${GLSL_COMPILER} ${GLSL_FILE} -o ${SPIRV_FILE})
endforeach(GLSL_FILE)

if (MSVC)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")
endif()

# glfw
message("Adding glfw:")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")
set(GLFW_DIR 3rdparty/glfw)

add_subdirectory(${GLFW_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

set_property(TARGET glfw PROPERTY FOLDER "3rdparty/glfw")
set_property(TARGET update_mappings PROPERTY FOLDER "3rdparty/glfw")

# glm
message("Adding glm:")

set(GLM_DIR 3rdparty/glm)
target_include_directories(${PROJECT_NAME} PRIVATE ${GLM_DIR})

# volk
message("Adding volk:")

set(VOLK_DIR 3rdparty/volk)
add_subdirectory(${VOLK_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${VOLK_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE volk)

set_property(TARGET volk PROPERTY FOLDER "3rdparty")

# tinyobjloader
message("Adding tinyobjloader:")

set(TINYOBJLOADER_DIR 3rdparty/tinyobjloader)
add_subdirectory(${TINYOBJLOADER_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYOBJLOADER_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE tinyobjloader)

set_property(TARGET tinyobjloader PROPERTY FOLDER "3rdparty/tinyobjloader")
set_property(TARGET uninstall PROPERTY FOLDER "3rdparty/tinyobjloader")

# meshoptimizer
message("Adding meshoptimizer:")

set(MESHOPTIMIZER_DIR 3rdparty/meshoptimizer)
add_subdirectory(${MESHOPTIMIZER_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${MESHOPTIMIZER_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)

set_property(TARGET meshoptimizer PROPERTY FOLDER "3rdparty")

# easy_profiler
message("Adding easy_profiler:")

set(EASY_PROFILER_NO_GUI ON CACHE BOOL "")
set(EASY_PROFILER_NO_SAMPLES ON CACHE BOOL "")
set(EASY_PROFILER_DIR 3rdparty/easy_profiler)

add_subdirectory(${EASY_PROFILER_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${EASY_PROFILER_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE easy_profiler)

set_property(TARGET easy_profiler PROPERTY FOLDER "3rdparty/easy_profiler")
set_property(TARGET profiler_converter PROPERTY FOLDER "3rdparty/easy_profiler")

# imgui
message("Adding imgui:")

set(IMGUI_DIR 3rdparty/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})
target_include_directories(imgui PRIVATE ${IMGUI_DIR})
target_include_directories(imgui PRIVATE ${GLFW_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)

set_property(TARGET imgui PROPERTY FOLDER "3rdparty")
