/**
 * @file    et_kv.c
 * @brief   flash 键值掉电存储实现
 *
 * 页格式 (小端字节序显式打包, 平台无关):
 *   [0 ] magic   u32  'ETKV'
 *   [4 ] seq     u32  页版本号, 每次页切换 +1
 *   [8 ] state   u32  0xFFFFFFFF=MOVING(搬迁中), 0x00000000=COMMITTED
 *   [12] hdr_crc u32  crc32(前 12B)
 *   [16] 记录区, 逐条追加:
 *         key u16 (bit15=tombstone) | len u16 | vcrc u32 = crc32(payload)
 *         payload ALIGN4(len) 字节, 尾部 0xFF 填充
 *
 * 一致性设计 (评审决议 A/B):
 *  - 记录先写头后写 payload: 追加中断必然 vcrc 校验失败, 读侧跳过即可;
 *  - 页头 state 置 COMMITTED 是页生效的最后一笔写: 搬迁中断的对页保持
 *    MOVING, init 弃用之, 源页全程不破坏;
 *  - init 扫描遇到不可解析记录(key/len 字段非法)判定脏尾, 自动搬迁修复;
 *    可解析但 CRC 失败的记录(位翻转)仅跳过, 不影响其余记录可见性;
 *  - 活跃页任何时刻都是 key 全集, 备用页数据永远可弃。
 */
#include "et_kv.h"
#include "port.h"
#include "et_crc.h"

#if ET_MODULE_KV

/* ---- 格式常量 ---- */
#define KV_MAGIC            0x564B5445u          /* 小端字节序下即 'E','T','K','V' */
#define KV_HDR_SIZE         16u
#define KV_REC_HDR_SIZE     8u
#define KV_STATE_COMMITTED  0x00000000u
#define KV_TOMBSTONE_BIT    0x8000u
#define KV_ALIGN4(x)        (((uint32_t)(x) + 3u) & ~3u)
#define KV_ALIGN8(x)        (((uint32_t)(x) + 7u) & ~7u)
/* 记录槽 = 记录头 + payload 向上对齐 8B。原因: G4 类 flash 以 64 位双字为
 * 编程粒度且每双字只允许编程一次 (目标非全 1 → PROGERR), 8B 槽保证所有
 * 写入起止的双字完整性 (F1 16 位粒度下仅多浪费 ≤4B/条)。
 * 详见 port/stm32g474/README.md "flash 约束"。 */
#define KV_REC_SLOT(len)    KV_ALIGN8(KV_REC_HDR_SIZE + (uint32_t)(len))

#define KV_PAGE_SIZE        ((uint32_t)PORT_FLASH_SECTOR_SIZE)
/* 单记录 payload 上限(对齐前): 公开宏 ET_KV_VAL_MAX(et_kv.h), 页容量 - 页头 - 记录头 */
#define KV_VAL_MAX          ((uint16_t)ET_KV_VAL_MAX)

/* ---- 字节打包(小端, 平台无关) ---- */
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* flash 区段分块拷贝(64B 栈块, 4B 对齐路径) */
static bool flash_copy(uint32_t dst_abs, uint32_t src_abs, uint32_t len)
{
    uint8_t  blk[64];
    uint32_t done = 0u;

    while (done < len) {
        uint32_t chunk = len - done;

        if (chunk > sizeof(blk)) {
            chunk = sizeof(blk);
        }
        if (!port_flash_read(src_abs + done, blk, chunk)) {
            return false;
        }
        if (port_flash_write(dst_abs + done, blk, chunk) != chunk) {
            return false;
        }
        done += chunk;
    }
    return true;
}

/* ---- 页探测 ---- */

typedef enum {
    KV_PG_EMPTY = 0,        /* 从未写过(页头全 0xFF)   */
    KV_PG_VALID,            /* magic/crc/state 三条件齐 */
    KV_PG_INVALID           /* 有数据但校验失败: 半写/半擦/坏页 */
} kv_page_state;

