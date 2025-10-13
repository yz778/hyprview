#include "ViewGesture.hpp"
#include "hyprview.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/helpers/Monitor.hpp>

void CViewGesture::begin(const ITrackpadGesture::STrackpadGestureBegin &e) {
  ITrackpadGesture::begin(e);

  m_lastDelta = 0.F;
  m_firstUpdate = true;

  auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
  if (!PMONITOR)
    return;

  auto it = g_pHyprViewInstances.find(PMONITOR);
  if (it == g_pHyprViewInstances.end() || !it->second) {
    // Create overview for this monitor
    g_pHyprViewInstances[PMONITOR] = std::make_unique<CHyprView>(PMONITOR, PMONITOR->m_activeWorkspace, true);
  } else {
    // Close the overview - but don't call selectHoveredWindow()
    // The gesture swipe is not an explicit selection
    it->second->setClosing(true);
  }
}

void CViewGesture::update(const ITrackpadGesture::STrackpadGestureUpdate &e) {
  if (m_firstUpdate) {
    m_firstUpdate = false;
    return;
  }

  m_lastDelta += distance(e);

  if (m_lastDelta <= 0.01) // plugin will crash if swipe ends at <= 0
    m_lastDelta = 0.01;

  auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
  if (!PMONITOR)
    return;

  auto it = g_pHyprViewInstances.find(PMONITOR);
  if (it != g_pHyprViewInstances.end() && it->second)
    it->second->onSwipeUpdate(m_lastDelta);
}

void CViewGesture::end(const ITrackpadGesture::STrackpadGestureEnd &e) {
  auto PMONITOR = g_pCompositor->m_lastMonitor.lock();
  if (!PMONITOR)
    return;

  auto it = g_pHyprViewInstances.find(PMONITOR);
  if (it != g_pHyprViewInstances.end() && it->second) {
    it->second->setClosing(false);
    it->second->onSwipeEnd();
    it->second->resetSwipe();
  }
}
