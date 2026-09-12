/**
 * @file    test_medfilt.c
 * @brief   et_medfilt 单元测试 (期望值均为手算中位数)
 */
#include "et_test.h"
#include "et_medfilt.h"

static int32_t     g_buf[15];
static et_medfilt_t g_mf;

/* ---------- 生命周期 / 参数校验 ---------- */

static void medfilt_init_validation(void)
{
    ET_CHECK(!et_medfilt_init(NULL, g_buf, 3u));
    ET_CHECK(!et_medfilt_init(&g_mf, NULL, 3u));
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 0u));       /* 过短 */
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 1u));
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 2u));
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 4u));       /* 偶数窗拒绝 */
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 16u));      /* 超长(>15)拒绝 */
    ET_CHECK(!et_medfilt_init(&g_mf, g_buf, 17u));
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 3u));
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 15u));       /* 上边界合法 */
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 5u));
    ET_CHECK_U32_EQ(5u, et_medfilt_window(&g_mf));
    ET_CHECK_U32_EQ(0u, et_medfilt_count(&g_mf));
}

/* ---------- 手算序列 ---------- */

static void medfilt_win3_sequence(void)
{
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 3u));
    ET_CHECK_U32_EQ(5,  (uint32_t)et_medfilt_push(&g_mf, 5));    /* {5} */
    ET_CHECK_U32_EQ(3,  (uint32_t)et_medfilt_push(&g_mf, 3));    /* {5,3} -> 下中位 3 */
    ET_CHECK_U32_EQ(5,  (uint32_t)et_medfilt_push(&g_mf, 8));    /* {3,5,8} */
    ET_CHECK_U32_EQ(8,  (uint32_t)et_medfilt_push(&g_mf, 100));  /* {8,3,100} */
    ET_CHECK_U32_EQ(8,  (uint32_t)et_medfilt_push(&g_mf, 1));    /* {8,1,100} */
    ET_CHECK_U32_EQ(2,  (uint32_t)et_medfilt_push(&g_mf, 2));    /* {2,1,100}? */
    /* 上一步窗={100,1,8} 再推 2 -> buf[2]=2: 窗={100,1,2} -> 中位 2 */
    ET_CHECK_U32_EQ(3u, et_medfilt_count(&g_mf));
}

static void medfilt_pulse_rejection(void)
{
    uint32_t i;

    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 5u));
    for (i = 0u; i < 5u; i++) {
        ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 100));
    }
    /* 单点大尖峰: 中值不动 */
    ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 10000));
    ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 100));
    ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 100));
    /* 两个尖峰(≤(win-1)/2=2)仍被抑制 */
    ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 10000));
    /* 第三个尖峰: 窗内尖峰过半数, 中值被打穿(边界语义) */
    ET_CHECK_U32_EQ(10000, (uint32_t)et_medfilt_push(&g_mf, 10000));
}

static void medfilt_step_delay_half_window(void)
{
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 5u));
    (void)et_medfilt_push(&g_mf, 0);
    (void)et_medfilt_push(&g_mf, 0);
    (void)et_medfilt_push(&g_mf, 0);
    (void)et_medfilt_push(&g_mf, 0);
    (void)et_medfilt_push(&g_mf, 0);
    /* 阶跃后延迟 (win-1)/2 = 2 个样本才翻转 */
    ET_CHECK_U32_EQ(0,   (uint32_t)et_medfilt_push(&g_mf, 100));  /* {0,0,0,0,100} */
    ET_CHECK_U32_EQ(0,   (uint32_t)et_medfilt_push(&g_mf, 100));  /* {0,0,0,100,100} */
    ET_CHECK_U32_EQ(100, (uint32_t)et_medfilt_push(&g_mf, 100));  /* {0,0,100,100,100} */
}

static void medfilt_constant_input(void)
{
    uint32_t i;

    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 5u));
    for (i = 0u; i < 8u; i++) {                 /* 窗满前后输出一致 */
        ET_CHECK_U32_EQ(7, (uint32_t)et_medfilt_push(&g_mf, 7));
    }
}

static void medfilt_negative_values(void)
{
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 3u));
    ET_CHECK_U32_EQ((uint32_t)-5, (uint32_t)et_medfilt_push(&g_mf, -5));
    ET_CHECK_U32_EQ((uint32_t)-5, (uint32_t)et_medfilt_push(&g_mf, -3));  /* {-5,-3} 下中位 */
    ET_CHECK_U32_EQ((uint32_t)-5, (uint32_t)et_medfilt_push(&g_mf, -8));  /* {-8,-5,-3} */
    ET_CHECK_U32_EQ((uint32_t)-3, (uint32_t)et_medfilt_push(&g_mf, -1));  /* {-8,-3,-1} */
}