static kv_page_state page_probe(uint32_t sector, uint32_t *seq_out)
{
    uint8_t  raw[KV_HDR_SIZE];
    uint32_t magic, seq, state, crc;

    if (!port_flash_read(sector * KV_PAGE_SIZE, raw, KV_HDR_SIZE)) {
        return KV_PG_INVALID;
    }
    magic = rd32(raw);
    seq   = rd32(raw + 4);
    state = rd32(raw + 8);
    crc   = rd32(raw + 12);

    if ((magic == 0xFFFFFFFFu) && (seq == 0xFFFFFFFFu) &&
        (state == 0xFFFFFFFFu) && (crc == 0xFFFFFFFFu)) {
        return KV_PG_EMPTY;
    }
    if ((magic != KV_MAGIC) || (crc != et_crc32(raw, 8u)) ||
        (state != KV_STATE_COMMITTED)) {
        return KV_PG_INVALID;
    }
    *seq_out = seq;
    return KV_PG_VALID;
}

/* 页头拆成两笔 8B 双字写入 (G4 双字单次编程约束, 见 KV_REC_SLOT 注):
 *   DW0 = magic|seq   : 页被认领的标记, 先写;
 *   DW1 = state|crc   : 内容就绪后由 page_commit_state 一笔写入生效。
 * 两笔之间掉电 → DW1 保持全 1 → kv_page_state 判 INVALID 弃页,
 * 两阶段提交语义与旧版 (16B 页头 + 4B 提交字) 完全一致。 */
static bool page_write_header(uint32_t sector, uint32_t seq)
{
    uint8_t raw[8];

    wr32(raw, KV_MAGIC);
    wr32(raw + 4, seq);
    return port_flash_write(sector * KV_PAGE_SIZE, raw, 8u) == 8u;
}

/* DW1 = state(COMMITTED) + crc。crc 仅覆盖 magic|seq (稳定字段),
 * 与 kv_page_state 读侧布局一致: state @+8, crc @+12。 */
static bool page_commit_state(uint32_t sector, uint32_t seq)
{
    uint8_t  raw[8];
    uint32_t crc;

    crc = 0u;
    {
        uint8_t ms[8];

        wr32(ms, KV_MAGIC);
        wr32(ms + 4, seq);
        crc = et_crc32(ms, 8u);
    }
    wr32(raw, KV_STATE_COMMITTED);
    wr32(raw + 4, crc);
    return port_flash_write(sector * KV_PAGE_SIZE + 8u, raw, 8u) == 8u;
}

/* ---- 记录遍历 ---- */

typedef struct {
    uint16_t key;           /* 用户 key(不含 tombstone 位) */
    uint16_t len;
    uint32_t off;           /* 记录头在页内偏移 */
    bool     deleted;
    bool     crc_ok;        /* payload CRC 校验通过 */
} kv_recinfo_t;

typedef void (*kv_visit_fn)(const kv_recinfo_t *ri, void *user);

typedef struct {
    uint32_t end_off;       /* 写入区结束 = 下一记录写入偏移 */
    bool     dirty;         /* 脏尾: 末尾存在不可解析记录 */
    uint16_t rec_cnt;       /* 记录槽位数(含 CRC 失效槽位) */
} kv_scan_res_t;

/* 分块计算 payload CRC, 零大栈 */
static bool rec_crc_ok(uint32_t page_base, uint32_t payload_off, uint16_t len,
                       uint32_t vcrc)
{
    uint8_t  blk[64];
    uint32_t crc = ET_CRC32_INIT;
    uint32_t done = 0u;

    while (done < (uint32_t)len) {
        uint32_t chunk = (uint32_t)len - done;

        if (chunk > sizeof(blk)) {
            chunk = sizeof(blk);
        }
        if (!port_flash_read(page_base + payload_off + done, blk, chunk)) {
            return false;
        }
        crc = et_crc32_update(crc, blk, chunk);
        done += chunk;
    }
    return (crc ^ ET_CRC32_INIT) == vcrc;
}

