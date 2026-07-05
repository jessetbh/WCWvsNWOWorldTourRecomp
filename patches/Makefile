TARGET = patches.elf

# MIPS-capable cross tools. NOTE: both the VS BuildTools and llvm.org WINDOWS builds of
# clang are trimmed to x86/ARM backends only, so on this machine the compiler is
# `zig cc` (Zig bundles a full LLVM incl. MIPS). ld.lld's MIPS support is unconditional,
# so any lld works. CMake passes CC/LD explicitly (see PATCHES_C_COMPILER/PATCHES_LD).
# (make predefines CC=cc / LD=ld, so `?=` won't fire — test the origin instead.)
ifeq ($(origin CC),default)
CC      := C:/Users/selki/toolchains/zig-windows-x86_64-0.14.0/zig.exe cc
endif
ifeq ($(origin LD),default)
LD      := C:/Users/selki/toolchains/clang+llvm-19.1.5-x86_64-pc-windows-msvc/bin/ld.lld.exe
endif

# Cross-compile the patches as MIPS so the recompiler can ingest patches.elf.
# (`-target mips-freestanding-none -mcpu=mips2` is the zig-compatible spelling of
# plain clang's `-target mips -mips2`; verified to emit the same non-PIC
# R_MIPS_HI16/LO16/26 relocs N64Recomp expects.)
CFLAGS   := -target mips-freestanding-none -mcpu=mips2 -mabi=32 -g -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
			-fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset \
			-Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas \
			-Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces \
			-Wno-unsupported-floating-point-opt -Wno-switch -D_MIPS_SZLONG=32

# Include paths. dummy_headers stubs out anything the patches reference but don't need a
# real definition for. Once a WCW headers source exists (a decomp or hand-written SDK
# headers), add it here the way BMHero adds -I ../lib/bmhero/include.
CPPFLAGS := -nostdinc -DF3DEX_GBI -D_LANGUAGE_C -DMIPS \
			-I dummy_headers \
			-I ../lib/N64ModernRuntime/ultramodern/include \
			-I ../lib/N64ModernRuntime/ultramodern/include/ultramodern

LDFLAGS  := -nostdlib -T patches.ld -T syms.ld -Map patches.map --unresolved-symbols=ignore-all --emit-relocs

C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)
C_DEPS := $(C_SRCS:.c=.d)

$(TARGET): $(C_OBJS) patches.ld syms.ld
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(C_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

# mingw32-make on Windows has no sh/rm; route deletion through cmd.
ifeq ($(OS),Windows_NT)
clean:
	cmd /C "del /Q $(C_OBJS) $(TARGET) $(C_DEPS) patches.map 2>nul || exit 0"
else
clean:
	rm -rf $(C_OBJS) $(TARGET) $(C_DEPS) patches.map
endif

-include $(C_DEPS)

.PHONY: clean
