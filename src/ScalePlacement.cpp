#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

PlacementResult scalePlacement(const std::vector<WindowInfo> &windows,
                               const ScreenInfo &screen) {
  PlacementResult result;

  const size_t windowCount = windows.size();

  if (windowCount == 0) {
    result.gridCols = 1;
    result.gridRows = 1;
    return result;
  }

  // Special case: single window
  if (windowCount == 1) {
    result.gridCols = 1;
    result.gridRows = 1;
    result.tiles.resize(1);

    // Calculate tile size with aspect ratio preservation
    double tileWidth = screen.width * 0.8;
    double tileHeight = screen.height * 0.8;

    // Adjust based on original aspect ratio
    double originalAspect = windows[0].width / windows[0].height;
    double screenAspect = tileWidth / tileHeight;

    if (originalAspect > screenAspect) {
      tileHeight = tileWidth / originalAspect;
    } else {
      tileWidth = tileHeight * originalAspect;
    }

    result.tiles[0] = {screen.offsetX + (screen.width - tileWidth) / 2.0,
                       screen.offsetY + (screen.height - tileHeight) / 2.0,
                       tileWidth, tileHeight};

    return result;
  }

  // Wayfire scale algorithm: Calculate grid dimensions
  // Use sqrt(N + 1) for rows to create a balanced grid
  int rows = static_cast<int>(std::sqrt(windowCount + 1));
  int cols = static_cast<int>(std::ceil(static_cast<double>(windowCount) / rows));

  // Ensure we have enough cells
  while (rows * cols < windowCount) {
    if (rows <= cols) {
      rows++;
    } else {
      cols++;
    }
  }

  result.gridRows = rows;
  result.gridCols = cols;

  // Apply outer margin to workarea (Wayfire uses outer_margin)
  // We'll use screen.margin as outer_margin
  double outerMargin = screen.margin;
  double spacing = screen.margin * 3.0; // spacing between windows (increased for better separation)

  // Adjust workarea for outer margins
  double workareaX = screen.offsetX + outerMargin;
  double workareaY = screen.offsetY + outerMargin;
  double workareaWidth = screen.width - 2.0 * outerMargin;
  double workareaHeight = screen.height - 2.0 * outerMargin;

  // Calculate slot dimensions
  // Available space = workarea - (num_gaps * spacing)
  // num_gaps = num_cells + 1 (spacing on edges + between cells)
  double slotHeight = std::max(
      (workareaHeight - (rows + 1) * spacing) / rows,
      1.0);
  double slotWidth = std::max(
      (workareaWidth - (cols + 1) * spacing) / cols,
      1.0);

  result.tiles.resize(windowCount);

  // Place windows in grid slots
  for (size_t idx = 0; idx < windowCount; ++idx) {
    int row = idx / cols;
    int col = idx % cols;

    // Calculate slot position (top-left corner)
    // Position = workarea_start + spacing + (spacing + slot_size) * index
    double slotX = workareaX + spacing + col * (spacing + slotWidth);
    double slotY = workareaY + spacing + row * (spacing + slotHeight);

    const auto &window = windows[idx];

    // Calculate scale factor to fit window in slot while preserving aspect ratio
    double scale = std::min(slotWidth / window.width, slotHeight / window.height);

    // Wayfire's default behavior: don't scale windows larger than original
    // (allow_scale_zoom = false by default)
    scale = std::min(scale, 1.0);

    // Calculate scaled window dimensions
    double scaledWidth = window.width * scale;
    double scaledHeight = window.height * scale;

    // Center the window within its slot
    // Slot center calculation
    double slotCenterX = slotX + slotWidth / 2.0;
    double slotCenterY = slotY + slotHeight / 2.0;

    // Position window so its center aligns with slot center
    double windowX = slotCenterX - scaledWidth / 2.0;
    double windowY = slotCenterY - scaledHeight / 2.0;

    result.tiles[idx] = {windowX, windowY, scaledWidth, scaledHeight};
  }

  return result;
}
