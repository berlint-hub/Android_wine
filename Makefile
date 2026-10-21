# Minimal devkitPro/libnx build for the AndroidWine hardware probe.
# Build with: make (from an environment with DEVKITPRO configured).

.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO to your devkitPro installation)
endif

include $(DEVKITPRO)/libnx/switch_rules

TARGET := androidwine_probe
BUILD := build
SOURCES := source
DATA :=
INCLUDES :=
APP_TITLE := AndroidWine Probe
APP_AUTHOR := AndroidWine contributors
APP_VERSION := 0.1.0

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -g -Wall -Wextra -O2 $(ARCH) -D__SWITCH__ -D_GNU_SOURCE
ASFLAGS := $(ARCH)
LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs $(ARCH) -Wl,-Map,$(TARGET).map
LIBS := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                 $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                 -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
export NACPFLAGS += --titleid=0100A0D012345000

.PHONY: all clean
all: $(BUILD)

$(BUILD):
	@mkdir -p "$@"
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

else

.PHONY: all
all: $(OUTPUT).nro

endif

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).map
