#include <array>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace Fate::PathUtils {
    std::filesystem::path getExecutablePath() {
#if defined(_WIN32)
        std::wstring buffer(MAX_PATH, L'\0');

        while (true) {
            const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), buffer.size());
            if (len == 0) return {};
            if (len < buffer.size() - 1) {
                buffer.resize(len);
                return {buffer};
            }
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__linux__)
        char buffer[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (len < 0) return {};

        buffer[len] = '\0';
        return {buffer};
#else
        return {};
#endif
    }

    std::filesystem::path getEnginePath() {
        return getExecutablePath().parent_path().parent_path();
    }

    void openBrowser(const std::string& url) {
        std::string command;

#if defined(_WIN32)
        command = "start " + url;
#elif defined(__linux__)
        command = "xdg-open " + url;
#else
        return;
#endif

        std::system(command.c_str());
    }
}
