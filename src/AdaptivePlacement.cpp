#include "PlacementAlgorithms.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

PlacementResult adaptivePlacement(const std::vector<WindowInfo>& windows, const ScreenInfo& screen) {
    PlacementResult result;

    const size_t windowCount = windows.size();

    if (windowCount == 0) {
        result.gridCols = 1;
        result.gridRows = 1;
        return result;
    }

    if (windowCount == 1) {
        result.gridCols = 1;
        result.gridRows = 1;
        result.tiles.resize(1);

        double tileWidth = screen.width * 0.7;
        double tileHeight = screen.height * 0.7;

        result.tiles[0] = {
            screen.offsetX + (screen.width - tileWidth) / 2.0,
            screen.offsetY + (screen.height - tileHeight) / 2.0,
            tileWidth - 2.0 * screen.margin,
            tileHeight - 2.0 * screen.margin
        };

        return result;
    }

    int cols, rows;

    if (windowCount == 2) {
        cols = 2;
        rows = 1;
    } else if (windowCount <= 4) {
        cols = 2;
        rows = 2;
    } else if (windowCount <= 6) {
        cols = 3;
        rows = 2;
    } else if (windowCount <= 9) {
        cols = 3;
        rows = 3;
    } else if (windowCount <= 12) {
        cols = 4;
        rows = 3;
    } else {
        // For many windows, use a wider layout
        cols = std::min(5, (int)std::ceil(std::sqrt(windowCount * 1.5)));
        rows = (windowCount + cols - 1) / cols;
    }

    result.gridCols = cols;
    result.gridRows = rows;
    result.tiles.resize(windowCount);

    double spacing = screen.margin * 1.5;

    // Calculate total area and scale factor to fit windows proportionally
    double totalWindowArea = 0.0;
    for (const auto& w : windows) {
        totalWindowArea += w.width * w.height;
    }

    // Use 80% of screen area
    double availableArea = screen.width * screen.height * 0.80;
    double baseScale = std::sqrt(availableArea / totalWindowArea);

    // Scale each window maintaining aspect ratio
    std::vector<double> windowWidths(windowCount);
    std::vector<double> windowHeights(windowCount);

    for (size_t i = 0; i < windowCount; ++i) {
        windowWidths[i] = windows[i].width * baseScale;
        windowHeights[i] = windows[i].height * baseScale;

        double sizeRatio = (windows[i].width * windows[i].height) / (screen.width * screen.height);
        double variation = 1.0;

        if (sizeRatio > 0.5) {
            variation = 0.92; // Large windows scaled down slightly
        } else if (sizeRatio < 0.15) {
            variation = 1.12; // Small windows boosted
        }

        windowWidths[i] *= variation;
        windowHeights[i] *= variation;
    }

    // Calculate row heights (tallest window in each row)
    std::vector<double> rowHeights(rows, 0.0);
    for (size_t i = 0; i < windowCount; ++i) {
        int row = i / cols;
        rowHeights[row] = std::max(rowHeights[row], windowHeights[i]);
    }

    // Calculate total width of each row
    std::vector<double> rowWidths(rows, 0.0);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t i = r * cols + c;
            if (i >= windowCount) break;
            rowWidths[r] += windowWidths[i];
        }
        // Add spacing between windows
        int windowsInRow = std::min((int)(windowCount - r * cols), cols);
        rowWidths[r] += spacing * (windowsInRow - 1);
    }

    // Find max row width
    double maxRowWidth = 0.0;
    for (double w : rowWidths) {
        maxRowWidth = std::max(maxRowWidth, w);
    }

    // Total height with spacing
    double totalHeight = spacing;
    for (double h : rowHeights) totalHeight += h + spacing;

    // Check if layout fits screen, scale down if needed
    if (maxRowWidth > screen.width * 0.95 || totalHeight > screen.height * 0.95) {
        double widthScale = (screen.width * 0.95) / maxRowWidth;
        double heightScale = (screen.height * 0.95) / totalHeight;
        double fitScale = std::min(widthScale, heightScale);

        for (size_t i = 0; i < windowCount; ++i) {
            windowWidths[i] *= fitScale;
            windowHeights[i] *= fitScale;
        }

        for (double& h : rowHeights) h *= fitScale;
        for (double& w : rowWidths) w *= fitScale;
        maxRowWidth *= fitScale;
        totalHeight *= fitScale;
    }

    // Place windows row by row, each row centered independently
    double startY = (screen.height - totalHeight) / 2.0 + spacing;

    for (size_t i = 0; i < windowCount; ++i) {
        int row = i / cols;
        int col = i % cols;

        // Calculate Y position for this row
        double y = startY;
        for (int r = 0; r < row; ++r) {
            y += rowHeights[r] + spacing;
        }

        // Calculate X position - center this row
        double rowStartX = (screen.width - rowWidths[row]) / 2.0;
        double x = rowStartX;

        // Add widths of previous windows in this row
        for (int c2 = 0; c2 < col; ++c2) {
            size_t prevIdx = row * cols + c2;
            x += windowWidths[prevIdx] + spacing;
        }

        // Center window vertically within row height
        double verticalOffset = (rowHeights[row] - windowHeights[i]) / 2.0;

        result.tiles[i] = {
            screen.offsetX + x,
            screen.offsetY + y + verticalOffset,
            windowWidths[i],
            windowHeights[i]
        };
    }

    return result;
}
