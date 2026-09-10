#pragma once

// Hyprland 0.54 renamed Debug::log to Log::logger->log.

#include <hyprland/src/debug/log/Logger.hpp>

// Old global log-level tokens (0.51 had these in scope via debug/Log.hpp).
inline constexpr Hyprutils::CLI::eLogLevel LOG  = Hyprutils::CLI::LOG_DEBUG;
inline constexpr Hyprutils::CLI::eLogLevel WARN = Hyprutils::CLI::LOG_WARN;

namespace Debug {
    template <typename... Args>
    inline void log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
        if (Log::logger)
            Log::logger->log(level, fmt, std::forward<Args>(args)...);
    }
    inline void log(Hyprutils::CLI::eLogLevel level, const std::string_view& str) {
        if (Log::logger)
            Log::logger->log(level, str);
    }
}