static kv_scan_res_t page_scan(uint32_t sector, kv_visit_fn fn, void *user)
{
    kv_scan_res_t res;
    uint32_t      base = sector * KV_PAGE_SIZE;
    uint32_t      off  = KV_HDR_SIZE;

    res.end_off = KV_HDR_SIZE;
    res.dirty   = false;
    res.rec_cnt = 0u;

    while ((off + KV_REC_HDR_SIZE) <= KV_PAGE_SIZE) {
        uint8_t      hdr[KV_REC_HDR_SIZE];
        uint16_t     enc_key, len;
        uint32_t     vcrc;
        kv_recinfo_t ri;

        if (!port_flash_read(base + off, hdr, KV_REC_HDR_SIZE)) {
            res.dirty = true;
            break;
        }
        enc_key = (uint16_t)(hdr[0] | ((uint16_t)hdr[1] << 8));
        len     = (uint16_t)(hdr[2] | ((uint16_t)hdr[3] << 8));
        vcrc    = rd32(&hdr[4]);

        if ((enc_key == 0xFFFFu) && (len == 0xFFFFu) && (vcrc == 0xFFFFFFFFu)) {
            break;                              /* 未写区: 正常结束 */
        }
        if ((enc_key == 0xFFFFu) || (len == 0xFFFFu) || ((uint32_t)len > KV_VAL_MAX)) {
            res.dirty = true;                   /* 不可解析: 脏尾 */
            break;
        }

        ri.key     = (uint16_t)(enc_key & 0x7FFFu);
        ri.deleted = (enc_key & KV_TOMBSTONE_BIT) != 0u;
        ri.len     = len;
        ri.off     = off;
        ri.crc_ok  = rec_crc_ok(base, off + KV_REC_HDR_SIZE, len, vcrc);

        res.rec_cnt++;
        off += KV_REC_SLOT(len);
        res.end_off = off;
        if (fn != NULL) {
            fn(&ri, user);
        }
    }
    return res;
}

/* off 之后是否存在同 key 的有效记录(压实去重判定) */
typedef struct {
    uint16_t key;
    uint32_t after_off;
    bool     found;
} kv_newer_t;

static void newer_visit(const kv_recinfo_t *ri, void *user)
{
    kv_newer_t *n = (kv_newer_t *)user;

    if ((ri->off > n->after_off) && ri->crc_ok && (ri->key == n->key)) {
        n->found = true;                        /* 更新的版本或 tombstone 都算 */
    }
}

static bool rec_has_newer(uint32_t sector, const kv_recinfo_t *ri)
{
    kv_newer_t n;

    n.key       = ri->key;
    n.after_off = ri->off;
    n.found     = false;
    (void)page_scan(sector, newer_visit, &n);
    return n.found;
}

/* get/del/size 共用的"最新有效版本"查找(顺序扫, 后者覆盖前者) */
typedef struct {
    uint16_t key;
    bool     found;
    bool     deleted;
    uint16_t len;
    uint32_t off;
} kv_find_t;

static void find_visit(const kv_recinfo_t *ri, void *user)
{
    kv_find_t *f = (kv_find_t *)user;

    if (ri->crc_ok && (ri->key == f->key)) {
        f->found   = true;
        f->deleted = ri->deleted;
        f->len     = ri->len;
        f->off     = ri->off;
    }
}

static void kv_find_latest(const et_kv_t *kv, uint16_t key, kv_find_t *f)
{
    f->key     = key;
    f->found   = false;
    f->deleted = false;
    f->len     = 0u;
    f->off     = 0u;
    (void)page_scan(kv->act_sector, find_visit, f);
}

/* ---- 记录追加 ---- */

