#pragma once
#include <vector>
#include <cstddef>

// Pure data structures for placement algorithms
// No Hyprland dependencies - these are purely mathematical

struct TileRect {
    double x;
    double y;
    double width;
    double height;
};

struct WindowInfo {
    size_t id;              // Index/ID of the window
    double width;           // Original window width
    double height;          // Original window height
};

struct ScreenInfo {
    double width;           // Available screen width (after reserved areas)
    double height;          // Available screen height (after reserved areas)
    double offsetX;         // Offset from screen origin (reserved top-left X)
    double offsetY;         // Offset from screen origin (reserved top-left Y)
    double margin;          // Margin between tiles
};

struct PlacementResult {
    std::vector<TileRect> tiles;  // Tile positions for each window
    int gridCols;                 // Number of columns (for centering)
    int gridRows;                 // Number of rows (for centering)
};

// Pure placement algorithm functions
// These only perform mathematical calculations, no Hyprland/window operations

PlacementResult gridPlacement(const std::vector<WindowInfo>& windows, const ScreenInfo& screen);
