#pragma once

#include "glad/glad.h"

class MeshHandle {
    GLuint baseVertex{};
    GLuint firstIndex{};

public:
    explicit MeshHandle(GLuint baseVertex, GLuint firstIndex);

    [[nodiscard]] GLuint getBaseVertex() const { return baseVertex; }
    [[nodiscard]] GLuint getFirstIndex() const { return firstIndex; }
};
