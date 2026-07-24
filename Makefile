CC := clang
CXX := clang++
AR := llvm-ar
MODE ?= debug
BLENDER ?= blender
PYTHON ?= python
FXC ?=

COMMON_SOURCES := \
	src/affect.c \
	src/affect_client.c \
	src/animation.c \
	src/app.c \
	src/bubble_layout.c \
	src/conversation.c \
	src/conversation_sources.c \
	src/delivery.c \
	src/dialogue.c \
	src/dialogue_art.c \
	src/draw.c \
	src/event_pump_sdl.c \
	src/expression_director.c \
	src/frame_clock.c \
	src/hook_output.c \
	src/humanoid.c \
	src/ik.c \
	src/json_scan.c \
	src/log.c \
	src/main.c \
	src/model.c \
	src/motion.c \
	src/motion_config.c \
	src/pose.c \
	src/pose_solver.c \
	src/presentation.c \
	src/presentation_event_queue.c \
	src/presentation_sdl_legacy.c \
	src/raster_sdl_legacy.c \
	src/portrait.c \
	src/portrait_motion.c \
	src/relay_core.c \
	src/scene.c \
	src/providers/codex_relay.c \
	src/providers/codex_stream.c \
	src/providers/live_source.c \
	src/providers/opencode_stream.c \
	src/provider_config.c \
	src/session_registry.c \
	src/settings_ui.c \
	src/state.c \
	src/text_renderer.c \
	src/user_settings.c

IMGUI_DIR := lib/imgui
DEAR_BINDINGS_GENERATED := lib/dear_bindings/generated
IMGUI_CPP_SOURCES := \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_demo.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdl3.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdlrenderer3.cpp \
	$(DEAR_BINDINGS_GENERATED)/dcimgui.cpp \
	$(DEAR_BINDINGS_GENERATED)/backends/dcimgui_impl_sdl3.cpp \
	$(DEAR_BINDINGS_GENERATED)/backends/dcimgui_impl_sdlrenderer3.cpp

ifeq ($(OS),Windows_NT)
PLATFORM := windows
EXE := .exe
SDL3_ROOT ?= C:/dev/SDL3
SDL3_TTF_ROOT ?= $(CURDIR)/.cache/sdl_ttf/SDL3_ttf-3.2.2
SHADERCROSS ?= $(CURDIR)/.cache/shadercross/bin/shadercross.exe
PLATFORM_SOURCES := src/platform/windows_ipc.c src/platform/windows_overlay.c \
	src/platform/windows_session_files.c src/raster_d3d11.c
PLATFORM_CPP_SOURCES := src/platform/windows_dcomp.cpp
CPPFLAGS += -Isrc -I"$(SDL3_ROOT)/include" -I"$(SDL3_TTF_ROOT)/include"
LDFLAGS += -L"$(SDL3_ROOT)/lib/x64" -L"$(SDL3_TTF_ROOT)/lib/x64"
LDLIBS += -lSDL3_ttf -lSDL3 -ldcomp -ldwmapi -ldxgi -ldxguid -luser32 -lgdi32 -ld3d11 \
	-lwinhttp -lws2_32 -lbcrypt -lole32
define make-dir
@powershell.exe -NoProfile -Command "New-Item -ItemType Directory -Force '$(subst /,\,$(dir $@))' | Out-Null"
endef
define copy-runtime
@powershell.exe -NoProfile -Command "if (Test-Path '$(SDL3_ROOT)/lib/x64/SDL3.dll') { Copy-Item -Force '$(SDL3_ROOT)/lib/x64/SDL3.dll' '$(dir $@)' }"
@powershell.exe -NoProfile -Command "if (Test-Path '$(SDL3_TTF_ROOT)/lib/x64/SDL3_ttf.dll') { Copy-Item -Force '$(SDL3_TTF_ROOT)/lib/x64/SDL3_ttf.dll' '$(dir $@)' }"
endef
define copy-output
@powershell.exe -NoProfile -Command "Copy-Item -Force '$(subst /,\,$<)' '$(subst /,\,$@)'"
endef
define remove-build
@powershell.exe -NoProfile -Command "if (Test-Path '$(BUILD_ROOT)') { Remove-Item -Recurse -Force '$(BUILD_ROOT)' }"
endef
else
PLATFORM := linux
EXE :=
PKG_CONFIG ?= pkg-config
SHADERCROSS ?= shadercross
PLATFORM_SOURCES := src/platform/linux_ipc.c src/platform/linux_overlay.c \
	src/platform/linux_session_files.c
PLATFORM_CPP_SOURCES :=
CPPFLAGS += -Isrc $(shell $(PKG_CONFIG) --cflags sdl3 SDL3_ttf)
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl3 SDL3_ttf) -lm
define make-dir
@mkdir -p "$(dir $@)"
endef
define copy-runtime
endef
define copy-output
@cp -f "$<" "$@"
endef
define remove-build
@rm -rf "$(BUILD_ROOT)"
endef
endif

