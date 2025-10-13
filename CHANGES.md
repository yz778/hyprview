# Proposed Changes for hyprview

## Overview
This pull request fixes critical rendering issues on multi-monitor setups and adds several quality-of-life enhancements to improve the user experience.

## Problem Statement
The original implementation used `monitor->m_size` which represents the logical size of the monitor. On multi-monitor setups or with certain scaling configurations, this caused:
- Incorrect rendering boundaries
- Recursive layer issues
- Misaligned window grids
- Poor touch/mouse coordinate calculations

## Solution
Replace all instances of `monitor->m_size` with `monitor->m_pixelSize` to use the full physical monitor dimensions at native resolution.

## Detailed Changes

### 1. Monitor Size Calculations (Core Fix)
**Files Modified:** `src/hyprview.cpp`, `src/HyprViewPassElement.cpp`

**Problem:** Using logical size (`m_size`) caused rendering issues on scaled or multi-monitor setups.

**Solution:** Use physical size (`m_pixelSize`) for all monitor dimension calculations.

**Affected Areas:**
- Background capture framebuffer allocation
- Grid tile size calculations
- Mouse position to tile index conversion
- Animation size/position calculations
- Opaque region definitions

**Example:**
```cpp
// Before
const auto MONITOR_SIZE = monitor->m_size;

// After
const auto MONITOR_SIZE = monitor->m_pixelSize;
```

### 2. Grid Centering Enhancement
**File:** `src/hyprview.cpp` (fullRender function)

**Enhancement:** Center the entire window grid on the screen for better aesthetics.

**Implementation:**
- Calculate actual grid dimensions based on number of windows
- Compute offsets to center the grid horizontally and vertically
- Apply offsets to cell positioning

**Benefits:**
- More balanced visual appearance
- Better use of screen real estate
- Clearer separation from screen edges

### 3. Workspace Number Display
**File:** `src/hyprview.cpp` (fullRender function)

**Enhancement:** Display workspace ID on each window tile for easier navigation.

**Implementation:**
- Render workspace number in top-left corner of each tile
- Use bright red text (RGB: 1.0, 0.2, 0.2) with 48pt font
- Add semi-transparent dark background (85% opacity) for contrast
- Include padding and rounded corners for professional appearance

**Benefits:**
- Quick visual identification of window workspaces
- Easier navigation in multi-workspace layouts
- No need to remember which workspace windows belong to

### 4. Dim Overlay for Overview Mode
**File:** `src/hyprview.cpp` (fullRender function)

**Enhancement:** Add a 40% dark overlay when overview is active.

**Implementation:**
```cpp
double dimAlpha = 0.4; // 40% dark overlay
g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0, 0.0, 0.0, dimAlpha), {});
```

**Benefits:**
- Clear visual distinction between overview and normal mode
- Especially useful when no windows are open
- Improves overall user experience clarity

### 5. Gesture Conflict Prevention
**File:** `src/main.cpp`

**Enhancement:** Block workspace gestures when overview is active.

**Implementation:**
- Hook into `swipeBegin`, `swipeUpdate`, and `swipeEnd` events
- Check if overview is active and not a hyprview gesture
- Cancel conflicting workspace gestures

**Benefits:**
- Prevents accidental workspace switches during overview
- Cleaner gesture interaction model
- Better user control

### 6. Swipe Flag Visibility
**File:** `src/hyprview.hpp`

**Change:** Move `swipe` member from private to public.

**Rationale:** Required for gesture hooks to detect hyprview-initiated gestures and prevent conflicts.

## Testing Recommendations

### Test Cases:
1. **Single Monitor:** Verify rendering at various resolutions (1920x1080, 2560x1440, 3840x2160)
2. **Multi-Monitor:** Test with different monitor configurations (same/different resolutions, scaling)
3. **Scaling:** Test with various Hyprland scaling factors (1.0, 1.25, 1.5, 2.0)
4. **Window Counts:** Test with 1, 2, 4, 9, 12+ windows
5. **Workspaces:** Verify workspace numbers display correctly across multiple workspaces
6. **Gestures:** Test that workspace gestures are blocked during overview
7. **Mouse Selection:** Verify accurate tile selection with mouse/trackpad

### Expected Behavior:
- Window grids render correctly without artifacts
- Grid is centered on screen
- Workspace numbers are clearly visible
- Dim overlay is present when overview is active
- No gesture conflicts occur
- Mouse/touch selection works accurately

## Backward Compatibility
These changes should be fully backward compatible. The modifications only affect internal rendering logic and add optional visual enhancements.

## Performance Impact
Minimal to none. The changes primarily affect existing rendering paths with no significant computational overhead. Workspace number rendering adds negligible overhead as it only renders text once per frame per visible window.

## Screenshots/Videos
(User can add screenshots showing before/after comparisons)

## Additional Notes
- All changes follow the existing code style and conventions
- No external dependencies added
- Changes are localized to rendering and event handling
- Documentation comments updated where appropriate