static bool kv_append(et_kv_t *kv, uint32_t dst_sector, uint32_t dst_off,
                      uint16_t key, bool tombstone, uint16_t len, const void *val)
{
    uint8_t  hdr[KV_REC_HDR_SIZE];
    uint32_t base = dst_sector * KV_PAGE_SIZE;
    uint32_t crc;
    uint16_t enc;
    uint32_t n8, tail;
    const uint8_t *p = (const uint8_t *)val;

    (void)kv;
    enc = key;
    if (tombstone) {
        enc = (uint16_t)(enc | KV_TOMBSTONE_BIT);
    }
    /* vcrc 仅覆盖有效字节; len==0 的空记录/tombstone 恒为 0 */
    crc = (len > 0u) ? et_crc32(p, len) : 0u;

    hdr[0] = (uint8_t)(enc & 0xFFu);
    hdr[1] = (uint8_t)((enc >> 8) & 0xFFu);
    hdr[2] = (uint8_t)(len & 0xFFu);
    hdr[3] = (uint8_t)((len >> 8) & 0xFFu);
    wr32(hdr + 4, crc);

    if (port_flash_write(base + dst_off, hdr, KV_REC_HDR_SIZE) != KV_REC_HDR_SIZE) {
        return false;
    }
    if (len == 0u) {
        return true;
    }

    /* payload: 起点 8B 对齐 (dst_off 为槽起点), 整双字分块直写;
     * 余量 (0/4/8B) 以 0xFF 补齐后单笔写入 —— 其伴随字是槽内 slack,
     * 必为未编程状态, 满足 G4 双字单次编程约束 */
    n8 = (uint32_t)len & ~7u;                   /* 整 8B 部分 */
    if (n8 > 0u) {
        if (port_flash_write(base + dst_off + KV_REC_HDR_SIZE, p, n8) != n8) {
            return false;
        }
    }
    tail = KV_ALIGN4((uint32_t)len) - n8;       /* 余量 0/4/8B */
    if (tail > 0u) {
        uint8_t t8[8] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu,
                          0xFFu, 0xFFu, 0xFFu, 0xFFu };
        uint32_t i;

        for (i = 0u; i < ((uint32_t)len - n8); i++) {
            t8[i] = p[n8 + i];
        }
        if (port_flash_write(base + dst_off + KV_REC_HDR_SIZE + n8,
                             t8, tail) != tail) {
            return false;
        }
    }
    return true;
}

/* ---- 压实(搬迁): 源页存活记录去重搬去对页 ---- */

typedef struct {
    uint32_t src_sector;
    uint32_t src_base;
    uint32_t dst_base;
    uint32_t dst_off;
    uint16_t moved;
    bool     fail;
} kv_migrate_t;

static void migrate_visit(const kv_recinfo_t *ri, void *user)
{
    kv_migrate_t *m = (kv_migrate_t *)user;
    uint32_t      rec_size;

    if (m->fail || (!ri->crc_ok) || ri->deleted ||
        rec_has_newer(m->src_sector, ri)) {
        return;                                 /* 坏记录/tombstone/旧版本: 不搬 */
    }
    rec_size = KV_REC_SLOT(ri->len);
    if (!flash_copy(m->dst_base + m->dst_off, m->src_base + ri->off, rec_size)) {
        m->fail = true;
        return;
    }
    m->dst_off += rec_size;
    m->moved++;
}

