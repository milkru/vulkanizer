## About
Rendering engine prototype made with `Vulkan`. Code is written using a [simpler C++ coding style](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b). Project requires `C++11` standard and a `x64` system. Currently the code is tested only on `Windows`, using `MSVC` (Visual Studio) and `MINGW` (Visual Studio Code) compilers, but the `Linux` is not completely supported yet.

![Demo](https://github.com/milkru/data_resources/blob/main/vulkanizer/buddha.png)

## Features
* Mesh loading using [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)
* Mesh optimizations using [meshoptimizer](https://github.com/zeux/meshoptimizer)
* Single mesh rendering
* In-flight frames
* CPU profiling using [easy_profiler](https://github.com/yse/easy_profiler)
* GPU profiling using query timestamps and pipeline statistics

## Installation
This project uses [CMake](https://cmake.org/download/) as a build tool. Since the project is built using `Vulkan`, the latest [Vulkan SDK](https://vulkan.lunarg.com) is required.

## License
Distributed under the MIT License. See `LICENSE` for more information.
