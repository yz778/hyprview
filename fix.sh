#!/bin/bash

# Script to automate fixing the compilation errors in hyprview.cpp and compiling the project

# Backup the original file
cp hyprview.cpp hyprview.cpp.bak
echo "Backed up hyprview.cpp to hyprview.cpp.bak"

# Add the include for hyprgraphics if not present
if ! grep -q '#include <hyprgraphics/color/Color.hpp>' hyprview.cpp; then
    sed -i '/#include "HyprViewPassElement.hpp"/a #include <hyprgraphics/color/Color.hpp>' hyprview.cpp
    echo "Added include for <hyprgraphics/color/Color.hpp>"
fi

# Remove the toGraphicsColor function if it exists
if grep -q 'Hyprgraphics::CColor toGraphicsColor' hyprview.cpp; then
    sed -i '/Hyprgraphics::CColor toGraphicsColor(const CHyprColor& color) {/,/}/d' hyprview.cpp
    echo "Removed toGraphicsColor function"
fi

# Revert clear calls to use GRID_COLOR.stripA() directly
sed -i 's/g_pHyprOpenGL->clear(toGraphicsColor(GRID_COLOR.stripA()));/g_pHyprOpenGL->clear(GRID_COLOR.stripA());/g' hyprview.cpp
echo "Reverted clear calls to use GRID_COLOR.stripA()"

# Fix the background renderRect in fullRender to use CHyprColor with SRectRenderData
# Handle both CColor and CHyprColor cases
sed -i 's/g_pHyprOpenGL->renderRect(monitorBox, CColor(0, 0, 0, 0.2));/CHyprOpenGLImpl::SRectRenderData data; data.round = 0; g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0, 0.0, 0.0, 0.2), data);/g' hyprview.cpp
sed -i 's/g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0, 0.0, 0.0, 0.2));/CHyprOpenGLImpl::SRectRenderData data; data.round = 0; g_pHyprOpenGL->renderRect(monitorBox, CHyprColor(0.0, 0.0, 0.0, 0.2), data);/g' hyprview.cpp
echo "Fixed background renderRect to use CHyprColor with SRectRenderData"

# Fix the border renderRect to reuse existing data variable and avoid redeclaration
# Handle cases where data is redeclared or toGraphicsColor is used
sed -i 's/CHyprOpenGLImpl::SRectRenderData data; data.round = BORDER_RADIUS; g_pHyprOpenGL->renderRect(borderBox, BORDERCOLOR, data);/data.round = BORDER_RADIUS; g_pHyprOpenGL->renderRect(borderBox, BORDERCOLOR, data);/g' hyprview.cpp
sed -i 's/g_pHyprOpenGL->renderRect(borderBox, toGraphicsColor(BORDERCOLOR), data);/data.round = BORDER_RADIUS; g_pHyprOpenGL->renderRect(borderBox, BORDERCOLOR, data);/g' hyprview.cpp
echo "Fixed border renderRect to reuse SRectRenderData and avoid redeclaration"

# Attempt to compile
echo "Attempting to compile..."
make all

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
else
    echo "Compilation failed. Check the errors above. You may need to restore from backup: mv hyprview.cpp.bak hyprview.cpp"
fi
