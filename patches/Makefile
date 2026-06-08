TARGET = patches.elf

CC      ?= clang
LD      ?= ld.lld

# Cross-compile the patches as MIPS so the recompiler can ingest patches.elf.
CFLAGS   := -target mips -mips2 -mabi=32 -g -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
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

clean:
	rm -rf $(C_OBJS) $(TARGET) $(C_DEPS)

-include $(C_DEPS)

.PHONY: clean
