#include "Files.h"

#include <array>
#include <format>

namespace FileUtils {
    std::string prettyBytes(const std::size_t bytes) {
        constexpr std::array<std::string_view, 5> units = {"B", "KB", "MB", "GB", "TB"};
        auto size = static_cast<double>(bytes);
        std::size_t unitIndex = 0;

        while (size >= 1000 && unitIndex < units.size() - 1) {
            size /= 1000;
            ++unitIndex;
        }

        return std::format("{:.3} {}", size, units[unitIndex]);
    }
}