static void medfilt_descending_input(void)
{
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 3u));
    ET_CHECK_U32_EQ(9, (uint32_t)et_medfilt_push(&g_mf, 9));    /* {9} */
    ET_CHECK_U32_EQ(7, (uint32_t)et_medfilt_push(&g_mf, 7));    /* {7,9} 下中位 7 */
    ET_CHECK_U32_EQ(7, (uint32_t)et_medfilt_push(&g_mf, 5));    /* {5,7,9} */
    ET_CHECK_U32_EQ(5, (uint32_t)et_medfilt_push(&g_mf, 3));    /* buf[0]=3: {3,7,5} */
}

/* ---------- 语义 ---------- */

static void medfilt_reset_semantics(void)
{
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 3u));
    (void)et_medfilt_push(&g_mf, 1);
    (void)et_medfilt_push(&g_mf, 2);
    (void)et_medfilt_push(&g_mf, 3);
    et_medfilt_reset(&g_mf);
    ET_CHECK_U32_EQ(0u, et_medfilt_count(&g_mf));
    ET_CHECK_U32_EQ(9, (uint32_t)et_medfilt_push(&g_mf, 9));    /* 复位后从空窗重计 */
    ET_CHECK_U32_EQ(9, (uint32_t)et_medfilt_push(&g_mf, 9));
    ET_CHECK_U32_EQ(9, (uint32_t)et_medfilt_push(&g_mf, 9));
}

static void medfilt_multi_instance(void)
{
    et_medfilt_t a;
    et_medfilt_t b;
    static int32_t ba[3];
    static int32_t bb[5];

    ET_CHECK(et_medfilt_init(&a, ba, 3u));
    ET_CHECK(et_medfilt_init(&b, bb, 5u));
    ET_CHECK_U32_EQ(50, (uint32_t)et_medfilt_push(&a, 50));
    ET_CHECK_U32_EQ(10, (uint32_t)et_medfilt_push(&b, 10));
    ET_CHECK_U32_EQ(1u, et_medfilt_count(&a));
    ET_CHECK_U32_EQ(1u, et_medfilt_count(&b));
    ET_CHECK_U32_EQ(3u, et_medfilt_window(&a));
    ET_CHECK_U32_EQ(5u, et_medfilt_window(&b));
}

static void medfilt_partial_fill_lower_median(void)
{
    /* 窗满前偶数样本: 取下中位(确定性), 文件头/头注已文档化 */
    ET_CHECK(et_medfilt_init(&g_mf, g_buf, 5u));
    ET_CHECK_U32_EQ(30, (uint32_t)et_medfilt_push(&g_mf, 30));  /* {30} */
    ET_CHECK_U32_EQ(10, (uint32_t)et_medfilt_push(&g_mf, 10));  /* {10,30} 下中位 */
    ET_CHECK_U32_EQ(20, (uint32_t)et_medfilt_push(&g_mf, 20));  /* {10,20,30} */
    ET_CHECK_U32_EQ(15, (uint32_t)et_medfilt_push(&g_mf, 15));  /* {10,15,20,30} 下中位 */
    ET_CHECK_U32_EQ(20, (uint32_t)et_medfilt_push(&g_mf, 25));  /* {10,15,20,25,30} */
}

const et_test_case_t *test_medfilt_cases(size_t *count)
{
    static const et_test_case_t tbl[] = {
        {"medfilt.init_validation",   medfilt_init_validation},
        {"medfilt.win3_sequence",     medfilt_win3_sequence},
        {"medfilt.pulse_rejection",   medfilt_pulse_rejection},
        {"medfilt.step_delay",        medfilt_step_delay_half_window},
        {"medfilt.constant_input",    medfilt_constant_input},
        {"medfilt.negative_values",   medfilt_negative_values},
        {"medfilt.ascending_check",   medfilt_descending_input},
        {"medfilt.reset_semantics",   medfilt_reset_semantics},
        {"medfilt.multi_instance",    medfilt_multi_instance},
        {"medfilt.partial_lower_median", medfilt_partial_fill_lower_median},
    };
    *count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}
