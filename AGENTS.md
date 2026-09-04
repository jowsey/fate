fate is a cross-platform game engine with a Vulkan renderer.

# Development Guidelines

- Make precise, uninvasive edits. New code must follow surrounding convention and formatting.
- Prioritize readability over performance outside hot paths.
- Take initiative to use modern, clean, idiomatic C++.
- Take initiative to surface correctness and performance opportunities as found.
- Delegate to existing dependencies or STL where possible (i.e. "does [dependency/STL] have a type or method for this?")
- Maintain compatibility across Windows/Linux/Mac and compiler toolchains.
- Present work done in a clear and concise manner. Justify all meaningful changes.