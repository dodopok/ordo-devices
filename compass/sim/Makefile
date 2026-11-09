# Draws every screen of the compass on a computer, with the same LVGL, the
# same fonts and the same ui/ code the firmware runs.
#
#   pio pkg install -e amoled175c     (once: fetches LVGL and ArduinoJson)
#   make -C sim shots                 (PNGs in sim/out/)
#   make -C sim movie                 (sim/out/ordo.gif: the dips and entrances)
#   make -C sim check                 (pokes every control under ASAN)
#   make -C sim test                  (the clock and wake arithmetic)
#
# Needs gcc, g++ and python3 with Pillow.

LIBDEPS ?= ../.pio/libdeps/amoled175c
LVGL    := $(LIBDEPS)/lvgl
JSON    := $(LIBDEPS)/ArduinoJson/src
JPEG    := $(LIBDEPS)/JPEGDEC/src
BUILD   := build
OUT     := out

# __LINUX__ is how the JPEG decoder is told it is not on a microcontroller.
DEFS  := -DLV_CONF_INCLUDE_SIMPLE -DORDO_BOARD_AMOLED_175C -DORDO_FIRMWARE_VERSION=\"sim\" -D__LINUX__
INCS  := -Ishim -I../include -I../src -I$(LVGL) -I$(JSON) -I$(JPEG)
SAN      ?=
CFLAGS   := -O1 -w $(SAN) $(DEFS) $(INCS)
CXXFLAGS := -O1 -std=gnu++17 -Wall -Wno-unused-function $(SAN) $(DEFS) $(INCS)

LVGL_SRC := $(shell find $(LVGL)/src -name '*.c')
FONT_SRC := $(wildcard ../src/ui/fonts/*.c)
# The screens, the model, and the picture decoder with its library: the same
# sources the firmware compiles.
SHARED   := ../src/model/today.cpp ../src/model/content.cpp ../src/content/art.cpp
APP_SRC  := $(wildcard ../src/ui/*.cpp) $(SHARED) $(JPEG)/JPEGDEC.cpp sim_main.cpp

LVGL_OBJ := $(patsubst $(LVGL)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRC))
FONT_OBJ := $(patsubst ../src/ui/fonts/%.c,$(BUILD)/fonts/%.o,$(FONT_SRC))
APP_OBJ  := $(patsubst %.cpp,$(BUILD)/app/%.o,$(notdir $(APP_SRC)))

vpath %.cpp ../src/ui ../src/model ../src/content $(JPEG) .

# The celebration page needs a picture like the ones the content host sends.
# The real ones belong to the CMS; a painted stand-in is made here. Point
# ORDO_SIM_CONTENT at a folder of real <slug>.jpg and <slug>.<locale>.json
# files to see those instead.
ART := content/celebration-martin-of-tours.jpg
$(ART): make_art.py
	@python3 make_art.py $@

.PHONY: shots movie check test clean
shots: $(BUILD)/sim $(ART)
	@mkdir -p $(OUT)
	@rm -f $(OUT)/*.png $(OUT)/*.ppm
	@./$(BUILD)/sim $(OUT) sample_today.json 2>/dev/null
	@python3 to_png.py $(OUT)

# The screens in motion: every step of a walk through them, with the panel's
# light as the firmware would set it.
movie: $(BUILD)/sim $(ART)
	@mkdir -p $(OUT)/movie
	@./$(BUILD)/sim $(OUT)/movie sample_today.json movie 2>/dev/null
	@python3 to_gif.py $(OUT)/movie $(OUT)/ordo.gif
	@rmdir $(OUT)/movie

$(BUILD)/sim: $(LVGL_OBJ) $(FONT_OBJ) $(APP_OBJ)
	@echo link $@
	@$(CXX) $(SAN) -o $@ $^ -lm

$(BUILD)/lvgl/%.o: $(LVGL)/src/%.c ../include/lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fonts/%.o: ../src/ui/fonts/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# The JPEG decoder is somebody else's code and, on a 64-bit computer, reads
# its bit stream through unaligned words. That is its business: it is checked
# for memory errors like the rest, not for undefined behaviour.
$(BUILD)/app/JPEGDEC.o: $(JPEG)/JPEGDEC.cpp
	@mkdir -p $(dir $@)
	@echo c++ $(notdir $<)
	@$(CXX) $(CXXFLAGS) -w $(if $(SAN),-fno-sanitize=undefined) -c $< -o $@

$(BUILD)/app/%.o: %.cpp $(wildcard ../src/ui/*.h) $(wildcard ../src/model/*.h) ../src/content/art.h
	@mkdir -p $(dir $@)
	@echo c++ $(notdir $<)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Pokes every control with the address and undefined-behaviour sanitizers
# on. A second build tree, since everything (LVGL included) is instrumented.
check: $(ART)
	@$(MAKE) --no-print-directory BUILD=build-asan SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -g" build-asan/sim
	@ASAN_OPTIONS=detect_leaks=0 ./build-asan/sim $(OUT) sample_today.json exercise 2>&1 | grep -v 'Warn\]' | tail -25

# The arithmetic the battery depends on: the clock, the wake schedule,
# reminders, the day seen from another date. No LVGL drawing involved, but
# the model needs its colour type, hence the include path.
TEST_SRC := test_logic.cpp ../src/app/schedule.cpp ../src/sys/clock.cpp ../src/sys/rtc_state.cpp $(SHARED)
test: $(LVGL_OBJ) $(FONT_OBJ) $(ART)
	@$(CXX) -std=gnu++17 -O1 -w -fsanitize=address $(DEFS) $(INCS) -c $(JPEG)/JPEGDEC.cpp -o $(BUILD)/test_jpegdec.o
	@$(CXX) -std=gnu++17 -O1 -Wall -Wno-maybe-uninitialized -fsanitize=address,undefined $(DEFS) $(INCS) -Dgettimeofday=fakeGettimeofday \
	  -o $(BUILD)/test_logic $(TEST_SRC) $(BUILD)/test_jpegdec.o $(LVGL_OBJ) $(FONT_OBJ) -lm
	@./$(BUILD)/test_logic sample_today.json

clean:
	rm -rf $(BUILD) build-asan $(OUT)
