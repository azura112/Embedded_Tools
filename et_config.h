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

/* ===================== 模块裁剪开关 ===================== */
#define ET_MODULE_RINGBUF       1   /* core: 环形缓冲区        */
#define ET_MODULE_QUEUE         1   /* core: 定长消息队列      */
#define ET_MODULE_MEMPOOL       1   /* core: 固定块内存池      */
#define ET_MODULE_STIMER        1   /* sys : 软件定时器(预留)  */
#define ET_MODULE_SCHED         1   /* sys : 任务调度器(预留)  */
#define ET_MODULE_EVENT         1   /* sys : 事件标志(预留)    */
#define ET_MODULE_CRC           1   /* proto: CRC校验(预留)    */
#define ET_MODULE_FRAME         1   /* proto: 帧解析器(预留)   */
#define ET_MODULE_ATCMD         1   /* proto: AT命令(预留)     */
#define ET_MODULE_KEY           1   /* driver: 按键(预留)      */
#define ET_MODULE_LED           1   /* driver: LED(预留)       */
#define ET_MODULE_LOG           1   /* debug: 日志(预留)       */

/* ===================== et_ringbuf ===================== */
/* 容量保证为 2 的幂时置 1, 取模运算优化为位与; 容量非 2 的幂必须为 0 */
#ifndef ET_RINGBUF_POW2
#define ET_RINGBUF_POW2         0
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
