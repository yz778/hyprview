#include "HyprViewPassElement.hpp"
#include "globals.hpp"
#include "hyprview.hpp"
#include <hyprland/src/render/OpenGL.hpp>

CHyprViewPassElement::CHyprViewPassElement(CHyprView *instance_) : instance(instance_) { ; }

void CHyprViewPassElement::draw(const CRegion &damage) {
  LOG_FILE("=== CHyprViewPassElement::draw() CALLED ===");
  LOG_FILE(std::string("draw(): instance pointer=") + std::to_string((unsigned long)instance));
  LOG_FILE(std::string("draw(): g_pHyprViewInstances.size()=") + std::to_string(g_pHyprViewInstances.size()));

  // Check if the instance still exists in the global map before rendering
  // This prevents crashes when the instance has been deleted
  bool instanceStillValid = false;
  for (auto &[monitor, inst] : g_pHyprViewInstances) {
    if (inst.get() == instance) {
      instanceStillValid = true;
      LOG_FILE("draw(): Found valid instance in map");
      break;
    }
  }

  if (!instanceStillValid) {
    LOG_FILE("draw(): Instance NOT valid - skipping render");
  } else if (!instance) {
    LOG_FILE("draw(): Instance pointer is null - skipping render");
  } else {
    LOG_FILE("draw(): Instance valid, calling fullRender()");
    instance->fullRender();
    LOG_FILE("draw(): fullRender() completed");
  }

  LOG_FILE("=== CHyprViewPassElement::draw() FINISHED ===");
}

bool CHyprViewPassElement::needsLiveBlur() { return false; }

bool CHyprViewPassElement::needsPrecomputeBlur() { return false; }

std::optional<CBox> CHyprViewPassElement::boundingBox() {
  // Return nullopt to indicate full screen rendering across all monitors
  return std::nullopt;
}

CRegion CHyprViewPassElement::opaqueRegion() {
  // Return the full monitor region to block windows underneath
  // The overview layer needs to be opaque to mask the real windows
  if (!instance || !instance->pMonitor.lock())
    return CRegion{};

  auto monitor = instance->pMonitor.lock();
  // Return the full monitor box as an opaque region
  CBox monitorBox = {monitor->m_position.x, monitor->m_position.y, monitor->m_size.x, monitor->m_size.y};
  return CRegion{monitorBox};
}
