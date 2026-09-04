<div align="middle">
    <img src=".github/fate-banner-logo.webp" width="512" alt="the fate logo" />
    <br/>
    <h1>the Fate game engine</h1>
</div>

A modern, cross-platform game engine with a Vulkan renderer.

# Usage

```shell
# See fate-editor --help for more information

fate-editor init my-project # create a new fate project at ./my-project
fate-editor open my-project # open your new project!
```

# Development

Expects copies of the Vulkan SDK (1.3+) and Slang (`slangc`) in the environment.

Build with CMake (>=3.28, 4.4 ideal) and your choice of MSVC, GCC, or LLVM, across Windows, Linux, or Mac.

```shell
cmake -B build/ # Ninja: pass `-DCMAKE_BUILD_TYPE=Release` here
cmake --build build/ --parallel # Visual Studio: pass `--config Release` here
```

Your IDE can probably do this for you, if you'd rather not use the terminal.

Mac builds require KosmicKrisp for runtime Vulkan → Metal translation, which is included as part of the LunarG Vulkan SDK.