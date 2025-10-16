#pragma once
#include "globals.hpp"
#include <vector>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/OpenGL.hpp>

class CHyprView;

void gridPlacement(CHyprView* hyprview, std::vector<PHLWINDOW> &windowsToRender);
