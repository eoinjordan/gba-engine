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