static bool kv_compact(et_kv_t *kv)
{
    kv_migrate_t  m;
    uint32_t      new_seq = kv->act_seq + 1u;
    uint32_t      tmp;

    /* 1. 擦对页(坏页/半擦页/旧页一并清零) */
    if (!port_flash_erase_sector(kv->alt_sector)) {
        return false;
    }
    if (kv->alt_sector == kv->sec_a) {
        kv->erase_cnt_a++;
    } else {
        kv->erase_cnt_b++;
    }

    /* 2. 写页头(seq+1, MOVING) */
    if (!page_write_header(kv->alt_sector, new_seq)) {
        return false;
    }

    /* 3. 搬迁存活记录(此段任意时刻掉电: 对页停在 MOVING, init 弃用) */
    m.src_sector = kv->act_sector;
    m.src_base   = kv->act_sector * KV_PAGE_SIZE;
    m.dst_base   = kv->alt_sector * KV_PAGE_SIZE;
    m.dst_off    = KV_HDR_SIZE;
    m.moved      = 0u;
    m.fail       = false;
    (void)page_scan(kv->act_sector, migrate_visit, &m);
    if (m.fail) {
        return false;                           /* 活跃未切换, 源页完好 */
    }

    /* 4. 置 COMMITTED —— 页生效的最后一笔写 */
    if (!page_commit_state(kv->alt_sector, new_seq)) {
        return false;
    }

    /* 5. 切换活跃页 */
    tmp            = kv->act_sector;
    kv->act_sector = kv->alt_sector;
    kv->alt_sector = tmp;
    kv->act_seq    = new_seq;
    kv->write_off  = m.dst_off;
    kv->record_cnt = m.moved;
    return true;
}

/* ---- 公开 API ---- */

static bool kv_layout_ok(const et_kv_layout_t *l)
{
    return (l != NULL) &&
           (l->sector_a < (uint32_t)PORT_FLASH_SECTOR_COUNT) &&
           (l->sector_b < (uint32_t)PORT_FLASH_SECTOR_COUNT) &&
           (l->sector_a != l->sector_b);
}

bool et_kv_init(et_kv_t *kv, const et_kv_layout_t *layout)
{
    kv_page_state pa, pb;
    uint32_t      seq_a = 0u, seq_b = 0u, seq;
    uint32_t      act;
    kv_scan_res_t scan;

    ET_ASSERT(kv != NULL);
    ET_ASSERT(kv_layout_ok(layout));
    if ((kv == NULL) || !kv_layout_ok(layout)) {
        return false;
    }

    kv->sec_a       = layout->sector_a;
    kv->sec_b       = layout->sector_b;
    kv->erase_cnt_a = 0u;
    kv->erase_cnt_b = 0u;

    pa = page_probe(layout->sector_a, &seq_a);
    pb = page_probe(layout->sector_b, &seq_b);

    if ((pa == KV_PG_VALID) && (pb == KV_PG_VALID)) {
        /* 双有效: seq 大者胜; 相等属异常(正常流程不会发生), 取 a */
        if (seq_b > seq_a) {
            act = layout->sector_b;
            seq = seq_b;
        } else {
            act = layout->sector_a;
            seq = seq_a;
        }
    } else if (pa == KV_PG_VALID) {
        act = layout->sector_a;
        seq = seq_a;
    } else if (pb == KV_PG_VALID) {
        act = layout->sector_b;
        seq = seq_b;
    } else if ((pa == KV_PG_EMPTY) && (pb == KV_PG_EMPTY)) {
        /* 首次上电: 空页无需擦除, 直接写页头并置 COMMITTED */
        if (!page_write_header(layout->sector_a, 1u)) {
            return false;
        }
        if (!page_commit_state(layout->sector_a, 1u)) {
            return false;
        }
        act = layout->sector_a;
        seq = 1u;
    } else {
        return false;                           /* 无有效页且有坏页: 由应用决定 format */
    }

    kv->act_sector = act;
    kv->act_seq    = seq;
    kv->alt_sector = (act == layout->sector_a) ? layout->sector_b : layout->sector_a;

    scan = page_scan(act, NULL, NULL);
    kv->write_off  = scan.end_off;
    kv->record_cnt = scan.rec_cnt;

    if (scan.dirty) {
        return kv_compact(kv);                  /* 脏尾: 自动搬迁修复 */
    }
    return true;
}

