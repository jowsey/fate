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

Expects a copy of the Vulkan SDK (1.3+) in the environment at `VULKAN_SDK`.

Build with CMake (>=3.28, 4.4 ideal) and your choice of toolchain across Windows, Linux, or Mac. I attempt to maintain consistent sane
defaults, so everything *should* just work for you.