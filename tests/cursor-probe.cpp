#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <lua.hpp>
#include <stdexcept>

// Test-only observer. Load exclusively in the nested test compositor.
APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
  if (!HyprlandAPI::addLuaFunction(handle, "hyprview_test", "has_cursor", [](lua_State *L) -> int {
        const auto &cursor = Pointer::mgr()->currentCursorImage();
        lua_pushboolean(L, cursor.pBuffer || cursor.surface);
        return 1;
      }))
    throw std::runtime_error("Cannot register cursor test observer");
  return {"hyprview-cursor-test", "Observe the cursor image in nested tests", "Hyprview tests", "1"};
}

APICALL EXPORT void PLUGIN_EXIT() {}