BUILD_ROOT := build/$(PLATFORM)
OBJ_DIR := $(BUILD_ROOT)/obj/$(MODE)
BIN_DIR := $(BUILD_ROOT)/bin/$(MODE)
MODE_TARGET := $(BIN_DIR)/eidolon$(EXE)
TARGET := $(BUILD_ROOT)/eidolon$(EXE)
SHADER_SOURCE_DIR := shaders
SHADER_BUILD_DIR := $(BUILD_ROOT)/shaders/$(MODE)
SHADER_NAMES := model.vert model.frag
SPIRV_SHADERS := $(addprefix $(SHADER_BUILD_DIR)/SPIRV/,$(addsuffix .spv,$(SHADER_NAMES)))
DXIL_SHADERS := $(addprefix $(SHADER_BUILD_DIR)/DXIL/,$(addsuffix .dxil,$(SHADER_NAMES)))
DXBC_VERTEX_SHADER := $(SHADER_BUILD_DIR)/DXBC/model.vert.cso
DXBC_FRAGMENT_SHADER := $(SHADER_BUILD_DIR)/DXBC/model.frag.cso
DXBC_SHADERS := $(DXBC_VERTEX_SHADER) $(DXBC_FRAGMENT_SHADER)
ifeq ($(OS),Windows_NT)
SHADER_OUTPUTS := $(DXBC_SHADERS)
else
SHADER_OUTPUTS := $(SPIRV_SHADERS) $(DXIL_SHADERS)
endif
RUNTIME_MODEL := $(CURDIR)/assets/model/rio.glb
MOTION_CONFIG := $(CURDIR)/config/motion.cfg
CHARACTER_CONFIG := $(CURDIR)/config/character.cfg
SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCES)
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
IMGUI_OBJECTS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(IMGUI_CPP_SOURCES))
PLATFORM_CPP_OBJECTS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(PLATFORM_CPP_SOURCES))
IMGUI_DEPS := $(IMGUI_OBJECTS:.o=.d) $(PLATFORM_CPP_OBJECTS:.o=.d)

CPPFLAGS += -Ilib/cgltf -DEIDOLON_ASSET_DIR=\"$(abspath assets)\" \
	-DEIDOLON_AFFECT_WORKER_PATH=\"$(abspath $(BUILD_ROOT)/eidolon-affect-worker$(EXE))\" \
	"-DEIDOLON_FONT_PATH=\"$(CURDIR)/assets/fonts/MesloLG Nerd Font/MesloLGSNerdFontMono-Regular.ttf\"" \
	-DEIDOLON_MODEL_PATH=\"$(RUNTIME_MODEL)\" \
	-DEIDOLON_MOTION_CONFIG_PATH=\"$(abspath $(MOTION_CONFIG))\" \
	-DEIDOLON_SYSTEM_SETTINGS_PATH=\"$(abspath config/settings.cfg)\" \
	-DEIDOLON_CHARACTER_CONFIG_PATH=\"$(abspath $(CHARACTER_CONFIG))\" \
	-DEIDOLON_PROVIDER_CONFIG_PATH=\"$(abspath config/providers.cfg)\" \
	-DEIDOLON_SHADER_DIR=\"$(abspath $(SHADER_BUILD_DIR))\"
CPPFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends \
	-I$(DEAR_BINDINGS_GENERATED) -I$(DEAR_BINDINGS_GENERATED)/backends
IMGUI_CPPFLAGS := $(CPPFLAGS) -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends \
	-I$(DEAR_BINDINGS_GENERATED) -I$(DEAR_BINDINGS_GENERATED)/backends
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -MMD -MP
CXXFLAGS += -std=c++17 -MMD -MP

ifeq ($(MODE),release)
CFLAGS += -O2 -DNDEBUG
CXXFLAGS += -O2 -DNDEBUG
else ifeq ($(MODE),debug)
CFLAGS += -O0 -g
CXXFLAGS += -O0 -g
else
$(error MODE must be debug or release)
endif

TEST_CFLAGS := $(filter-out -MMD -MP,$(CFLAGS))

.PHONY: all force-output clean check editor-config imgui-smoke bgfx-smoke bgfx-interop-smoke bgfx-dcomp-smoke d3d11-dcomp-smoke win32-dcomp-backend-smoke sdl-renderer-dcomp-smoke sdl-gpu-dcomp-smoke graphics-backend-benchmark provider-live-test codex-relay-test shaders text-setup affect-setup affect affect-check affect-benchmark character-sprites character-sprites-download character-sprites-check model-audit model-material-audit model-export model-preview \
	model-preview-glb model-mouth model-mouth-sheet model-mouth-pick model-mouth-calibrate help log

all: $(TARGET)

editor-config:
	$(PYTHON) tools/generate_compile_commands.py --make "$(MAKE)" --mode "$(MODE)" \
		--target "$(MODE_TARGET)" --output "$(CURDIR)/compile_commands.json"

text-setup:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(CURDIR)/tools/setup_text_windows.ps1"

affect-setup:
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(CURDIR)/tools/setup_affect_windows.ps1"

ifeq ($(OS),Windows_NT)
AFFECT_CACHE := $(CURDIR)/.cache/affect
AFFECT_ORT := $(AFFECT_CACHE)/onnxruntime
AFFECT_MODEL_DIR := $(AFFECT_CACHE)/model
AFFECT_WORKER := $(BUILD_ROOT)/eidolon-affect-worker.exe
AFFECT_CLIENT_TEST := $(BUILD_ROOT)/tests/$(MODE)/affect_client_test.exe
AFFECT_BENCHMARK := $(BUILD_ROOT)/tools/$(MODE)/affect_benchmark.exe
WIN32_DCOMP_BACKEND_SMOKE := $(BUILD_ROOT)/tests/$(MODE)/windows_dcomp_backend_smoke.exe
WIN32_DCOMP_BACKEND_SMOKE_OBJECT := $(OBJ_DIR)/tests/windows_dcomp_backend_smoke.o

$(WIN32_DCOMP_BACKEND_SMOKE): $(WIN32_DCOMP_BACKEND_SMOKE_OBJECT) \
		$(OBJ_DIR)/src/presentation.o $(OBJ_DIR)/src/presentation_event_queue.o \
		$(OBJ_DIR)/src/scene.o \
		$(OBJ_DIR)/src/platform/windows_dcomp.o
	$(make-dir)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
	$(copy-runtime)

