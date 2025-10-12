#pragma once
#include <chrono>
#include <fstream>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <iomanip>
#include <sstream>
#include <string>

inline HANDLE PHANDLE = nullptr;

// File logging function for debugging
inline void LOG_FILE(const std::string &message) {
  // Check if debug logging is enabled
  static auto *const *PDEBUG = (Hyprlang::INT *const *)HyprlandAPI::getConfigValue(PHANDLE, "plugin:hyprview:debug_log")->getDataStaticPtr();
  if (!PDEBUG || !**PDEBUG)
    return;

  std::ofstream logFile("/tmp/hyprview.log", std::ios::app);
  if (logFile.is_open()) {
    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    logFile << "[" << oss.str() << "] " << message << std::endl;
    logFile.close();
  }
}