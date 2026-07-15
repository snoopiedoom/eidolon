CC := clang
MODE ?= debug
BLENDER ?= blender
PYTHON ?= python

COMMON_SOURCES := \
	src/animation.c \
	src/app.c \
	src/dialogue.c \
	src/draw.c \
	src/hook_output.c \
	src/log.c \
	src/main.c \
	src/model.c \
	src/session_watch.c \
	src/state.c

ifeq ($(OS),Windows_NT)
PLATFORM := windows
EXE := .exe
SDL3_ROOT ?= C:/dev/SDL3
SHADERCROSS ?= $(CURDIR)/.cache/shadercross/bin/shadercross.exe
PLATFORM_SOURCES := src/platform/windows_ipc.c src/platform/windows_overlay.c \
	src/platform/windows_session_files.c
CPPFLAGS += -Isrc -I"$(SDL3_ROOT)/include"
LDFLAGS += -L"$(SDL3_ROOT)/lib/x64"
LDLIBS += -lSDL3 -ldwmapi -luser32 -lgdi32
define make-dir
@powershell.exe -NoProfile -Command "New-Item -ItemType Directory -Force '$(subst /,\,$(dir $@))' | Out-Null"
endef
define copy-runtime
@powershell.exe -NoProfile -Command "if (Test-Path '$(SDL3_ROOT)/lib/x64/SDL3.dll') { Copy-Item -Force '$(SDL3_ROOT)/lib/x64/SDL3.dll' '$(dir $@)' }"
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
CPPFLAGS += -Isrc $(shell $(PKG_CONFIG) --cflags sdl3)
LDLIBS += $(shell $(PKG_CONFIG) --libs sdl3) -lm
define make-dir
@mkdir -p "$(dir $@)"
endef
define copy-runtime
endef
define remove-build
@rm -rf "$(BUILD_ROOT)"
endef
endif

BUILD_ROOT := build/$(PLATFORM)
OBJ_DIR := $(BUILD_ROOT)/obj
TARGET := $(BUILD_ROOT)/eidolon$(EXE)
SHADER_SOURCE_DIR := shaders
SHADER_BUILD_DIR := $(BUILD_ROOT)/shaders
SHADER_NAMES := model.vert model.frag
SPIRV_SHADERS := $(addprefix $(SHADER_BUILD_DIR)/SPIRV/,$(addsuffix .spv,$(SHADER_NAMES)))
DXIL_SHADERS := $(addprefix $(SHADER_BUILD_DIR)/DXIL/,$(addsuffix .dxil,$(SHADER_NAMES)))
SHADER_OUTPUTS := $(SPIRV_SHADERS) $(DXIL_SHADERS)
RUNTIME_MODEL := $(CURDIR)/assets/model/rio.glb
SOURCES := $(COMMON_SOURCES) $(PLATFORM_SOURCES)
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

CPPFLAGS += -Ilib/cgltf -DEIDOLON_ASSET_DIR=\"$(abspath assets)\" \
	-DEIDOLON_MODEL_PATH=\"$(RUNTIME_MODEL)\" \
	-DEIDOLON_SHADER_DIR=\"$(abspath $(SHADER_BUILD_DIR))\"
CFLAGS += -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -MMD -MP

ifeq ($(MODE),release)
CFLAGS += -O2 -DNDEBUG
else ifeq ($(MODE),debug)
CFLAGS += -O0 -g
else
$(error MODE must be debug or release)
endif

TEST_CFLAGS := $(filter-out -MMD -MP,$(CFLAGS))

.PHONY: all clean check shaders model-audit model-material-audit model-export model-preview \
	model-preview-glb model-mouth model-mouth-sheet model-mouth-pick model-mouth-calibrate help

all: $(TARGET)

$(TARGET): $(OBJECTS) | shaders
	$(make-dir)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@
	$(copy-runtime)

$(OBJ_DIR)/%.o: %.c
	$(make-dir)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

shaders: $(SHADER_OUTPUTS)

$(SHADER_BUILD_DIR)/SPIRV/%.spv: $(SHADER_SOURCE_DIR)/%.hlsl
	$(make-dir)
	"$(SHADERCROSS)" "$<" -o "$@"

$(SHADER_BUILD_DIR)/DXIL/%.dxil: $(SHADER_SOURCE_DIR)/%.hlsl
	$(make-dir)
	"$(SHADERCROSS)" "$<" -o "$@"

ANIMATION_TEST := $(BUILD_ROOT)/tests/animation_test$(EXE)
STATE_TEST := $(BUILD_ROOT)/tests/state_test$(EXE)
DIALOGUE_TEST := $(BUILD_ROOT)/tests/dialogue_test$(EXE)
HOOK_OUTPUT_TEST := $(BUILD_ROOT)/tests/hook_output_test$(EXE)

$(ANIMATION_TEST): tests/animation_test.c src/animation.c
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(STATE_TEST): tests/state_test.c src/state.c
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(DIALOGUE_TEST): tests/dialogue_test.c src/dialogue.c
	$(make-dir)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(HOOK_OUTPUT_TEST): tests/hook_output_test.c src/hook_output.c src/log.c
	$(make-dir)
	$(CC) $(CPPFLAGS) -DEIDOLON_TEST_TRANSCRIPT=\"$(abspath tests/fixtures/transcript.jsonl)\" \
		$(TEST_CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

check: $(ANIMATION_TEST) $(STATE_TEST) $(DIALOGUE_TEST) $(HOOK_OUTPUT_TEST)
	$(ANIMATION_TEST)
	$(STATE_TEST)
	$(DIALOGUE_TEST)
	$(HOOK_OUTPUT_TEST)

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

help:
	@echo "make                 build debug Eidolon with clang"
	@echo "make MODE=release    build optimized Eidolon with clang"
	@echo "make check           build and run unit tests"
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

-include $(DEPS)
