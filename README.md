## About
Rendering engine prototype made with `Vulkan 1.3`. Code is written using a [simpler C++ coding style](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b). Project is written for the `C++11` standard and `x64` system. Currently the code is tested only on `Windows`, using `MSVC` (Visual Studio) and `MINGW` (Visual Studio Code) compilers. `Linux` is not completely supported at the moment, but it should be easy to port, since all third party libraries are cross platform.

![Demo](https://github.com/milkru/data_resources/blob/main/vulkanizer/mesh_bunny.PNG)

## Features
* Vulkan meta loading using [volk](https://github.com/zeux/volk)
* Window handling using [glfw](https://github.com/glfw/glfw)
* Mesh loading using [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
* Mesh optimizations using [meshoptimizer](https://github.com/zeux/meshoptimizer)
* Single mesh rendering
* NVidia [Mesh Shading Pipeline](https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/) support, with traditional pipeline still supported
* Programmable vertex fetching with 12 byte vertices
* Depth buffering with [reversed-Z](https://developer.nvidia.com/content/depth-precision-visualized)
* Automatic descriptor set layout creation with [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)
* Vulkan's dynamic rendering, push descriptors and descriptor update templates support
* CPU profiling using [easy_profiler](https://github.com/yse/easy_profiler)
* GPU profiling using query timestamps and pipeline statistics
* Custom [Dear ImGui](https://github.com/ocornut/imgui) Vulkan backend with *Performance* and *Settings* windows
* In-flight frames

## Installation
This project uses [CMake](https://cmake.org/download/) as a build tool. Since the project is built using `Vulkan`, the latest [Vulkan SDK](https://vulkan.lunarg.com) is required.

## License
Distributed under the MIT License. See `LICENSE` for more information.
