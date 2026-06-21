#pragma once

#include "glad/glad.h"

class GPUMeshHandle {
    GLuint vboOffset{};
    GLuint eboOffset{};

public:
    explicit GPUMeshHandle(const GLuint vboOffset, const GLuint eboOffset)
        : vboOffset(vboOffset), eboOffset(eboOffset) {
    }

    [[nodiscard]] GLuint getVboOffset() const { return vboOffset; }
    [[nodiscard]] GLuint getEboOffset() const { return eboOffset; }
};