bool et_kv_format(et_kv_t *kv, const et_kv_layout_t *layout)
{
    ET_ASSERT(kv != NULL);
    ET_ASSERT(kv_layout_ok(layout));
    if ((kv == NULL) || !kv_layout_ok(layout)) {
        return false;
    }

    if (!port_flash_erase_sector(layout->sector_a) ||
        !port_flash_erase_sector(layout->sector_b)) {
        return false;
    }
    kv->sec_a       = layout->sector_a;
    kv->sec_b       = layout->sector_b;
    kv->erase_cnt_a = 1u;
    kv->erase_cnt_b = 1u;

    if (!page_write_header(layout->sector_a, 1u)) {
        return false;
    }
    if (!page_commit_state(layout->sector_a, 1u)) {
        return false;
    }
    kv->act_sector = layout->sector_a;
    kv->alt_sector = layout->sector_b;
    kv->act_seq    = 1u;
    kv->write_off  = KV_HDR_SIZE;
    kv->record_cnt = 0u;
    return true;
}

bool et_kv_set(et_kv_t *kv, uint16_t key, const void *val, uint16_t len)
{
    uint32_t rec_size;

    ET_ASSERT(kv != NULL);
    ET_ASSERT((key >= 1u) && (key <= ET_KV_KEY_MAX));
    ET_ASSERT((len == 0u) || (val != NULL));
    if (kv == NULL) {
        return false;
    }
    if ((key == 0u) || (key > ET_KV_KEY_MAX) ||
        ((len > 0u) && (val == NULL)) || ((uint32_t)len > KV_VAL_MAX)) {
        return false;
    }

    rec_size = KV_REC_SLOT(len);
    if ((kv->write_off + rec_size) > KV_PAGE_SIZE) {
        if (!kv_compact(kv)) {
            return false;
        }
        if ((kv->write_off + rec_size) > KV_PAGE_SIZE) {
            return false;                       /* 压实后仍放不下 */
        }
    }
    if (!kv_append(kv, kv->act_sector, kv->write_off, key, false, len, val)) {
        return false;
    }
    kv->write_off  += rec_size;
    kv->record_cnt++;
    return true;
}

bool et_kv_get(et_kv_t *kv, uint16_t key, void *buf, uint16_t cap,
               uint16_t *out_len)
{
    kv_find_t f;
    uint32_t  base;

    ET_ASSERT(kv != NULL);
    if (kv == NULL) {
        return false;
    }
    kv_find_latest(kv, key, &f);
    if ((!f.found) || f.deleted) {
        return false;
    }
    if (out_len != NULL) {
        *out_len = f.len;                       /* 缓冲不足时也给出真实长度 */
    }
    if (cap < f.len) {
        return false;
    }
    if (f.len > 0u) {
        base = kv->act_sector * KV_PAGE_SIZE;
        if (!port_flash_read(base + f.off + KV_REC_HDR_SIZE, buf, f.len)) {
            return false;
        }
    }
    return true;
}

bool et_kv_del(et_kv_t *kv, uint16_t key)
{
    kv_find_t f;
    uint32_t  rec_size = KV_REC_HDR_SIZE;

    ET_ASSERT(kv != NULL);
    if (kv == NULL) {
        return false;
    }
    kv_find_latest(kv, key, &f);
    if ((!f.found) || f.deleted) {
        return false;                           /* 不存在: 零写入 */
    }

    if ((kv->write_off + rec_size) > KV_PAGE_SIZE) {
        if (!kv_compact(kv)) {
            return false;
        }
    }
    if (!kv_append(kv, kv->act_sector, kv->write_off, key, true, 0u, NULL)) {
        return false;
    }
    kv->write_off  += rec_size;
    kv->record_cnt++;
    return true;
}

uint16_t et_kv_size(et_kv_t *kv, uint16_t key)
{
    kv_find_t f;

    if (kv == NULL) {
        return 0u;
    }
    kv_find_latest(kv, key, &f);
    if ((!f.found) || f.deleted) {
        return 0u;
    }
    return f.len;
}

bool et_kv_commit(et_kv_t *kv)
{
    ET_ASSERT(kv != NULL);
    if (kv == NULL) {
        return false;
    }
    return kv_compact(kv);
}

