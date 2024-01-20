cmake_minimum_required(VERSION 3.16)

project(vulkanizer)

message("Compiling Shaders:")

set(GLSL_COMPILER "$ENV{VULKAN_SDK}/Bin/glslc.exe")

file(GLOB_RECURSE GLSL_SRC_FILES "src/shaders/*.comp" "src/shaders/*.task" "src/shaders/*.mesh" "src/shaders/*.vert" "src/shaders/*.frag")
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/shaders/")

source_group("Shader Files" FILES ${GLSL_SRC_FILES})

foreach(GLSL_FILE ${GLSL_SRC_FILES})
	get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
	set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${GLSL_FILE_NAME}.spv")
	execute_process(COMMAND ${GLSL_COMPILER} "-g" ${GLSL_FILE} -o ${SPIRV_FILE} "--target-env=vulkan1.1")
endforeach(GLSL_FILE)

message("Building Project:")

file(GLOB_RECURSE SRC_FILES "src/*.h" "src/*.cpp")
file(GLOB_RECURSE GLSL_SRC_FILES "src/shaders/*.comp" "src/shaders/*.task" "src/shaders/*.mesh" "src/shaders/*.vert" "src/shaders/*.frag")
source_group("Shader Files" FILES ${GLSL_SRC_FILES})

add_definitions(-DIDXTYPEWIDTH=32 -DREALTYPEWIDTH=32)

include_directories(src)
add_executable(${PROJECT_NAME} ${SRC_FILES} ${GLSL_SRC_FILES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG_>)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:RELEASE_>)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /wd26812)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
		COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders")
endif()

if (MINGW)
	set(CMAKE_EXE_LINKER_FLAGS " -static")
	target_link_libraries(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
endif()

message("Adding glfw:")

set(GLFW_DIR 3rdparty/glfw)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_INSTALL OFF CACHE BOOL "")

add_subdirectory(${GLFW_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

set_property(TARGET glfw PROPERTY FOLDER "3rdparty/glfw")
set_property(TARGET update_mappings PROPERTY FOLDER "3rdparty/glfw")

message("Adding glm:")

set(GLM_DIR 3rdparty/glm)

target_include_directories(${PROJECT_NAME} PRIVATE ${GLM_DIR})

message("Adding volk:")

set(VOLK_DIR 3rdparty/volk)

add_subdirectory(${VOLK_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${VOLK_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE volk)

set_property(TARGET volk PROPERTY FOLDER "3rdparty")

# Temporarily using this repo until vertex locking feature comes to the original repo
message("Adding meshoptimizer-cpp:")

set(MESHOPTIMIZER_DIR 3rdparty/meshoptimizer-cpp)

add_subdirectory(${MESHOPTIMIZER_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${MESHOPTIMIZER_DIR}/src)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)

set_property(TARGET meshoptimizer PROPERTY FOLDER "3rdparty")

message("Adding easy_profiler:")

set(EASY_PROFILER_DIR 3rdparty/easy_profiler)

set(EASY_PROFILER_NO_GUI ON CACHE BOOL "")
set(EASY_PROFILER_NO_SAMPLES ON CACHE BOOL "")

add_subdirectory(${EASY_PROFILER_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${EASY_PROFILER_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE easy_profiler)

set_property(TARGET easy_profiler PROPERTY FOLDER "3rdparty/easy_profiler")
set_property(TARGET profiler_converter PROPERTY FOLDER "3rdparty/easy_profiler")

message("Adding imgui:")

set(IMGUI_DIR 3rdparty/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})
target_include_directories(imgui PRIVATE ${IMGUI_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)

set_property(TARGET imgui PROPERTY FOLDER "3rdparty")

message("Adding SPIRV-Reflect:")

set(SPIRV_REFLECT_DIR 3rdparty/SPIRV-Reflect)

set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "")
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "")
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "")

add_subdirectory(${SPIRV_REFLECT_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${SPIRV_REFLECT_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)

set_property(TARGET spirv-reflect-static PROPERTY FOLDER "3rdparty")

message("Adding fast_obj:")

set(FAST_OBJ_DIR 3rdparty/fast_obj)

add_subdirectory(${FAST_OBJ_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${FAST_OBJ_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE fast_obj_lib)

set_property(TARGET fast_obj_lib PROPERTY FOLDER "3rdparty")

message("Adding VulkanMemoryAllocator:")

set(VMA_DIR 3rdparty/VulkanMemoryAllocator)

set(VMA_STATIC_VULKAN_FUNCTIONS OFF CACHE BOOL "")
set(VMA_DYNAMIC_VULKAN_FUNCTIONS OFF CACHE BOOL "")

add_subdirectory(${VMA_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${VMA_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE VulkanMemoryAllocator)

set_property(TARGET VulkanMemoryAllocator PROPERTY FOLDER "3rdparty")

message("Adding CRCpp:")

set(CRC_DIR 3rdparty/CRCpp)

set(BUILD_DOC OFF CACHE BOOL "")
set(BUILD_TEST OFF CACHE BOOL "")

add_subdirectory(${CRC_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CRC_DIR}/inc)
target_link_libraries(${PROJECT_NAME} PRIVATE CRCpp)

message("Adding METIS:")

set(METIS_DIR 3rdparty/METIS)
set(GKLIB_DIR 3rdparty/GKlib)

# Avoids editing 3rdparty projects
file(WRITE ${METIS_DIR}/build/xinclude/CMakeLists.txt "")
file(WRITE ${METIS_DIR}/programs/CMakeLists.txt "")
file(WRITE ${GKLIB_DIR}/test/CMakeLists.txt "")
file(WRITE ${CMAKE_SOURCE_DIR}/conf/check_thread_storage.c "")

add_subdirectory(${METIS_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${METIS_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE metis)

add_subdirectory(${GKLIB_DIR})
target_include_directories(metis PRIVATE ${GKLIB_DIR})
target_include_directories(metis PRIVATE ${METIS_DIR}/include)
target_link_libraries(metis PRIVATE GKlib)

set_property(TARGET metis PROPERTY FOLDER "3rdparty")
set_property(TARGET GKlib PROPERTY FOLDER "3rdparty")
