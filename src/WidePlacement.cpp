#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>

// Windows Task View (Windows 10/11) uses:
// - Horizontal rows with equal-sized tiles
// - Strong preference for horizontal layouts (more columns, fewer rows)
// - Tight, uniform spacing
// - Tiles try to maintain 16:9 aspect ratio
// - Clean, uniform grid without size variation

PlacementResult widePlacement(const std::vector<WindowInfo> &windows,
                              const ScreenInfo &screen) {
  PlacementResult result;

  const size_t windowCount = windows.size();

  if (windowCount == 0) {
    result.gridCols = 1;
    result.gridRows = 1;
    return result;
  }

  // Special case: single window at 75% of screen (Windows style)
  if (windowCount == 1) {
    result.gridCols = 1;
    result.gridRows = 1;
    result.tiles.resize(1);

    double tileWidth = screen.width * 0.75;
    double tileHeight = screen.height * 0.75;

    result.tiles[0] = {screen.offsetX + (screen.width - tileWidth) / 2.0,
                       screen.offsetY + (screen.height - tileHeight) / 2.0,
                       tileWidth - 2.0 * screen.margin,
                       tileHeight - 2.0 * screen.margin};

    return result;
  }

  // Windows strongly prefers horizontal layouts
  // Uses more columns and fewer rows than other systems
  int cols;
  if (windowCount == 2) {
    cols = 2;
  } else if (windowCount <= 4) {
    cols = 4; // Windows shows 4 across even for 3-4 windows
  } else if (windowCount <= 8) {
    cols = 4;
  } else if (windowCount <= 12) {
    cols = 5;
  } else if (windowCount <= 20) {
    cols = 6;
  } else {
    cols = 7;
  }

  int rows = (windowCount + cols - 1) / cols;

  result.gridCols = cols;
  result.gridRows = rows;
  result.tiles.resize(windowCount);

  // Wide placement uses generous spacing for breathing room
  double spacing = screen.margin * 2.5;

  // Calculate available space
  double availableWidth = screen.width - spacing * (cols + 1);
  double availableHeight = screen.height - spacing * (rows + 1);

  double tileWidth = availableWidth / cols;
  double tileHeight = availableHeight / rows;

  // Windows tries to maintain 16:9 aspect ratio for tiles
  double targetAspect = 16.0 / 9.0;
  double currentAspect = tileWidth / tileHeight;

  // Adjust dimensions to get closer to 16:9
  if (currentAspect > targetAspect * 1.2) {
    // Too wide, reduce width
    tileWidth = tileHeight * targetAspect;
  } else if (currentAspect < targetAspect * 0.8) {
    // Too tall, reduce height
    tileHeight = tileWidth / targetAspect;
  }

  // Place tiles in uniform grid
  for (size_t i = 0; i < windowCount; ++i) {
    int col = i % cols;
    int row = i / cols;

    double x = spacing + col * (tileWidth + spacing);
    double y = spacing + row * (tileHeight + spacing);

    result.tiles[i] = {screen.offsetX + x, screen.offsetY + y, tileWidth,
                       tileHeight};
  }

  // Center the grid
  double totalWidth = cols * tileWidth + (cols + 1) * spacing;
  double totalHeight = rows * tileHeight + (rows + 1) * spacing;

  double offsetX = (screen.width - totalWidth) / 2.0;
  double offsetY = (screen.height - totalHeight) / 2.0;

  for (size_t i = 0; i < windowCount; ++i) {
    result.tiles[i].x += offsetX;
    result.tiles[i].y += offsetY;
  }

  return result;
}
