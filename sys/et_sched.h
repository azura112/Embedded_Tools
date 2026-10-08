/**
 * @file    et_sched.h
 * @brief   协作式周期任务调度器 (前后台超级循环专用)
 *
 * 模型:
 *  - 任务为"固定周期函数": 注册时声明 period_ms, 主循环反复调用
 *    et_sched_poll_once(), 到期任务依次执行;
 *  - 错峰: 各任务按注册顺序执行, 建议将周期设为互质以分散负载峰值;
 *  - 漂移吸收: 任务执行耗时导致错过多个周期时, 只补跑一次并重锚定 now,
 *    避免积压风暴(与 et_stimer 策略一致);
 *  - 失准计数(v2.27, REQ-9): 补跑前若"实际间隔 > period_ms + 容差", 记一次
 *    miss —— 调度行为本身(补跑一次 + 重锚)零改动, 计数只观测不干预;
 *    主循环停转期间的漏检属已知边界(口径见 docs/API_GUIDE.md §4.2);
 *
 * 并发策略:
 *  - 【仅限主循环上下文】: register/unregister/poll_once 不可在 ISR 调用,
 *    因此内部无需临界区; ISR 与主循环的交互请使用 et_event 或 et_queue;
 *
 * 时基: uint32 毫秒, period_ms >= 1 且 < 2^31。
 */
#ifndef ET_SCHED_H
#define ET_SCHED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "et_config.h"
#include "port.h"           /* port_tick_ms_t / PORT_TICK_WAIT_FOREVER (tickless) */

#if ET_MODULE_SCHED

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*et_task_fn)(void *arg);

typedef struct et_task {
    struct et_task *next;           /* 内部链表指针, 勿动 */
    et_task_fn      fn;             /* 任务函数           */
    void           *arg;            /* 任务参数           */
    uint32_t        period_ms;      /* 调度周期           */
    uint32_t        last_run;       /* 上次运行时刻       */
    bool            in_list;        /* 是否已注册         */
    /* ---- v2.2 追加字段(API_STABILITY §5 附则人工登记 #1, 破坏半径见登记区) ---- */
    uint32_t        last_ms;        /* 上次执行耗时 ms, 勿动              */
    uint32_t        max_ms;         /* 历史最长执行耗时 ms, 勿动          */
    /* ---- v2.27 追加字段(API_STABILITY §5.1 人工登记, REQ-9 周期失准计数) ---- */
    uint32_t        miss_cnt;       /* 错过周期次数, 勿动                 */
    uint32_t        tol_ms;         /* 判定容差 ms(注册置默认, 勿动)     */
} et_task_t;

/* 注册任务: 尾插保持 FIFO 公平性; period_ms >= 1 */
bool et_sched_register(et_task_t *t, et_task_fn fn, void *arg, uint32_t period_ms);

/* 注销任务: 未注册返回 false */
bool et_sched_unregister(et_task_t *t);

/* 扫描一遍到期任务并逐个执行后返回(非阻塞, 适合主循环 + WFI 低功耗模式) */
void et_sched_poll_once(void);

/* tickless: 最近到期任务还需多少毫秒(相对值, 0=已到期应立即 poll_once);
 * 无注册任务返回 PORT_TICK_WAIT_FOREVER。跨回绕用无符号减法; 注销任务后
 * 下次调用自然重算。🏠MAIN(与模块其余 API 同上下文约束)。
 *
 * ⚠ WFI 配对约束: 依返回值休眠前必须确保唤醒源已使能(串口 RX 中断等)。
 *   "RX 轮询 + WFI"会丢失睡眠窗口内的字节 —— G474 实机教训,
 *   见 移植stm32实机记录.md 问题1 与 API_GUIDE tickless 配方。 */
port_tick_ms_t et_sched_next_due(void);

/* 复位: 注销全部任务(热复位场景/测试隔离用) */
void et_sched_reset(void);

/* ---- v2.2 任务耗时统计(诊断抖动/超时任务的第一手数据) ----
 * 计量口径: poll_once 在每次任务执行前后各取一次毫秒时基, 差值即本次耗时;
 * 分辨率 = 时基粒度(1ms), 快于 1ms 的任务报 0 —— 只反映"毫秒级可观测"的
 * 耗时, 不做高精度剖析(与本模块不引入新时基依赖的定位一致)。 */
/* 只读查询: last_ms = 上次执行耗时, max_ms = 注册以来最长; 未跑过/未注册
 * 任务两者均 0; 任一输出指针可为 NULL(不取该项)。 */
void et_sched_task_stats(const et_task_t *t, uint32_t *last_ms, uint32_t *max_ms);

/* 清零指定任务的耗时统计(max 一并清零; 不影响调度状态) */
void et_sched_task_stats_reset(et_task_t *t);

/* ---- v2.27 周期失准计数(REQ-9: 主循环过载/阻塞的第一手数据) ----
 * 判定口径: poll_once 在补跑前用"重锚前的实际间隔"与 period_ms + tol_ms 比较,
 * 严格大于才计一次(恰好到点不计); 一次补跑至多计一次, 停转期间错过的多个周期
 * 不逐个数(已知边界, 与"不补跑积压"的调度策略一致)。 */
/* 只读查询: 未注册/NULL 报 0; 输出指针可为 NULL(不取该项)。🏠MAIN */
void et_sched_task_miss(const et_task_t *t, uint32_t *miss_cnt);

/* 清零指定任务的 miss 计数(不影响调度状态与耗时统计)。🏠MAIN */
void et_sched_task_miss_reset(et_task_t *t);

/* 设置该任务的失准判定容差 ms(注册时置 ET_SCHED_MISS_TOL_MS 默认值;
 * 传 0 = 严格判"实际间隔 > period_ms")。🏠MAIN */
void et_sched_task_set_tolerance(et_task_t *t, uint32_t tol_ms);

#ifdef __cplusplus
}
#endif

#endif /* ET_MODULE_SCHED */
#endif /* ET_SCHED_H */
