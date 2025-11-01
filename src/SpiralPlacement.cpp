#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>

PlacementResult spiralPlacement(const std::vector<WindowInfo> &windows,
                                const ScreenInfo &screen) {
  PlacementResult result;

  const size_t windowCount = windows.size();

  if (windowCount == 0) {
    result.gridCols = 1;
    result.gridRows = 1;
    return result;
  }

  // Special case: single window at 80% of screen (centered)
  if (windowCount == 1) {
    result.gridCols = 1;
    result.gridRows = 1;
    result.tiles.resize(1);

    double tileWidth = screen.width * 0.8;
    double tileHeight = screen.height * 0.8;

    result.tiles[0] = {screen.offsetX + (screen.width - tileWidth) / 2.0,
                       screen.offsetY + (screen.height - tileHeight) / 2.0,
                       tileWidth - 2.0 * screen.margin,
                       tileHeight - 2.0 * screen.margin};

    return result;
  }

  // Determine grid size needed to fit all windows
  // We'll use a square or near-square grid
  int gridSize = (int)std::ceil(std::sqrt((double)windowCount));
  result.gridCols = gridSize;
  result.gridRows = gridSize;

  // Calculate tile dimensions
  double tileWidth = screen.width / gridSize;
  double tileHeight = screen.height / gridSize;

  // Ensure reasonable aspect ratio (not too tall)
  double maxTileHeight = tileWidth * 0.75; // 4:3 aspect ratio max
  if (tileHeight > maxTileHeight) {
    tileHeight = maxTileHeight;
  }

  result.tiles.resize(windowCount);

  // Generate spiral path from center outward
  // Start at center of grid
  int centerX = gridSize / 2;
  int centerY = gridSize / 2;

  // Spiral direction: right -> down -> left -> up -> repeat
  int dx = 1, dy = 0;
  int x = centerX, y = centerY;
  int segmentLength = 1;
  int segmentPassed = 0;
  int directionChanges = 0;

  size_t placedWindows = 0;

  // Place first window at center
  if (placedWindows < windowCount) {
    double posX = x * tileWidth + screen.margin;
    double posY = y * tileHeight + screen.margin;

    result.tiles[placedWindows] = {screen.offsetX + posX, screen.offsetY + posY,
                                   tileWidth - 2.0 * screen.margin,
                                   tileHeight - 2.0 * screen.margin};
    placedWindows++;
  }

  // Continue spiral outward
  while (placedWindows < windowCount) {
    // Move to next position
    x += dx;
    y += dy;

    // Check if position is within grid bounds
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
      double posX = x * tileWidth + screen.margin;
      double posY = y * tileHeight + screen.margin;

      result.tiles[placedWindows] = {
          screen.offsetX + posX, screen.offsetY + posY,
          tileWidth - 2.0 * screen.margin, tileHeight - 2.0 * screen.margin};
      placedWindows++;
    }

    segmentPassed++;

    // Change direction after completing a segment
    if (segmentPassed == segmentLength) {
      segmentPassed = 0;

      // Rotate direction: right->down, down->left, left->up, up->right
      int temp = dx;
      dx = -dy;
      dy = temp;

      directionChanges++;

      // Increase segment length after two direction changes
      if (directionChanges % 2 == 0) {
        segmentLength++;
      }
    }

    // Safety check to prevent infinite loop
    if (x < -gridSize || x > 2 * gridSize || y < -gridSize ||
        y > 2 * gridSize) {
      break;
    }
  }

  // Center the entire grid on screen
  double totalGridWidth = gridSize * tileWidth;
  double totalGridHeight = gridSize * tileHeight;

  double centerOffsetX = (screen.width - totalGridWidth) / 2.0;
  double centerOffsetY = (screen.height - totalGridHeight) / 2.0;

  for (size_t i = 0; i < windowCount; ++i) {
    result.tiles[i].x += centerOffsetX;
    result.tiles[i].y += centerOffsetY;
  }

  return result;
}