win32-dcomp-backend-smoke: $(WIN32_DCOMP_BACKEND_SMOKE)
	"$(WIN32_DCOMP_BACKEND_SMOKE)"

affect: $(AFFECT_WORKER)

$(AFFECT_WORKER): tools/affect_worker.c src/affect_tokenizer.c src/affect_tokenizer.h \
		src/affect_protocol.h $(AFFECT_ORT)/include/onnxruntime_c_api.h \
		$(AFFECT_MODEL_DIR)/model_quantized.onnx $(AFFECT_MODEL_DIR)/vocab.json \
		$(AFFECT_MODEL_DIR)/merges.txt
	$(make-dir)
	$(CC) -Isrc -I"$(AFFECT_ORT)/include" \
		-DEIDOLON_AFFECT_MODEL_PATH=L\"$(AFFECT_MODEL_DIR)/model_quantized.onnx\" \
		-DEIDOLON_AFFECT_VOCAB_PATH=\"$(AFFECT_MODEL_DIR)/vocab.json\" \
		-DEIDOLON_AFFECT_MERGES_PATH=\"$(AFFECT_MODEL_DIR)/merges.txt\" \
		$(filter-out -MMD -MP,$(CFLAGS)) tools/affect_worker.c src/affect_tokenizer.c \
		-L"$(AFFECT_ORT)/lib" -lonnxruntime -o $@
	@powershell.exe -NoProfile -Command "Copy-Item -Force '$(AFFECT_ORT)/lib/onnxruntime.dll' '$(dir $@)'"

$(AFFECT_CLIENT_TEST): tests/affect_client_test.c src/affect_client.c src/affect_client.h \
		src/affect_protocol.h src/log.c $(AFFECT_WORKER) $(SDL3_ROOT)/lib/x64/SDL3.dll
	$(make-dir)
	$(CC) $(CPPFLAGS) -DEIDOLON_TEST_AFFECT_WORKER=\"$(abspath $(AFFECT_WORKER))\" \
		$(TEST_CFLAGS) tests/affect_client_test.c src/affect_client.c src/log.c \
		$(LDFLAGS) $(LDLIBS) -o $@
	@powershell.exe -NoProfile -Command "Copy-Item -Force '$(SDL3_ROOT)/lib/x64/SDL3.dll' '$(dir $@)'"

$(AFFECT_BENCHMARK): tools/affect_benchmark.c src/affect.c src/affect.h src/affect_protocol.h \
		src/state.h $(AFFECT_WORKER)
	$(make-dir)
	$(CC) -Isrc $(TEST_CFLAGS) tools/affect_benchmark.c src/affect.c -lpsapi -o $@

affect-check: $(AFFECT_WORKER) $(AFFECT_CLIENT_TEST)
	"$(AFFECT_WORKER)" --text "I love this. You did a wonderful job."
	"$(AFFECT_CLIENT_TEST)"

affect-benchmark: $(AFFECT_BENCHMARK)
	"$(AFFECT_BENCHMARK)" "$(AFFECT_WORKER)"
else
affect:
	@echo "The native affect worker is currently prepared for Windows only."
	@false

affect-check: affect

affect-benchmark: affect
endif

character-sprites:
	$(PYTHON) tools/download_character_sprites.py

character-sprites-download:
	$(PYTHON) tools/download_character_sprites.py --download

character-sprites-check:
	$(PYTHON) -m unittest discover -s tests -p download_character_sprites_test.py

force-output:

$(TARGET): $(MODE_TARGET) force-output
	$(make-dir)
	$(copy-output)
	$(copy-runtime)

$(MODE_TARGET): $(OBJECTS) $(IMGUI_OBJECTS) $(PLATFORM_CPP_OBJECTS) | shaders
	$(make-dir)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(IMGUI_OBJECTS) $(PLATFORM_CPP_OBJECTS) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: %.c
	$(make-dir)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

IMGUI_SMOKE := $(BUILD_ROOT)/tests/$(MODE)/imgui_smoke$(EXE)
IMGUI_SMOKE_OBJECT := $(OBJ_DIR)/tests/imgui_smoke.o

