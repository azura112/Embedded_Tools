/**
 * @file    et_test.c
 * @brief   轻量级单元测试框架实现 (仅宿主机使用)
 */
#include "et_test.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

jmp_buf      et_test_jmp_buf;
const char  *et_test_file = "";
long         et_test_line = 0;
char         et_test_msg[256] = "";

int et_test_run(const char *suite, const et_test_case_t *cases, size_t count)
{
    size_t   i;
    unsigned pass = 0u;
    unsigned fail = 0u;

    printf("\n[Suite] %s (%lu cases)\n", suite, (unsigned long)count);

    for (i = 0u; i < count; i++) {
        printf("  %-46s ", cases[i].name);
        fflush(stdout);

        if (setjmp(et_test_jmp_buf) == 0) {
            cases[i].fn();
            printf("PASS\n");
            pass++;
        } else {
            printf("FAIL\n    at %s:%ld  %s\n",
                   et_test_file, et_test_line,
                   (et_test_msg[0] != '\0') ? et_test_msg : "(no detail)");
            fail++;
        }
    }

    printf("[Suite] %s => pass=%u fail=%u\n", suite, pass, fail);
    return (int)fail;
}