/* key_count 统计: 每条有效记录若是该 key 最新版本则计数 */
typedef struct {
    uint32_t act_sector;
    uint16_t count;
} kv_keycnt_t;

static void keycnt_visit(const kv_recinfo_t *ri, void *user)
{
    kv_keycnt_t *k = (kv_keycnt_t *)user;

    if (ri->crc_ok && (!ri->deleted) && !rec_has_newer(k->act_sector, ri)) {
        k->count++;
    }
}

void et_kv_stats(et_kv_t *kv, et_kv_stats_t *st)
{
    kv_keycnt_t kc;

    ET_ASSERT(kv != NULL);
    ET_ASSERT(st != NULL);
    if ((kv == NULL) || (st == NULL)) {
        return;
    }
    st->seq          = kv->act_seq;
    st->erase_cnt_a  = kv->erase_cnt_a;
    st->erase_cnt_b  = kv->erase_cnt_b;
    st->used_bytes   = kv->write_off;
    st->free_bytes   = KV_PAGE_SIZE - kv->write_off;
    st->record_count = kv->record_cnt;

    kc.act_sector = kv->act_sector;
    kc.count      = 0u;
    (void)page_scan(kv->act_sector, keycnt_visit, &kc);
    st->key_count = kc.count;
}

/* ===================== 枚举迭代 (v1.4) ===================== */

bool et_kv_iter_init(const et_kv_t *kv, et_kv_iter_t *it)
{
    if ((kv == NULL) || (it == NULL)) {
        return false;
    }
    if (kv->act_seq == 0u) {
        return false;               /* 未初始化/未格式化的零句柄 */
    }
    it->sector = kv->act_sector;    /* 快照: 固定活跃页扇区 */
    it->off    = KV_HDR_SIZE;
    return true;
}

bool et_kv_iter_next(const et_kv_t *kv, et_kv_iter_t *it,
                     uint16_t *key, uint16_t *len)
{
    uint32_t base;

    if ((kv == NULL) || (it == NULL) || (key == NULL)) {
        return false;
    }
    base = it->sector * KV_PAGE_SIZE;
    while ((it->off + KV_REC_HDR_SIZE) <= KV_PAGE_SIZE) {
        uint8_t      hdr[KV_REC_HDR_SIZE];
        uint16_t     enc_key, rlen;
        uint32_t     vcrc;
        kv_recinfo_t ri;

        if (!port_flash_read(base + it->off, hdr, KV_REC_HDR_SIZE)) {
            return false;
        }
        enc_key = (uint16_t)(hdr[0] | ((uint16_t)hdr[1] << 8));
        rlen    = (uint16_t)(hdr[2] | ((uint16_t)hdr[3] << 8));
        vcrc    = rd32(&hdr[4]);

        if ((enc_key == 0xFFFFu) && (rlen == 0xFFFFu) &&
            (vcrc == 0xFFFFFFFFu)) {
            return false;                       /* 未写区: 枚举结束 */
        }
        if ((enc_key == 0xFFFFu) || (rlen == 0xFFFFu) ||
            ((uint32_t)rlen > KV_VAL_MAX)) {
            return false;                       /* 脏尾: 终止 */
        }

        ri.key     = (uint16_t)(enc_key & 0x7FFFu);
        ri.deleted = (enc_key & KV_TOMBSTONE_BIT) != 0u;
        ri.len     = rlen;
        ri.off     = it->off;
        ri.crc_ok  = rec_crc_ok(base, it->off + KV_REC_HDR_SIZE, rlen, vcrc);

        it->off += KV_REC_SLOT(rlen);

        /* 仅输出: CRC 有效 + 非 tombstone + 无更新版本(去重) */
        if (ri.crc_ok && (!ri.deleted) && !rec_has_newer(it->sector, &ri)) {
            *key = ri.key;
            if (len != NULL) {
                *len = ri.len;
            }
            return true;
        }
    }
    return false;
}

#endif /* ET_MODULE_KV */
