# Else exist specifically for clang
ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

# Build directory
BUILD_DIR = ../build

# Source files
SRCS = main.cpp hyprview.cpp ViewGesture.cpp HyprViewPassElement.cpp GridPlacement.cpp SpiralPlacement.cpp FlowPlacement.cpp AdaptivePlacement.cpp WidePlacement.cpp ScalePlacement.cpp

# Object files
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.cpp=.o))

# Output file
TARGET = $(BUILD_DIR)/hyprview.so

# Compiler flags
# Use system headers from /usr/include instead of hyprpm cache
CXXFLAGS = -shared -fPIC $(EXTRA_FLAGS) -g -I/usr/include -I/usr/include/hyprland -I/usr/include/hyprland/src -I/usr/include/hyprland/protocols `pkg-config --cflags pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon` -std=c++2b -Wno-narrowing

.PHONY: all clean format

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c -fPIC $(EXTRA_FLAGS) -g -I/usr/include -I/usr/include/hyprland -I/usr/include/hyprland/src -I/usr/include/hyprland/protocols `pkg-config --cflags pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon` -std=c++2b -Wno-narrowing $< -o $@

clean:
	rm -rf $(BUILD_DIR)

format:
	find . -type f \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -style=llvm --style='{ColumnLimit: 0}' -i
