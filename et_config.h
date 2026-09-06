/**
 * @file    et_config.h
 * @brief   Embedded_Tools 全局裁剪配置
 *
 * 移植说明:
 *  1. 将本文件拷贝到工程中, 按需修改各配置项;
 *  2. 所有配置均带 #ifndef 保护, 可改用编译器 -D 选项覆盖;
 *  3. 模块开关置 0 后, 对应模块的 .c 不应参与编译(头文件内容亦被屏蔽)。
 */
#ifndef ET_CONFIG_H
#define ET_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 版本信息 ===================== */
/* 发布时须与 git tag 一致 (tag 规则: v主.次.补) */
#define ET_VERSION_MAJOR        1
#define ET_VERSION_MINOR        7
#define ET_VERSION_PATCH        0
/* 整数编码 0x010700 = 1.7.0, 便于条件编译比较: #if ET_VERSION >= 0x010700 */
#define ET_VERSION              ((ET_VERSION_MAJOR << 16) | \
                                 (ET_VERSION_MINOR << 8)  | \
                                 (ET_VERSION_PATCH))
/* 字符串 "主.次.补", 由上方三个分量自动拼出 */
#define ET_VERSION_STR_(x)      #x
#define ET_VERSION_STR(x)       ET_VERSION_STR_(x)
#define ET_VERSION_STRING       ET_VERSION_STR(ET_VERSION_MAJOR) "." \
                                ET_VERSION_STR(ET_VERSION_MINOR) "." \
                                ET_VERSION_STR(ET_VERSION_PATCH)

/* ===================== 模块裁剪开关 ===================== */
/* 均带 #ifndef 保护, 可用 -DET_MODULE_XXX=0 覆盖(对应 .c 仍需移出编译列表) */
#ifndef ET_MODULE_RINGBUF
#define ET_MODULE_RINGBUF       1   /* core: SPSC 环形缓冲区            */
#endif
#ifndef ET_MODULE_QUEUE
#define ET_MODULE_QUEUE         1   /* core: 定长消息队列 (SPSC)        */
#endif
#ifndef ET_MODULE_MEMPOOL
#define ET_MODULE_MEMPOOL       1   /* core: 固定块内存池               */
#endif
#ifndef ET_MODULE_LIST
#define ET_MODULE_LIST          1   /* core: 侵入式双向链表             */
#endif
#ifndef ET_MODULE_FILTER
#define ET_MODULE_FILTER        1   /* algorithm: 定点数字滤波器组      */
#endif
#ifndef ET_MODULE_FSM
#define ET_MODULE_FSM           1   /* algorithm: 表驱动状态机          */
#endif
#ifndef ET_MODULE_STIMER
#define ET_MODULE_STIMER        1   /* sys  : 软件定时器                */
#endif
#ifndef ET_MODULE_SCHED
#define ET_MODULE_SCHED         1   /* sys  : 协作式周期任务调度器      */
#endif
#ifndef ET_MODULE_WDT
#define ET_MODULE_WDT           1   /* sys  : 看门狗封装+阻塞段保护     */
#endif
#ifndef ET_MODULE_EVENT
#define ET_MODULE_EVENT         1   /* sys  : 32 位事件标志组           */
#endif
#ifndef ET_MODULE_CRC
#define ET_MODULE_CRC           1   /* proto: CRC8/CRC16/CRC32 校验     */
#endif
#ifndef ET_MODULE_FRAME
#define ET_MODULE_FRAME         1   /* proto: 字节流帧解析器            */
#endif
#ifndef ET_MODULE_ATCMD
#define ET_MODULE_ATCMD         1   /* proto: AT 命令解析器             */
#endif
#ifndef ET_MODULE_XMODEM
#define ET_MODULE_XMODEM        1   /* proto: XMODEM-CRC 接收器         */
#endif
#ifndef ET_MODULE_KEY
#define ET_MODULE_KEY           1   /* driver: 按键状态机               */
#endif
#ifndef ET_MODULE_LED
#define ET_MODULE_LED           1   /* driver: LED 模式管理器           */
#endif
#ifndef ET_MODULE_SPWM
#define ET_MODULE_SPWM          1   /* driver: 多通道软件 PWM           */
#endif
#ifndef ET_MODULE_KV
#define ET_MODULE_KV            1   /* storage: flash 键值掉电存储      */
#endif
#ifndef ET_MODULE_BOOTCTL
#define ET_MODULE_BOOTCTL       1   /* storage: 安全升级控制(A/B 状态机)*/
#endif
#ifndef ET_MODULE_SOFTCLOCK
#define ET_MODULE_SOFTCLOCK     1   /* sys: 软时钟(毫秒 tick→日历时钟)  */
#endif
#ifndef ET_MODULE_SHELL
#define ET_MODULE_SHELL         1   /* debug: 行式交互壳(atcmd 之上)    */
#endif

