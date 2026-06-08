# GBA VM Makefile
# Requires devkitARM toolchain

# Toolchain
DEVKITPRO ?= C:/devkitPro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
PREFIX := $(DEVKITARM)/bin/arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
GBAFIX := $(DEVKITPRO)/tools/bin/gbafix

# Directories
SRCDIR := src
INCDIR := include
OBJDIR := obj
BINDIR := bin

# Compiler flags
CFLAGS := -mthumb -mthumb-interwork -mcpu=arm7tdmi -specs=gba.specs
CFLAGS += -Wall -Wextra -O2 -fomit-frame-pointer
CFLAGS += -I$(INCDIR)

# Linker flags
LDFLAGS := -Wl,-Map,$(BINDIR)/game.map

# Source files
SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target
all: $(BINDIR)/game.gba

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link ELF binary
$(BINDIR)/game.elf: $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

# Convert to GBA ROM
$(BINDIR)/game.gba: $(BINDIR)/game.elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@

# Clean
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Phony targets
.PHONY: all clean

# ---------------------------------------------------------------------------
# Host-side unit tests
#
# Most of the engine talks directly to GBA hardware (memory-mapped registers,
# VRAM, OAM) and can only be meaningfully exercised on real hardware or in an
# emulator. The bytecode VM/script-runner (src/vm.c), however, is plain
# portable C, so it's compiled and run here with the host toolchain — fast
# feedback without needing devkitARM or a GBA at all.
# ---------------------------------------------------------------------------
HOST_CC ?= gcc
HOST_CFLAGS := -std=c11 -Wall -Wextra -I$(INCDIR) -Itests
TEST_BIN := $(BINDIR)/test_runner
TEST_SOURCES := tests/test_vm.c tests/test_stubs.c $(SRCDIR)/vm.c $(SRCDIR)/camera.c $(SRCDIR)/collision.c

test: | $(BINDIR)
	$(HOST_CC) $(HOST_CFLAGS) $(TEST_SOURCES) -o $(TEST_BIN)
	$(TEST_BIN)

.PHONY: test
