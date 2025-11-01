#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

PlacementResult flowPlacement(const std::vector<WindowInfo> &windows,
                              const ScreenInfo &screen) {
  PlacementResult result;

  const size_t windowCount = windows.size();

  if (windowCount == 0) {
    result.gridCols = 1;
    result.gridRows = 1;
    return result;
  }

  // Special case: single window at 60% of screen
  if (windowCount == 1) {
    result.gridCols = 1;
    result.gridRows = 1;
    result.tiles.resize(1);

    double tileWidth = screen.width * 0.6;
    double tileHeight = screen.height * 0.6;

    result.tiles[0] = {screen.offsetX + (screen.width - tileWidth) / 2.0,
                       screen.offsetY + (screen.height - tileHeight) / 2.0,
                       tileWidth - 2.0 * screen.margin,
                       tileHeight - 2.0 * screen.margin};

    return result;
  }

  double spacing = screen.margin * 2.0;

  // Calculate initial scale for windows
  double totalWindowArea = 0.0;
  for (const auto &w : windows) {
    totalWindowArea += w.width * w.height;
  }

  double availableArea = screen.width * screen.height * 0.75;
  double areaScale = std::sqrt(availableArea / totalWindowArea);

  // Scale each window and calculate widths for packing
  struct WindowData {
    size_t index;
    double width;
    double height;
    double area;
  };

  std::vector<WindowData> windowData(windowCount);
  for (size_t i = 0; i < windowCount; ++i) {
    windowData[i].index = i;
    windowData[i].width = windows[i].width * areaScale;
    windowData[i].height = windows[i].height * areaScale;
    windowData[i].area = windowData[i].width * windowData[i].height;
  }

  // Sort windows by area (largest first) for better packing
  std::sort(
      windowData.begin(), windowData.end(),
      [](const WindowData &a, const WindowData &b) { return a.area > b.area; });

  // Distribute windows into rows using a balanced approach
  // Aim for balanced row widths rather than fixed column count
  std::vector<std::vector<size_t>> rows;
  std::vector<double> rowWidths;

  double targetRowWidth = screen.width * 0.85; // Target width per row

  for (const auto &wd : windowData) {
    // Try to add to existing row if it doesn't exceed target width too much
    bool placed = false;

    for (size_t r = 0; r < rows.size(); ++r) {
      double newWidth = rowWidths[r] + wd.width + spacing;

      // Allow up to 110% of target width to avoid too many rows
      if (newWidth < targetRowWidth * 1.1) {
        rows[r].push_back(wd.index);
        rowWidths[r] = newWidth;
        placed = true;
        break;
      }
    }

    // Start new row if not placed
    if (!placed) {
      rows.push_back({wd.index});
      rowWidths.push_back(wd.width + spacing);
    }
  }

  int numRows = rows.size();
  result.gridRows = numRows;
  result.gridCols = 0;
  for (const auto &row : rows) {
    result.gridCols = std::max(result.gridCols, (int)row.size());
  }
  result.tiles.resize(windowCount);

  // Create lookup for scaled dimensions
  std::vector<double> windowWidths(windowCount);
  std::vector<double> windowHeights(windowCount);
  for (const auto &wd : windowData) {
    windowWidths[wd.index] = wd.width;
    windowHeights[wd.index] = wd.height;
  }

  // Calculate row heights
  std::vector<double> rowHeights(numRows, 0.0);
  for (int r = 0; r < numRows; ++r) {
    for (size_t idx : rows[r]) {
      rowHeights[r] = std::max(rowHeights[r], windowHeights[idx]);
    }
  }

  // Calculate total layout height
  double totalHeight = spacing;
  for (double h : rowHeights)
    totalHeight += h + spacing;

  // Check if we need to scale down to fit
  double maxRowWidth = *std::max_element(rowWidths.begin(), rowWidths.end());

  if (maxRowWidth > screen.width * 0.95 || totalHeight > screen.height * 0.95) {
    double widthScale = (screen.width * 0.95) / maxRowWidth;
    double heightScale = (screen.height * 0.95) / totalHeight;
    double fitScale = std::min(widthScale, heightScale);

    for (size_t i = 0; i < windowCount; ++i) {
      windowWidths[i] *= fitScale;
      windowHeights[i] *= fitScale;
    }

    for (double &h : rowHeights)
      h *= fitScale;
    for (double &w : rowWidths)
      w *= fitScale;
    totalHeight *= fitScale;
  }

  // Place windows row by row
  double startY = (screen.height - totalHeight) / 2.0 + spacing;

  for (int r = 0; r < numRows; ++r) {
    double y = startY;
    for (int prevRow = 0; prevRow < r; ++prevRow) {
      y += rowHeights[prevRow] + spacing;
    }

    // Calculate actual row width
    double actualRowWidth = -spacing;
    for (size_t idx : rows[r]) {
      actualRowWidth += windowWidths[idx] + spacing;
    }

    // Center this row
    double rowStartX = (screen.width - actualRowWidth) / 2.0;
    double x = rowStartX;

    for (size_t idx : rows[r]) {
      // Center window vertically in row
      double verticalOffset = (rowHeights[r] - windowHeights[idx]) / 2.0;

      result.tiles[idx] = {screen.offsetX + x,
                           screen.offsetY + y + verticalOffset,
                           windowWidths[idx], windowHeights[idx]};

      x += windowWidths[idx] + spacing;
    }
  }

  return result;
}
