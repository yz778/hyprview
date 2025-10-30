#include "HyprViewPassElement.hpp"
#include "hyprview.hpp"
#include <hyprland/src/render/OpenGL.hpp>

CHyprViewPassElement::CHyprViewPassElement(CHyprView *instance_)
    : instance(instance_) {
  ;
}

void CHyprViewPassElement::draw(const CRegion &damage) {

  // Check if the instance still exists in the global map before rendering
  // This prevents crashes when the instance has been deleted
  bool instanceStillValid = false;
  for (auto &[monitor, inst] : g_pHyprViewInstances) {
    if (inst.get() == instance) {
      instanceStillValid = true;

      break;
    }
  }

  if (!instanceStillValid) {

  } else if (!instance) {

  } else {
    instance->fullRender();
  }
}

bool CHyprViewPassElement::needsLiveBlur() { return false; }

bool CHyprViewPassElement::needsPrecomputeBlur() { return false; }

std::optional<CBox> CHyprViewPassElement::boundingBox() {
  // Return nullopt for unbounded rendering
  return std::nullopt;
}

CRegion CHyprViewPassElement::opaqueRegion() {
  // Return the full monitor region to block windows underneath
  // The overview layer needs to be opaque to mask the real windows
  if (!instance || !instance->pMonitor.lock())
    return CRegion{};

  auto monitor = instance->pMonitor.lock();
  // Return the full monitor box as an opaque region using full physical size
  Vector2D fullMonitorSize = monitor->m_pixelSize;
  CBox monitorBox = {monitor->m_position.x, monitor->m_position.y,
                     fullMonitorSize.x, fullMonitorSize.y};
  return CRegion{monitorBox};
}
