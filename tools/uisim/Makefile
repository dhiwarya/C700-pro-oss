# tools/uisim: desktop build of the firmware UI, rendering screens to images.
#
#   make -C tools/uisim            build build/uisim
#   tools/uisim/run.sh [dark]      build, render all screens, convert to PNG
#
# MAPS: a folder holding MAP/<name>.map (SD_MOUNT_POINT on the device).

ROOT  := ../..
LVGL  := $(ROOT)/managed_components/lvgl__lvgl
MAIN  := $(ROOT)/main
BUILD := build
MAPS  ?= $(abspath ../../../sim_sdcard)

# the real firmware modules compiled for the desktop
FW    := ui datapage theme layouts fields config stats sun mapview mapfile
FW_SRCS   := $(addprefix $(MAIN)/,$(addsuffix .c,$(FW))) $(wildcard $(MAIN)/fonts/*.c)
SIM_SRCS  := sim_main.c sim_rtos.c sim_data.c
LVGL_SRCS := $(shell find $(LVGL)/src -name '*.c')

CC     ?= cc
CFLAGS := -O1 -g -std=gnu11 -DLV_CONF_INCLUDE_SIMPLE -DUISIM -DSD_MOUNT_POINT='"$(MAPS)"' \
          -I. -Istubs -I$(LVGL) -I$(MAIN) -I$(MAIN)/fonts -Wno-unused-parameter
LDLIBS := -lm

LVGL_OBJS := $(patsubst $(LVGL)/%.c,$(BUILD)/lvgl/%.o,$(LVGL_SRCS))
FW_OBJS   := $(patsubst $(ROOT)/%.c,$(BUILD)/fw/%.o,$(FW_SRCS))
SIM_OBJS  := $(patsubst %.c,$(BUILD)/sim/%.o,$(SIM_SRCS))

$(BUILD)/uisim: $(LVGL_OBJS) $(FW_OBJS) $(SIM_OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(BUILD)/lvgl/%.o: $(LVGL)/%.c lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c $< -o $@

$(BUILD)/fw/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sim/%.o: %.c sim.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: clean
