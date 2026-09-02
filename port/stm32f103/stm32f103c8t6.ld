/* STM32F103C8T6 (BluePill) 链接脚本: 64K Flash / 20K RAM
 * et_kv 参数区 = 片内 flash 末端 16KB (PORT_FLASH_SECTOR_SIZE/COUNT 默认值)。
 * 代码/常量/数据 LMA 一律禁止落入参数区 (F1 页擦目标 = 取指页, 见 port_stm32f103.c),
 * 文件末尾的 ASSERT 是链接期兜底。 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);    /* 栈顶 = RAM 末尾, 满递减栈 */

/* 参数区起点 = 0x0800C000 (须与 et_config.h 几何一致) */
_param_area_base = ORIGIN(FLASH) + LENGTH(FLASH) - 16K;

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
           "image overlaps et_kv param area (tail 16K of flash)")
}
