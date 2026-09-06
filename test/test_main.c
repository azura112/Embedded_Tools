/**
 * @file    test_main.c
 * @brief   单元测试入口: 汇总所有测试套件
 */
#include "et_test.h"

extern const et_test_case_t *test_ringbuf_cases(size_t *count);
extern const et_test_case_t *test_queue_cases(size_t *count);
extern const et_test_case_t *test_mempool_cases(size_t *count);
extern const et_test_case_t *test_list_cases(size_t *count);
extern const et_test_case_t *test_filter_cases(size_t *count);
extern const et_test_case_t *test_stimer_cases(size_t *count);
extern const et_test_case_t *test_sched_cases(size_t *count);
extern const et_test_case_t *test_event_cases(size_t *count);
extern const et_test_case_t *test_crc_cases(size_t *count);
extern const et_test_case_t *test_frame_cases(size_t *count);
extern const et_test_case_t *test_atcmd_cases(size_t *count);
extern const et_test_case_t *test_key_cases(size_t *count);
extern const et_test_case_t *test_led_cases(size_t *count);
extern const et_test_case_t *test_spwm_cases(size_t *count);
extern const et_test_case_t *test_log_cases(size_t *count);
extern const et_test_case_t *test_assert_cases(size_t *count);
extern const et_test_case_t *test_kv_cases(size_t *count);
extern const et_test_case_t *test_softclock_cases(size_t *count);
extern const et_test_case_t *test_fsm_cases(size_t *count);
extern const et_test_case_t *test_xmodem_cases(size_t *count);
extern const et_test_case_t *test_shell_cases(size_t *count);
extern const et_test_case_t *test_bootctl_cases(size_t *count);
extern const et_test_case_t *test_wdt_cases(size_t *count);
extern const et_test_case_t *test_shell_hist_cases(size_t *count);
extern const et_test_case_t *test_selftest_cases(size_t *count);

typedef const et_test_case_t *(*et_suite_get_fn)(size_t *count);

typedef struct {
    const char    *name;
    et_suite_get_fn get;
} et_suite_t;

static const et_suite_t g_suites[] = {
    { "core/ringbuf", test_ringbuf_cases },
    { "core/queue",   test_queue_cases   },
    { "core/mempool", test_mempool_cases },
    { "core/list",    test_list_cases    },
    { "algo/filter",  test_filter_cases  },
    { "sys/stimer",   test_stimer_cases  },
    { "sys/sched",    test_sched_cases   },
    { "sys/event",    test_event_cases   },
    { "proto/crc",    test_crc_cases     },
    { "proto/frame",  test_frame_cases   },
    { "proto/atcmd",  test_atcmd_cases   },
    { "driver/key",   test_key_cases     },
    { "driver/led",   test_led_cases     },
    { "driver/spwm",  test_spwm_cases    },
    { "debug/log",    test_log_cases     },
    { "debug/assert", test_assert_cases  },
    { "storage/kv",   test_kv_cases      },
    { "sys/softclock", test_softclock_cases },
    { "algo/fsm",     test_fsm_cases     },
    { "proto/xmodem", test_xmodem_cases  },
    { "debug/shell",  test_shell_cases   },
    { "storage/bc",   test_bootctl_cases },
    { "sys/wdt",      test_wdt_cases     },
    { "debug/selftest", test_selftest_cases },
    { "debug/shhist", test_shell_hist_cases },
};

int main(void)
{
    int      total_fail = 0;
    size_t   i;

    printf("=== Embedded_Tools Unit Tests ===\n");
    for (i = 0u; i < (sizeof(g_suites) / sizeof(g_suites[0])); i++) {
        size_t               n      = 0u;
        const et_test_case_t *cases = g_suites[i].get(&n);

        total_fail += et_test_run(g_suites[i].name, cases, n);
    }

    printf("\n=== RESULT: %s (fail=%d) ===\n",
           (total_fail == 0) ? "ALL PASS" : "FAILED", total_fail);
    return (total_fail == 0) ? 0 : 1;
}