$(OBJ_DIR)/%.o: %.cpp
	$(make-dir)
	$(CXX) $(IMGUI_CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(IMGUI_SMOKE_OBJECT): tests/imgui_smoke.c
	$(make-dir)
	$(CC) $(IMGUI_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(IMGUI_SMOKE): $(IMGUI_SMOKE_OBJECT) $(IMGUI_OBJECTS)
	$(make-dir)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
	$(copy-runtime)

imgui-smoke: $(IMGUI_SMOKE)
	"$(IMGUI_SMOKE)"

ifeq ($(OS),Windows_NT)
BGFX_DIR := lib/bgfx
BX_DIR := lib/bx
BIMG_DIR := lib/bimg
BGFX_BUILD_DIR := $(BUILD_ROOT)/bgfx/$(MODE)
BGFX_OBJ_DIR := $(BGFX_BUILD_DIR)/obj
BGFX_LIB := $(BGFX_BUILD_DIR)/libeidolon-bgfx.a
BGFX_SMOKE := $(BGFX_BUILD_DIR)/bgfx_smoke.exe
BGFX_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/bgfx_smoke.o
BGFX_INTEROP_SMOKE := $(BGFX_BUILD_DIR)/bgfx_interop_smoke.exe
BGFX_INTEROP_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/bgfx_interop_smoke.o
BGFX_DCOMP_SMOKE := $(BGFX_BUILD_DIR)/bgfx_dcomp_smoke.exe
BGFX_DCOMP_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/bgfx_dcomp_smoke.o
D3D11_DCOMP_SMOKE := $(BGFX_BUILD_DIR)/d3d11_dcomp_smoke.exe
D3D11_DCOMP_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/d3d11_dcomp_smoke.o
SDL_GPU_DCOMP_SMOKE := $(BGFX_BUILD_DIR)/sdl_gpu_dcomp_smoke.exe
SDL_GPU_DCOMP_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/sdl_gpu_dcomp_smoke.o
SDL_RENDERER_DCOMP_SMOKE := $(BGFX_BUILD_DIR)/sdl_renderer_dcomp_smoke.exe
SDL_RENDERER_DCOMP_SMOKE_OBJECT := $(BGFX_OBJ_DIR)/sdl_renderer_dcomp_smoke.o
BGFX_OBJECTS := \
	$(BGFX_OBJ_DIR)/bx.o \
	$(BGFX_OBJ_DIR)/bimg.o \
	$(BGFX_OBJ_DIR)/bgfx.o
BGFX_DEPS := $(BGFX_OBJECTS:.o=.d) $(BGFX_SMOKE_OBJECT:.o=.d) \
	$(BGFX_INTEROP_SMOKE_OBJECT:.o=.d) $(BGFX_DCOMP_SMOKE_OBJECT:.o=.d) \
	$(D3D11_DCOMP_SMOKE_OBJECT:.o=.d) $(SDL_GPU_DCOMP_SMOKE_OBJECT:.o=.d) \
	$(SDL_RENDERER_DCOMP_SMOKE_OBJECT:.o=.d)
BGFX_CPPFLAGS := \
	-I$(BGFX_DIR)/include \
	-I$(BGFX_DIR)/3rdparty \
	-I$(BGFX_DIR)/3rdparty/directx-headers/include/directx \
	-I$(BX_DIR)/include \
	-I$(BX_DIR)/include/compat/msvc \
	-I$(BX_DIR)/3rdparty \
	-I$(BIMG_DIR)/include \
	-DBGFX_CONFIG_RENDERER_DIRECT3D11=1 \
	-DBGFX_CONFIG_VIDEO=0 \
	-DBIMG_CONFIG_DECODE_ASTC=0 \
	-D_CRT_SECURE_NO_WARNINGS
BGFX_CXXFLAGS := -std=c++20 -MMD -MP \
	-msse4.2 -Wno-microsoft-enum-value -Wno-microsoft-const-init

ifeq ($(MODE),release)
BGFX_CPPFLAGS += -DBX_CONFIG_DEBUG=0
BGFX_CXXFLAGS += -O2 -DNDEBUG
else
BGFX_CPPFLAGS += -DBX_CONFIG_DEBUG=1
BGFX_CXXFLAGS += -O0 -g
endif

$(BGFX_OBJ_DIR)/bx.o: $(BX_DIR)/src/amalgamated.cpp
	$(make-dir)
	$(CXX) $(BGFX_CPPFLAGS) $(BGFX_CXXFLAGS) -c $< -o $@

$(BGFX_OBJ_DIR)/bimg.o: $(BIMG_DIR)/src/image.cpp
	$(make-dir)
	$(CXX) $(BGFX_CPPFLAGS) $(BGFX_CXXFLAGS) -c $< -o $@

$(BGFX_OBJ_DIR)/bgfx.o: $(BGFX_DIR)/src/amalgamated.cpp
	$(make-dir)
	$(CXX) $(BGFX_CPPFLAGS) $(BGFX_CXXFLAGS) -c $< -o $@

$(BGFX_LIB): $(BGFX_OBJECTS)
	$(make-dir)
	$(AR) rcs $@ $^

$(BGFX_SMOKE_OBJECT): tests/bgfx_smoke.c
	$(make-dir)
	$(CC) -I"$(SDL3_ROOT)/include" -I$(BGFX_DIR)/include -I$(BX_DIR)/include \
		$(CFLAGS) -c $< -o $@

$(BGFX_INTEROP_SMOKE_OBJECT): tests/bgfx_interop_smoke.c
	$(make-dir)
	$(CC) -I"$(SDL3_ROOT)/include" -I$(BGFX_DIR)/include -I$(BX_DIR)/include \
		$(CFLAGS) -c $< -o $@

$(BGFX_DCOMP_SMOKE_OBJECT): tests/bgfx_dcomp_smoke.cpp
	$(make-dir)
	$(CXX) -I$(BGFX_DIR)/include -I$(BX_DIR)/include $(BGFX_CXXFLAGS) \
		-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
		-Wno-language-extension-token -c $< -o $@

$(D3D11_DCOMP_SMOKE_OBJECT): tests/bgfx_dcomp_smoke.cpp
	$(make-dir)
	$(CXX) -DEIDOLON_DCOMP_NATIVE_D3D11=1 $(CXXFLAGS) \
		-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
		-Wno-language-extension-token -c $< -o $@

$(SDL_GPU_DCOMP_SMOKE_OBJECT): tests/bgfx_dcomp_smoke.cpp
	$(make-dir)
	$(CXX) -DEIDOLON_DCOMP_SDL_GPU=1 -I"$(SDL3_ROOT)/include" $(CXXFLAGS) \
		-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
		-Wno-language-extension-token -c $< -o $@

$(SDL_RENDERER_DCOMP_SMOKE_OBJECT): tests/bgfx_dcomp_smoke.cpp
	$(make-dir)
	$(CXX) -DEIDOLON_DCOMP_SDL_RENDERER=1 -I"$(SDL3_ROOT)/include" $(CXXFLAGS) \
		-Wall -Wextra -Wpedantic -Wshadow -Wconversion \
		-Wno-language-extension-token -c $< -o $@

$(BGFX_SMOKE): $(BGFX_SMOKE_OBJECT) $(BGFX_LIB)
	$(make-dir)
	$(CXX) $(LDFLAGS) $^ -lSDL3 -ld3d11 -ldxgi -ldxguid -ld3dcompiler \
		-lgdi32 -lpsapi -luser32 -lole32 -o $@
	$(copy-runtime)

bgfx-smoke: $(BGFX_SMOKE)
	"$(BGFX_SMOKE)"

$(BGFX_INTEROP_SMOKE): $(BGFX_INTEROP_SMOKE_OBJECT) $(BGFX_LIB)
	$(make-dir)
	$(CXX) $(LDFLAGS) $^ -lSDL3 -ld3d11 -ldxgi -ldxguid -ld3dcompiler \
		-lgdi32 -lpsapi -luser32 -lole32 -o $@
	$(copy-runtime)

bgfx-interop-smoke: $(BGFX_INTEROP_SMOKE)
	"$(BGFX_INTEROP_SMOKE)"

$(BGFX_DCOMP_SMOKE): $(BGFX_DCOMP_SMOKE_OBJECT) $(BGFX_LIB)
	$(make-dir)
	$(CXX) $^ -ldcomp -ld3d11 -ldxgi -ldxguid -ld3dcompiler \
		-lgdi32 -lpsapi -luser32 -lole32 -o $@

bgfx-dcomp-smoke: $(BGFX_DCOMP_SMOKE)
	"$(BGFX_DCOMP_SMOKE)" $(if $(filter 1,$(SHOW)),--show,)

$(D3D11_DCOMP_SMOKE): $(D3D11_DCOMP_SMOKE_OBJECT)
	$(make-dir)
	$(CXX) $^ -ldcomp -ld3d11 -ldxgi -ldxguid -lgdi32 -lpsapi -luser32 -lole32 -o $@

d3d11-dcomp-smoke: $(D3D11_DCOMP_SMOKE)
	"$(D3D11_DCOMP_SMOKE)" $(if $(filter 1,$(SHOW)),--show,)

$(SDL_GPU_DCOMP_SMOKE): $(SDL_GPU_DCOMP_SMOKE_OBJECT)
	$(make-dir)
	$(CXX) -L"$(SDL3_ROOT)/lib/x64" $^ -lSDL3 -ldcomp -ld3d11 -ldxgi -ldxguid \
		-lgdi32 -lpsapi -luser32 -lole32 -o $@
	$(copy-runtime)

sdl-gpu-dcomp-smoke: $(SDL_GPU_DCOMP_SMOKE)
	"$(SDL_GPU_DCOMP_SMOKE)" $(if $(filter 1,$(SHOW)),--show,)

$(SDL_RENDERER_DCOMP_SMOKE): $(SDL_RENDERER_DCOMP_SMOKE_OBJECT)
	$(make-dir)
	$(CXX) -L"$(SDL3_ROOT)/lib/x64" $^ -lSDL3 -ldcomp -ld3d11 -ldxgi -ldxguid \
		-lgdi32 -lpsapi -luser32 -lole32 -o $@
	$(copy-runtime)

sdl-renderer-dcomp-smoke: $(SDL_RENDERER_DCOMP_SMOKE)
	"$(SDL_RENDERER_DCOMP_SMOKE)" $(if $(filter 1,$(SHOW)),--show,)

graphics-backend-benchmark: $(D3D11_DCOMP_SMOKE) $(SDL_RENDERER_DCOMP_SMOKE) \
	$(BGFX_DCOMP_SMOKE) $(SDL_GPU_DCOMP_SMOKE)
	"$(D3D11_DCOMP_SMOKE)" --benchmark
	"$(SDL_RENDERER_DCOMP_SMOKE)" --benchmark
	"$(BGFX_DCOMP_SMOKE)" --benchmark
	"$(SDL_GPU_DCOMP_SMOKE)" --benchmark
else
bgfx-smoke:
	@echo "The bgfx D3D11 smoke test is currently prepared for Windows only."
	@false

bgfx-interop-smoke:
	@echo "The bgfx D3D11 interop smoke test is currently prepared for Windows only."
	@false

bgfx-dcomp-smoke:
	@echo "The bgfx DirectComposition smoke test is currently prepared for Windows only."
	@false

d3d11-dcomp-smoke:
	@echo "The D3D11 DirectComposition smoke test is currently prepared for Windows only."
	@false

sdl-gpu-dcomp-smoke:
	@echo "The SDL_GPU DirectComposition smoke test is currently prepared for Windows only."
	@false

sdl-renderer-dcomp-smoke:
	@echo "The SDL_Renderer DirectComposition smoke test is currently prepared for Windows only."
	@false

graphics-backend-benchmark:
	@echo "The graphics backend benchmark is currently prepared for Windows only."
	@false
endif

shaders: $(SHADER_OUTPUTS)

$(DXBC_VERTEX_SHADER): $(SHADER_SOURCE_DIR)/model.vert.hlsl tools/compile_d3d11_shader.ps1
	$(make-dir)
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(CURDIR)/tools/compile_d3d11_shader.ps1" \
		-SourcePath "$(CURDIR)/$<" -Target vs_5_0 -OutputPath "$(CURDIR)/$@" -Mode $(MODE) \
		-FxcPath "$(FXC)"

$(DXBC_FRAGMENT_SHADER): $(SHADER_SOURCE_DIR)/model.frag.hlsl tools/compile_d3d11_shader.ps1
	$(make-dir)
	@powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(CURDIR)/tools/compile_d3d11_shader.ps1" \
		-SourcePath "$(CURDIR)/$<" -Target ps_5_0 -OutputPath "$(CURDIR)/$@" -Mode $(MODE) \
		-FxcPath "$(FXC)"

$(SHADER_BUILD_DIR)/SPIRV/%.spv: $(SHADER_SOURCE_DIR)/%.hlsl
	$(make-dir)
	"$(SHADERCROSS)" "$<" -o "$@"

$(SHADER_BUILD_DIR)/DXIL/%.dxil: $(SHADER_SOURCE_DIR)/%.hlsl
	$(make-dir)
	"$(SHADERCROSS)" "$<" -o "$@"

TEST_DIR := $(BUILD_ROOT)/tests/$(MODE)
ANIMATION_TEST := $(TEST_DIR)/animation_test$(EXE)
STATE_TEST := $(TEST_DIR)/state_test$(EXE)
DIALOGUE_TEST := $(TEST_DIR)/dialogue_test$(EXE)
DIALOGUE_ART_TEST := $(TEST_DIR)/dialogue_art_test$(EXE)
DELIVERY_TEST := $(TEST_DIR)/delivery_test$(EXE)
HOOK_OUTPUT_TEST := $(TEST_DIR)/hook_output_test$(EXE)
MOTION_TEST := $(TEST_DIR)/motion_test$(EXE)
MOTION_CONFIG_TEST := $(TEST_DIR)/motion_config_test$(EXE)
POSE_TEST := $(TEST_DIR)/pose_test$(EXE)
IK_TEST := $(TEST_DIR)/ik_test$(EXE)
HUMANOID_TEST := $(TEST_DIR)/humanoid_test$(EXE)
POSE_SOLVER_TEST := $(TEST_DIR)/pose_solver_test$(EXE)
PORTRAIT_TEST := $(TEST_DIR)/portrait_test$(EXE)
PORTRAIT_MOTION_TEST := $(TEST_DIR)/portrait_motion_test$(EXE)
AFFECT_TEST := $(TEST_DIR)/affect_test$(EXE)
AFFECT_TOKENIZER_TEST := $(TEST_DIR)/affect_tokenizer_test$(EXE)
EXPRESSION_DIRECTOR_TEST := $(TEST_DIR)/expression_director_test$(EXE)
FRAME_CLOCK_TEST := $(TEST_DIR)/frame_clock_test$(EXE)
PRESENTATION_TEST := $(TEST_DIR)/presentation_test$(EXE)
PRESENTATION_EVENT_QUEUE_TEST := $(TEST_DIR)/presentation_event_queue_test$(EXE)
SCENE_TEST := $(TEST_DIR)/scene_test$(EXE)
BUBBLE_LAYOUT_TEST := $(TEST_DIR)/bubble_layout_test$(EXE)
SESSION_REGISTRY_TEST := $(TEST_DIR)/session_registry_test$(EXE)
USER_SETTINGS_TEST := $(TEST_DIR)/user_settings_test$(EXE)
CONVERSATION_TEST := $(TEST_DIR)/conversation_test$(EXE)
LIVE_SOURCE_TEST := $(TEST_DIR)/live_source_test$(EXE)
RELAY_CORE_TEST := $(TEST_DIR)/relay_core_test$(EXE)
CODEX_RELAY_TEST := $(TEST_DIR)/codex_relay_test$(EXE)

ifeq ($(OS),Windows_NT)
TEST_RUNTIME := $(TEST_DIR)/SDL3.dll

$(TEST_RUNTIME): $(SDL3_ROOT)/lib/x64/SDL3.dll
	$(make-dir)
	@powershell.exe -NoProfile -Command "Copy-Item -Force '$(SDL3_ROOT)/lib/x64/SDL3.dll' '$@'"
else
TEST_RUNTIME :=
endif

$(ANIMATION_TEST): tests/animation_test.c src/animation.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(STATE_TEST): tests/state_test.c src/state.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(DIALOGUE_TEST): tests/dialogue_test.c src/dialogue.c src/delivery.c \
		src/expression_director.c src/affect.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(DIALOGUE_ART_TEST): tests/dialogue_art_test.c src/dialogue_art.c src/text_renderer.c \
		src/dialogue.c src/delivery.c src/expression_director.c src/affect.c src/log.c \
		| $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
	$(copy-runtime)

$(DELIVERY_TEST): tests/delivery_test.c src/delivery.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(HOOK_OUTPUT_TEST): tests/hook_output_test.c src/hook_output.c src/log.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) -DEIDOLON_TEST_TRANSCRIPT=\"$(abspath tests/fixtures/transcript.jsonl)\" \
		-DEIDOLON_TEST_SUBAGENT_TRANSCRIPT=\"$(abspath tests/fixtures/subagent.jsonl)\" \
		$(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(MOTION_TEST): tests/motion_test.c src/motion.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(MOTION_CONFIG_TEST): tests/motion_config_test.c src/motion_config.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(POSE_TEST): tests/pose_test.c src/pose.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(IK_TEST): tests/ik_test.c src/ik.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(HUMANOID_TEST): tests/humanoid_test.c src/humanoid.c src/motion.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(POSE_SOLVER_TEST): tests/pose_solver_test.c src/pose_solver.c src/humanoid.c src/motion.c src/ik.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(PORTRAIT_TEST): tests/portrait_test.c src/portrait.c src/portrait_motion.c src/affect.c \
		src/state.c src/log.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(PORTRAIT_MOTION_TEST): tests/portrait_motion_test.c src/portrait_motion.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(AFFECT_TEST): tests/affect_test.c src/affect.c src/state.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(AFFECT_TOKENIZER_TEST): tests/affect_tokenizer_test.c src/affect_tokenizer.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) \
		-DEIDOLON_TEST_AFFECT_VOCAB=\"$(abspath tests/fixtures/affect_vocab.json)\" \
		-DEIDOLON_TEST_AFFECT_MERGES=\"$(abspath tests/fixtures/affect_merges.txt)\" \
		$(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(EXPRESSION_DIRECTOR_TEST): tests/expression_director_test.c src/expression_director.c \
		src/affect.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(FRAME_CLOCK_TEST): tests/frame_clock_test.c src/frame_clock.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(PRESENTATION_TEST): tests/presentation_test.c src/presentation.c src/scene.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(PRESENTATION_EVENT_QUEUE_TEST): tests/presentation_event_queue_test.c \
		src/presentation_event_queue.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(SCENE_TEST): tests/scene_test.c src/scene.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUBBLE_LAYOUT_TEST): tests/bubble_layout_test.c src/bubble_layout.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(SESSION_REGISTRY_TEST): tests/session_registry_test.c src/session_registry.c src/dialogue.c \
		src/delivery.c src/expression_director.c src/affect.c src/log.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(USER_SETTINGS_TEST): tests/user_settings_test.c src/user_settings.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) \
		-DEIDOLON_TEST_SETTINGS_PATH=\"$(abspath $(TEST_DIR)/user-settings-roundtrip.cfg)\" \
		$(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(CONVERSATION_TEST): tests/conversation_test.c src/conversation.c src/json_scan.c \
		src/provider_config.c src/providers/codex_stream.c src/providers/opencode_stream.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(LIVE_SOURCE_TEST): tests/live_source_test.c src/conversation.c src/json_scan.c src/log.c \
		src/providers/codex_stream.c src/providers/live_source.c \
		src/providers/opencode_stream.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(RELAY_CORE_TEST): tests/relay_core_test.c src/relay_core.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(CODEX_RELAY_TEST): tests/codex_relay_test.c src/conversation.c src/json_scan.c src/log.c \
		src/relay_core.c src/providers/codex_relay.c src/providers/codex_stream.c \
		src/providers/live_source.c src/providers/opencode_stream.c | $(TEST_RUNTIME)
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

PROVIDER ?= codex
PROVIDER_URL ?= ws://127.0.0.1:4500
provider-live-test: $(LIVE_SOURCE_TEST)
	"$(LIVE_SOURCE_TEST)" "$(PROVIDER)" "$(PROVIDER_URL)"

codex-relay-test: $(CODEX_RELAY_TEST)
	"$(CODEX_RELAY_TEST)"

check: $(ANIMATION_TEST) $(STATE_TEST) $(DIALOGUE_TEST) $(DIALOGUE_ART_TEST) $(DELIVERY_TEST) $(HOOK_OUTPUT_TEST) $(MOTION_TEST) \
	$(MOTION_CONFIG_TEST) $(POSE_TEST) $(IK_TEST) $(HUMANOID_TEST) $(POSE_SOLVER_TEST) \
	$(PORTRAIT_TEST) $(PORTRAIT_MOTION_TEST) $(AFFECT_TEST) $(AFFECT_TOKENIZER_TEST) $(BUBBLE_LAYOUT_TEST) \
	$(EXPRESSION_DIRECTOR_TEST) $(FRAME_CLOCK_TEST) $(PRESENTATION_TEST) $(PRESENTATION_EVENT_QUEUE_TEST) \
	$(SCENE_TEST) $(SESSION_REGISTRY_TEST) $(USER_SETTINGS_TEST) \
	$(CONVERSATION_TEST) $(RELAY_CORE_TEST)
	$(ANIMATION_TEST)
	$(STATE_TEST)
	$(DIALOGUE_TEST)
	$(DIALOGUE_ART_TEST)
	$(DELIVERY_TEST)
	$(HOOK_OUTPUT_TEST)
	$(MOTION_TEST)
	$(MOTION_CONFIG_TEST)
	$(POSE_TEST)
	$(IK_TEST)
	$(HUMANOID_TEST)
	$(POSE_SOLVER_TEST)
	$(PORTRAIT_TEST)
	$(PORTRAIT_MOTION_TEST)
	$(AFFECT_TEST)
	$(AFFECT_TOKENIZER_TEST)
	$(EXPRESSION_DIRECTOR_TEST)
	$(FRAME_CLOCK_TEST)
	$(PRESENTATION_TEST)
	$(PRESENTATION_EVENT_QUEUE_TEST)
	$(SCENE_TEST)
	$(BUBBLE_LAYOUT_TEST)
	$(SESSION_REGISTRY_TEST)
	$(USER_SETTINGS_TEST)
	$(CONVERSATION_TEST)
	$(RELAY_CORE_TEST)

MODEL_SOURCE_DIR := $(CURDIR)/assets/blue-archive-rio-battle-full-rip-rig/source/Rio Battle
MODEL_AUDIT := $(CURDIR)/build/model-audit/index.json
MODEL_MATERIAL_AUDIT := $(CURDIR)/build/model-audit/ch0331-materials.json
MODEL_MASTER := $(MODEL_SOURCE_DIR)/CH0331_Mesh.fbx
MODEL_HALO := $(MODEL_SOURCE_DIR)/Animator/CH0331_Halo/CH0331_Halo.fbx
MODEL_GLB := $(RUNTIME_MODEL)
MODEL_PREVIEW := $(CURDIR)/build/model-audit/ch0331-preview.png
MODEL_GLB_PREVIEW := $(CURDIR)/build/model-audit/rio-glb-preview.png
MOUTH_SOURCE := $(CURDIR)/build/model-audit/mouth-source-alpha.png
MOUTH_TEXTURE := $(MODEL_SOURCE_DIR)/Texture2D/Character_Mouth_Black.png
MOUTH_CANDIDATES := $(CURDIR)/build/model-audit/mouth-candidates
MOUTH_MANIFEST := $(MOUTH_CANDIDATES)/manifest.json
MOUTH_RENDERS := $(MOUTH_CANDIDATES)/renders
MOUTH_SHEET := $(CURDIR)/build/model-audit/mouth-position-sheet.png
MOUTH_CALIBRATION := $(CURDIR)/build/model-audit/mouth-calibration.json
PICK ?= C3

model-audit:
	$(BLENDER) --background --factory-startup --python tools/blender_audit.py -- \
		--input "$(MODEL_SOURCE_DIR)" --output "$(MODEL_AUDIT)"

model-material-audit:
	$(BLENDER) --background --factory-startup --python tools/blender_material_audit.py -- \
		--input "$(MODEL_MASTER)" --output "$(MODEL_MATERIAL_AUDIT)"

model-preview:
	$(BLENDER) --background --factory-startup --python tools/blender_preview.py -- \
		--input "$(MODEL_MASTER)" --output "$(MODEL_PREVIEW)"

model-export:
	$(BLENDER) --background --factory-startup --python tools/blender_export.py -- \
		--master "$(MODEL_MASTER)" --halo "$(MODEL_HALO)" --output "$(MODEL_GLB)" \
		--mouth-calibration "$(MOUTH_CALIBRATION)"

model-mouth:
	$(PYTHON) tools/build_mouth_texture.py --input "$(MOUTH_SOURCE)" --output "$(MOUTH_TEXTURE)"

model-mouth-sheet:
	$(PYTHON) tools/build_mouth_candidates.py --input "$(MOUTH_SOURCE)" --output "$(MOUTH_CANDIDATES)"
	$(BLENDER) --background --factory-startup --python tools/blender_mouth_candidates.py -- \
		--master "$(MODEL_MASTER)" --manifest "$(MOUTH_MANIFEST)" --output "$(MOUTH_RENDERS)"
	$(PYTHON) tools/compose_mouth_sheet.py --manifest "$(MOUTH_MANIFEST)" \
		--renders "$(MOUTH_RENDERS)" --output "$(MOUTH_SHEET)"

model-mouth-pick:
	$(PYTHON) tools/pick_mouth_candidate.py --manifest "$(MOUTH_MANIFEST)" \
		--pick "$(PICK)" --output "$(MOUTH_TEXTURE)"

model-mouth-calibrate:
	$(BLENDER) --factory-startup --python tools/blender_mouth_calibrate_live.py -- \
		--master "$(MODEL_MASTER)" --texture "$(MOUTH_TEXTURE)" \
		--calibration "$(MOUTH_CALIBRATION)"

model-preview-glb: model-export
	$(BLENDER) --background --factory-startup --python tools/blender_preview.py -- \
		--input "$(MODEL_GLB)" --output "$(MODEL_GLB_PREVIEW)"

clean:
	$(remove-build)

log:
	powershell.exe -NoProfile -Command "$$path = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Eidolon\eidolon.log'; Get-Content -LiteralPath $$path -Tail 120 -Wait"

help:
	@echo "make                 build debug Eidolon with clang"
	@echo "make MODE=release    build optimized Eidolon with clang"
	@echo "make check           build and run unit tests"
	@echo "make editor-config   regenerate compile_commands.json for clangd/VS Code"
	@echo "make imgui-smoke     build and run Dear ImGui through its generated C API"
	@echo "make bgfx-smoke      build pinned bgfx submodules and run the hidden C99/D3D11 probe"
	@echo "make bgfx-interop-smoke  verify bgfx renders into an Eidolon-owned D3D11 texture"
	@echo "make bgfx-dcomp-smoke  verify two bgfx layers through DirectComposition"
	@echo "make bgfx-dcomp-smoke SHOW=1  run the draggable owner-controlled visual probe"
	@echo "make d3d11-dcomp-smoke  run the equivalent native D3D11 baseline"
	@echo "make sdl-gpu-dcomp-smoke  measure SDL_GPU's required CPU bridge"
	@echo "make sdl-renderer-dcomp-smoke  run the current SDL/D3D11 baseline"
	@echo "make graphics-backend-benchmark  compare equivalent Eidolon-sized layers"
	@echo "make provider-live-test PROVIDER=codex PROVIDER_URL=ws://...  probe a running provider"
	@echo "make codex-relay-test  launch a hidden app-server and verify the in-path relay"
	@echo "make text-setup      download verified SDL_ttf runtime/development files"
	@echo "make affect-setup    download verified optional GoEmotions runtime/model"
	@echo "make affect          build the optional native GoEmotions worker"
	@echo "make affect-check    run one visible native inference smoke test"
	@echo "make affect-benchmark  profile the persistent worker across six dialogue beats"
	@echo "make character-sprites  inspect the complete Blue Archive portrait download"
	@echo "make character-sprites-download  download all grouped character portraits"
	@echo "make character-sprites-check  test wiki filename grouping without network access"
	@echo "make log             tail the Eidolon debug log"
	@echo "make shaders         bake SDL_GPU SPIR-V and DXIL shaders"
	@echo "make model-audit     inspect every Rio FBX with Blender"
	@echo "make model-material-audit  inspect imported FBX shader graphs"
	@echo "make model-export    repair and export the runtime Rio GLB"
	@echo "make model-mouth     fit the prepared alpha source to Rio's mouth UVs"
	@echo "make model-mouth-sheet  render a labeled 5x5 mouth-position grid"
	@echo "make model-mouth-pick PICK=C3  promote one reviewed candidate"
	@echo "make model-mouth-calibrate  open the live unlit placement tool"
	@echo "make model-preview   render the canonical Rio rig"
	@echo "make model-preview-glb  export and render the runtime GLB"
	@echo "make clean           remove this platform's build directory"

-include $(DEPS) $(IMGUI_DEPS) $(BGFX_DEPS)
