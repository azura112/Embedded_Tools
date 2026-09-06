/**
 * @file    xmodem_host_recv.c
 * @brief   host 端到端自测: stdin 字节流(由 tools/xmodem_send.py --emit 产出)
 *          喂入 et_xmodem 接收器, 镜像写 argv[1], 与原文件比对即闭环
 *
 * 编译(库根目录):
 *   gcc -std=c99 -Wall -Wextra -I. -Iprotocol -Isys -Icore -Idrivers -Idebug -Istorage \
 *       -o build/xmrecv tools/xmodem_host_recv.c \
 *       protocol/et_xmodem.c protocol/et_crc.c core/et_ringbuf.c
 *   (1K 大块自测加 -DET_XM_1K=1)
 *
 * 用法: build/xmrecv <输出镜像> < 块流 ; 退出码 0 = DONE 且字节数与流一致
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "et_xmodem.h"
#include "et_crc.h"

#define RECV_BUF_CAP    (132u + 1024u)      /* 兼容 1K 大块 */

static FILE    *g_out;
static uint32_t g_total;

static bool sink(void *user, uint32_t off, const uint8_t *d, uint32_t len)
{
    (void)user;
    if (off != g_total) {                   /* 只允许顺序写 */
        return false;
    }
    if (fwrite(d, 1u, len, g_out) != len) {
        return false;
    }
    g_total += len;
    return true;
}

int main(int argc, char **argv)
{
    static et_xmodem_t xm;
    static uint8_t     buf[RECV_BUF_CAP];
    uint32_t           now = 0u;
    int                ch;
    et_xm_act_t        act = ET_XM_ERR;
    int                done = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: xmrecv <out.bin> < stream.bin\n");
        return 2;
    }
#ifdef _WIN32
    /* 二进制 stdin: 流含 0x1A(尾块填充), 文本模式会误判为 EOF */
    (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
    g_out = fopen(argv[1], "wb");
    if (g_out == NULL) {
        perror("fopen");
        return 2;
    }

    et_xmodem_rx_init(&xm, buf, sizeof(buf), sink, NULL);
    while ((ch = getchar()) != EOF) {
        act = et_xmodem_rx(&xm, (uint8_t)ch, now);
        now += 10u;                         /* 字节间隔 10ms(远小于静默阈值) */
        if ((getenv("XMTRACE") != NULL) && (act != ET_XM_IDLE)) {
            fprintf(stderr, "[xmtrace] byte#%lu act=%d total=%u expect=%u\n",
                    (unsigned long)now / 10u, (int)act,
                    (unsigned)xm.total, (unsigned)xm.expect);
        }
        if ((act == ET_XM_DONE) || (act == ET_XM_CAN) || (act == ET_XM_ERR)) {
            done = (act == ET_XM_DONE);
            break;
        }
    }

    fclose(g_out);
    if (!done) {
        fprintf(stderr, "FAIL: session ended with act=%d\n", (int)act);
        return 1;
    }
    printf("recv ok: %u bytes\n", (unsigned)g_total);
    return 0;
}
