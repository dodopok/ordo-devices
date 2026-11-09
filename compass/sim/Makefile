# Draws every screen of the compass on a computer, with the same LVGL, the
# same fonts and the same ui/ code the firmware runs.
#
#   pio pkg install -e amoled175c     (once: fetches LVGL and ArduinoJson)
#   make -C sim shots                 (PNGs in sim/out/)
#
# Needs gcc, g++ and python3 with Pillow.

LIBDEPS ?= ../.pio/libdeps/amoled175c
LVGL    := $(LIBDEPS)/lvgl
JSON    := $(LIBDEPS)/ArduinoJson/src
BUILD   := build
OUT     := out

DEFS  := -DLV_CONF_INCLUDE_SIMPLE -DORDO_BOARD_AMOLED_175C -DORDO_FIRMWARE_VERSION=\"sim\"
INCS  := -Ishim -I../include -I../src -I$(LVGL) -I$(JSON)
SAN      ?=
CFLAGS   := -O1 -w $(SAN) $(DEFS) $(INCS)
CXXFLAGS := -O1 -std=gnu++17 -Wall -Wno-unused-function $(SAN) $(DEFS) $(INCS)

LVGL_SRC := $(shell find $(LVGL)/src -name '*.c')
FONT_SRC := $(wildcard ../src/ui/fonts/*.c)
APP_SRC  := $(wildcard ../src/ui/*.cpp) ../src/model/today.cpp sim_main.cpp

LVGL_OBJ := $(patsubst $(LVGL)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRC))
FONT_OBJ := $(patsubst ../src/ui/fonts/%.c,$(BUILD)/fonts/%.o,$(FONT_SRC))
APP_OBJ  := $(patsubst %.cpp,$(BUILD)/app/%.o,$(notdir $(APP_SRC)))

vpath %.cpp ../src/ui ../src/model .

.PHONY: shots check clean
shots: $(BUILD)/sim
	@mkdir -p $(OUT)
	@./$(BUILD)/sim $(OUT) sample_today.json 2>/dev/null
	@python3 to_png.py $(OUT)

$(BUILD)/sim: $(LVGL_OBJ) $(FONT_OBJ) $(APP_OBJ)
	@echo link $@
	@$(CXX) $(SAN) -o $@ $^ -lm

$(BUILD)/lvgl/%.o: $(LVGL)/src/%.c ../include/lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fonts/%.o: ../src/ui/fonts/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/app/%.o: %.cpp $(wildcard ../src/ui/*.h) ../src/model/today.h
	@mkdir -p $(dir $@)
	@echo c++ $(notdir $<)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Pokes every control with the address and undefined-behaviour sanitizers
# on. A second build tree, since everything (LVGL included) is instrumented.
check:
	@$(MAKE) --no-print-directory BUILD=build-asan SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -g" build-asan/sim
	@ASAN_OPTIONS=detect_leaks=0 ./build-asan/sim $(OUT) sample_today.json exercise 2>&1 | grep -v 'Warn\]' | tail -25

clean:
	rm -rf $(BUILD) build-asan $(OUT)
