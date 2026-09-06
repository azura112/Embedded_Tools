/* STM32G474VET6 链接脚本: 512K Flash / 128K RAM (与 CubeMX 生成脚本同几何)
 * RAM = SRAM1 80K + SRAM2 16K + CCM SRAM 32K 别名 (0x20018000 起, DMA 不可达;
 * 本库零 DMA, 无影响)。
 * et_kv 参数区 = 片内 flash 末端 32KB (16×2KB 页, PORT_FLASH_SECTOR_SIZE/COUNT)。
 * 代码/常量/数据 LMA 一律禁止落入参数区, 文件末尾的 ASSERT 是链接期兜底。 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);    /* 栈顶 = RAM 末尾, 满递减栈 */

/* 参数区起点 = 0x08078000 (须与 et_config.h 几何一致, -D 覆盖时同步改这里) */
_param_area_base = ORIGIN(FLASH) + LENGTH(FLASH) - 32K;

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* .data: 运行地址在 RAM, 加载地址在 FLASH */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    . = ALIGN(4);
    _end = .;                           /* 堆起点(本库零动态内存, 仅调试参考) */

    /* 镜像 LMA 末尾 (含 .rodata 与 .data 的加载映像) 不得越过参数区 */
    _sidata_end = _sidata + SIZEOF(.data);
    ASSERT(_sidata_end <= _param_area_base,
           "image overlaps et_kv param area (tail 32K of flash)")
}
