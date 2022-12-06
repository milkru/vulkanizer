# About
Rendering engine prototype made with `Vulkan 1.3`. Code is written using a [simpler C++ coding style](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b). Project is written for the `C++20` standard and `x64` system. Currently the code is tested only on `Windows`, using `MSVC` (Visual Studio) and `MINGW` (Visual Studio Code) compilers. `Linux` is not completely supported at the moment, but it should be easy to port, since all third party libraries are cross platform.

## Screenshots
![Demo](https://github.com/milkru/data_resources/blob/main/vulkanizer/occ_mesh.png)

![Demo](https://github.com/milkru/data_resources/blob/main/vulkanizer/occ_trad.png)

![Demo](https://github.com/milkru/data_resources/blob/main/vulkanizer/occ_frez.png)

## Features
* Vulkan meta loading with [volk](https://github.com/zeux/volk)
* Window handling with [glfw](https://github.com/glfw/glfw)
* Mesh loading with [fast_obj](https://github.com/thisistherk/fast_obj)
* Mesh optimizations with [meshoptimizer](https://github.com/zeux/meshoptimizer)
* GPU memory allocator with [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* CPU profiling with [easy_profiler](https://github.com/yse/easy_profiler)
* GPU profiling with query timestamps and pipeline statistics
* Custom [Dear ImGui](https://github.com/ocornut/imgui) Vulkan backend with *Performance* and *Settings* windows
* Multiple mesh rendering
* Programmable vertex fetching with 14 byte vertices
* Sampler caching
* Mesh LOD system
* Mesh GPU frustum and two-pass occlusion culling with draw call compaction
* NVidia [Mesh Shading Pipeline](https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/) support, with traditional pipeline still supported
* Meshlet cone and frustum culling
* Depth buffering with [reversed-Z](https://developer.nvidia.com/content/depth-precision-visualized)
* Automatic descriptor set layout creation with [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)
* Vulkan's dynamic rendering, indirect draw count, push descriptors, descriptor update templates and debug marker support
* In-flight frames

## Installation
This project uses [CMake](https://cmake.org/download/) as a build tool. Since the project is built using `Vulkan`, the latest [Vulkan SDK](https://vulkan.lunarg.com) is required.

## Requirements
Make sure that your graphics card can support listed Vulkan features and make sure to have updated driver.

## License
Distributed under the MIT License. See `LICENSE` for more information.