#ifndef ET_MODULE_SELFTEST
#define ET_MODULE_SELFTEST      0   /* debug: 板上自测组件(v1.7, 发布默认裁剪;
                                        *   host 测试/需自测的固件以 -D=1 启用) */
#endif
#ifndef ET_MODULE_LOG
#define ET_MODULE_LOG           1   /* debug: 分级日志                  */
#endif

/* ===================== et_ringbuf ===================== */
/* 容量保证为 2 的幂时置 1, 取模运算优化为位与; 容量非 2 的幂必须为 0 */
#ifndef ET_RINGBUF_POW2
#define ET_RINGBUF_POW2         0
#endif

/* ===================== et_crc ===================== */
/* 查表优化: 置 1 时 CRC16-CCITT 用 256 项静态表(吞吐优先, 表驻只读段),
 * 默认 0 保持位算法零 RAM; 表放置段可用 -DET_CRC_TABLE_SECTION=".段名" 指定 */
#ifndef ET_CRC_TABLE
#define ET_CRC_TABLE            0
#endif

/* ===================== et_mempool ===================== */
/* 块区对齐粒度, 默认按指针宽度对齐 */
#ifndef ET_MEMPOOL_ALIGN
#define ET_MEMPOOL_ALIGN        ((uint32_t)sizeof(void *))
#endif
/* 严格模式: free 时校验指针归属与重复释放(增加少量代码, 建议调试期开启) */
#ifndef ET_MEMPOOL_STRICT
#define ET_MEMPOOL_STRICT       1
#endif

/* ===================== et_spwm ===================== */
/* 软件 PWM 最大通道数(静态注册表容量), 按需裁剪节省 RAM */
#ifndef ET_SPWM_CH_MAX
#define ET_SPWM_CH_MAX          4
#endif

/* ===================== flash 参数区几何 (port 契约, 见 port.h) ===================== */
/* 仅 ET_MODULE_KV=1 时参与编译; 平台几何不同用 -D 覆盖 */
#ifndef PORT_FLASH_SECTOR_SIZE
#define PORT_FLASH_SECTOR_SIZE  1024u    /* F103 中容量页 1KB */
#endif
#ifndef PORT_FLASH_SECTOR_COUNT
#define PORT_FLASH_SECTOR_COUNT 16u      /* 参数区扇区数(et_kv 用其中两扇区) */
#endif
#ifndef PORT_FLASH_ERASE_MS_MAX
#define PORT_FLASH_ERASE_MS_MAX 20u      /* F103 1KB 页擦典型 ~20ms */
#endif

/* ===================== 调试断言 ===================== */
/* 默认空实现; 平台可映射到自身断言/复位钩子, 例如:
 *   #define ET_ASSERT(cond)  do{ if(!(cond)) et_fault_halt(__FILE__, __LINE__); }while(0) */
#ifndef ET_ASSERT
#define ET_ASSERT(cond)         ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ET_CONFIG_H */
