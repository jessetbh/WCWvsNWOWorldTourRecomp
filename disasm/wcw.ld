SECTIONS
{
    __romPos = 0;
    header_ROM_START = __romPos;
    header_VRAM = ADDR(.header);
    .header : AT(header_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        header_DATA_START = .;
        build/asm/header.o(.data);
        header_DATA_END = .;
        header_DATA_SIZE = ABSOLUTE(header_DATA_END - header_DATA_START);
    }
    __romPos += SIZEOF(.header);
    header_ROM_END = __romPos;
    header_VRAM_END = .;

    ipl3_ROM_START = __romPos;
    ipl3_VRAM = ADDR(.ipl3);
    .ipl3 : AT(ipl3_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        ipl3_DATA_START = .;
        build/assets/ipl3.o(.data);
        ipl3_DATA_END = .;
        ipl3_DATA_SIZE = ABSOLUTE(ipl3_DATA_END - ipl3_DATA_START);
    }
    __romPos += SIZEOF(.ipl3);
    ipl3_ROM_END = __romPos;
    ipl3_VRAM_END = .;

    entry_ROM_START = __romPos;
    entry_VRAM = ADDR(.entry);
    .entry 0x80000400 : AT(entry_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        entry_TEXT_START = .;
        build/asm/1000.o(.text);
        . = ALIGN(., 16);
        entry_TEXT_END = .;
        entry_TEXT_SIZE = ABSOLUTE(entry_TEXT_END - entry_TEXT_START);
        entry_DATA_START = .;
        build/asm/1000.o(.data);
        . = ALIGN(., 16);
        entry_DATA_END = .;
        entry_DATA_SIZE = ABSOLUTE(entry_DATA_END - entry_DATA_START);
        entry_RODATA_START = .;
        build/asm/1000.o(.rodata);
        . = ALIGN(., 16);
        entry_RODATA_END = .;
        entry_RODATA_SIZE = ABSOLUTE(entry_RODATA_END - entry_RODATA_START);
    }
    entry_bss_VRAM = ADDR(.entry_bss);
    .entry_bss (NOLOAD) : SUBALIGN(16)
    {
        FILL(0x00000000);
        entry_BSS_START = .;
        build/asm/1000.o(.bss);
        . = ALIGN(., 16);
        entry_BSS_END = .;
        entry_BSS_SIZE = ABSOLUTE(entry_BSS_END - entry_BSS_START);
    }
    __romPos += SIZEOF(.entry);
    __romPos = ALIGN(__romPos, 16);
    . = ALIGN(., 16);
    entry_ROM_END = __romPos;
    entry_VRAM_END = .;

    main_ROM_START = __romPos;
    main_VRAM = ADDR(.main);
    .main entry_VRAM_END : AT(main_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        main_TEXT_START = .;
        build/asm/1050.o(.text);
        . = ALIGN(., 16);
        main_TEXT_END = .;
        main_TEXT_SIZE = ABSOLUTE(main_TEXT_END - main_TEXT_START);
        main_DATA_START = .;
        build/asm/1050.o(.data);
        build/asm/asm/data/2A9B0.data.o(.data);
        . = ALIGN(., 16);
        main_DATA_END = .;
        main_DATA_SIZE = ABSOLUTE(main_DATA_END - main_DATA_START);
        main_RODATA_START = .;
        build/asm/1050.o(.rodata);
        . = ALIGN(., 16);
        main_RODATA_END = .;
        main_RODATA_SIZE = ABSOLUTE(main_RODATA_END - main_RODATA_START);
    }
    main_bss_VRAM = ADDR(.main_bss);
    .main_bss (NOLOAD) : SUBALIGN(16)
    {
        FILL(0x00000000);
        main_BSS_START = .;
        build/asm/1050.o(.bss);
        build/asm/asm/data/393F0.bss.o(.bss);
        . = ALIGN(., 16);
        main_BSS_END = .;
        main_BSS_SIZE = ABSOLUTE(main_BSS_END - main_BSS_START);
    }
    __romPos += SIZEOF(.main);
    __romPos = ALIGN(__romPos, 16);
    . = ALIGN(., 16);
    main_ROM_END = __romPos;
    main_VRAM_END = .;

    assets_ROM_START = __romPos;
    assets_VRAM = ADDR(.assets);
    .assets main_VRAM_END : AT(assets_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        assets_DATA_START = .;
        build/assets/assets.o(.data);
        assets_DATA_END = .;
        assets_DATA_SIZE = ABSOLUTE(assets_DATA_END - assets_DATA_START);
    }
    __romPos += SIZEOF(.assets);
    assets_ROM_END = __romPos;
    assets_VRAM_END = .;

    ovl_a_ROM_START = __romPos;
    ovl_a_VRAM = ADDR(.ovl_a);
    .ovl_a 0x80090000 : AT(ovl_a_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        ovl_a_TEXT_START = .;
        build/asm/A21750.o(.text);
        . = ALIGN(., 16);
        ovl_a_TEXT_END = .;
        ovl_a_TEXT_SIZE = ABSOLUTE(ovl_a_TEXT_END - ovl_a_TEXT_START);
        ovl_a_DATA_START = .;
        build/asm/A21750.o(.data);
        build/asm/asm/data/A55310.data.o(.data);
        . = ALIGN(., 16);
        ovl_a_DATA_END = .;
        ovl_a_DATA_SIZE = ABSOLUTE(ovl_a_DATA_END - ovl_a_DATA_START);
        ovl_a_RODATA_START = .;
        build/asm/A21750.o(.rodata);
        . = ALIGN(., 16);
        ovl_a_RODATA_END = .;
        ovl_a_RODATA_SIZE = ABSOLUTE(ovl_a_RODATA_END - ovl_a_RODATA_START);
    }
    ovl_a_bss_VRAM = ADDR(.ovl_a_bss);
    .ovl_a_bss (NOLOAD) : SUBALIGN(16)
    {
        FILL(0x00000000);
        ovl_a_BSS_START = .;
        build/asm/A21750.o(.bss);
        build/asm/asm/data/A69570.bss.o(.bss);
        . = ALIGN(., 16);
        ovl_a_BSS_END = .;
        ovl_a_BSS_SIZE = ABSOLUTE(ovl_a_BSS_END - ovl_a_BSS_START);
    }
    __romPos += SIZEOF(.ovl_a);
    __romPos = ALIGN(__romPos, 16);
    . = ALIGN(., 16);
    ovl_a_ROM_END = __romPos;
    ovl_a_VRAM_END = .;

    ovl_b_ROM_START = __romPos;
    ovl_b_VRAM = ADDR(.ovl_b);
    .ovl_b 0x80090000 : AT(ovl_b_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        ovl_b_TEXT_START = .;
        build/asm/A69570.o(.text);
        . = ALIGN(., 16);
        ovl_b_TEXT_END = .;
        ovl_b_TEXT_SIZE = ABSOLUTE(ovl_b_TEXT_END - ovl_b_TEXT_START);
        ovl_b_DATA_START = .;
        build/asm/A69570.o(.data);
        build/asm/asm/data/ABF4E0.data.o(.data);
        . = ALIGN(., 16);
        ovl_b_DATA_END = .;
        ovl_b_DATA_SIZE = ABSOLUTE(ovl_b_DATA_END - ovl_b_DATA_START);
        ovl_b_RODATA_START = .;
        build/asm/A69570.o(.rodata);
        . = ALIGN(., 16);
        ovl_b_RODATA_END = .;
        ovl_b_RODATA_SIZE = ABSOLUTE(ovl_b_RODATA_END - ovl_b_RODATA_START);
    }
    ovl_b_bss_VRAM = ADDR(.ovl_b_bss);
    .ovl_b_bss (NOLOAD) : SUBALIGN(16)
    {
        FILL(0x00000000);
        ovl_b_BSS_START = .;
        build/asm/A69570.o(.bss);
        build/asm/asm/data/AC2970.bss.o(.bss);
        . = ALIGN(., 16);
        ovl_b_BSS_END = .;
        ovl_b_BSS_SIZE = ABSOLUTE(ovl_b_BSS_END - ovl_b_BSS_START);
    }
    __romPos += SIZEOF(.ovl_b);
    __romPos = ALIGN(__romPos, 16);
    . = ALIGN(., 16);
    ovl_b_ROM_END = __romPos;
    ovl_b_VRAM_END = .;

    assets_tail_ROM_START = __romPos;
    assets_tail_VRAM = ADDR(.assets_tail);
    .assets_tail : AT(assets_tail_ROM_START) SUBALIGN(16)
    {
        FILL(0x00000000);
        assets_tail_DATA_START = .;
        build/assets/assets_tail.o(.data);
        assets_tail_DATA_END = .;
        assets_tail_DATA_SIZE = ABSOLUTE(assets_tail_DATA_END - assets_tail_DATA_START);
    }
    __romPos += SIZEOF(.assets_tail);
    assets_tail_ROM_END = __romPos;
    assets_tail_VRAM_END = .;

    /DISCARD/ :
    {
        *(*);
    }
}
