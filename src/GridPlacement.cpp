#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>

PlacementResult gridPlacement(const std::vector<WindowInfo>& windows, const ScreenInfo& screen) {
    PlacementResult result;

    const size_t windowCount = windows.size();

    if (windowCount == 0) {
        result.gridCols = 1;
        result.gridRows = 1;
        return result;
    }

    // Step 1: Determine grid dimensions (fixed thresholds)
    if (windowCount == 1) {
        result.gridCols = 1;
        result.gridRows = 1;
    } else if (windowCount <= 2) {
        result.gridCols = 2;
        result.gridRows = 1;
    } else if (windowCount <= 4) {
        result.gridCols = 2;
        result.gridRows = 2;
    } else if (windowCount <= 9) {
        result.gridCols = 3;
        result.gridRows = 3;
    } else {
        result.gridCols = 4;
        result.gridRows = (windowCount + 3) / 4; // Ceiling division
    }

    result.tiles.resize(windowCount);

    // Step 2: Calculate actual rows used (may be less than allocated)
    int actualRows = (windowCount + result.gridCols - 1) / result.gridCols;

    // Step 3: Calculate tile size to maximize screen usage
    double tileWidth = screen.width / result.gridCols;
    double tileHeight = tileWidth * 0.6; // Start with reasonable aspect ratio (16:10 ish)

    // Make sure total height doesn't exceed available screen height
    double maxTileHeight = screen.height / actualRows;
    if (tileHeight > maxTileHeight) {
        tileHeight = maxTileHeight;
    }

    // Special case: single window at 80% of screen
    if (windowCount == 1) {
        tileWidth = screen.width * 0.8;
        tileHeight = screen.height * 0.8;
    }

    // Account for margins
    double tileRenderWidth = tileWidth - 2.0 * screen.margin;
    double tileRenderHeight = tileHeight - 2.0 * screen.margin;

    // Step 4: Place the tiles
    for (size_t i = 0; i < windowCount; ++i) {
        int col = i % result.gridCols;
        int row = i / result.gridCols;

        result.tiles[i] = {
            col * tileWidth + screen.margin,
            row * tileHeight + screen.margin,
            tileRenderWidth,
            tileRenderHeight
        };
    }

    // Step 5: Center everything vertically (and horizontally)
    double totalGridWidth = result.gridCols * tileWidth;
    double totalGridHeight = actualRows * tileHeight;

    double centerOffsetX = (screen.width - totalGridWidth) / 2.0;
    double centerOffsetY = (screen.height - totalGridHeight) / 2.0;

    for (size_t i = 0; i < windowCount; ++i) {
        result.tiles[i].x += screen.offsetX + centerOffsetX;
        result.tiles[i].y += screen.offsetY + centerOffsetY;
    }

    return result;
}
