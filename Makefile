OUT = subtitles
CXX = x86_64-w64-mingw32-g++
TARGET = x86_64-pc-windows-gnu
SRC = $(wildcard Source/*.cpp) $(wildcard Dependencies/imgui/*.cpp) Dependencies/imgui/backends/imgui_impl_dx11.cpp Dependencies/imgui/backends/imgui_impl_win32.cpp
SRC += $(filter-out Dependencies/Detours/src/uimports.cpp,$(wildcard Dependencies/Detours/src/*.cpp))
OBJ = $(addprefix $(TARGET)/,$(SRC:.cpp=.o))
INCLUDE = -ISource -IDependencies/Detours/src -IDependencies/imgui -IDependencies/imgui/backends -IDependencies/toml11
DEFINES = -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x501
CXXFLAGS = -Wall -Wextra -Ofast
LDFLAGS = -shared -static -s -pthread -lpsapi -lgdi32 -ldwmapi -ld3dcompiler

all: options $(OUT)

.PHONY: options
options:
	@echo "CXXFLAGS	= $(CXXFLAGS)"
	@echo "LDFLAGS		= $(LDFLAGS)"
	@echo "CXX		= $(CXX)"

.PHONY: dirs
dirs:
	@mkdir -p $(TARGET)/Source
	@mkdir -p $(TARGET)/Dependencies/Detours/src
	@mkdir -p $(TARGET)/Dependencies/imgui/backends

$(TARGET)/%.o: %.cpp
	@echo "BUILD	$@"
	@bear -- $(CXX) -c $(CXXFLAGS) $(DEFINES) $(INCLUDE) $< -o $@

.PHONY: $(OUT)
$(OUT): dirs $(OBJ)
	@echo "LINK	$@"
	@$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDE) -o $(TARGET)/$@.dll $(OBJ) $(LDFLAGS) $(LIBS)

.PHONY: dist
dist: $(OUT)
	mkdir -p out/
	cp $(TARGET)/$(OUT).dll out/
	cp dist/* out/
	cd out && 7z a -t7z ../$(OUT).7z .
	rm -rf out

.PHONY: clean
clean:
	rm -rf $(TARGET)
