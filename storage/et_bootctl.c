/**
 * @file    et_bootctl.c
 * @brief   安全升级控制实现 (镜像头校验 + append-only 状态记录)
 *
 * 可靠性模型见 et_bootctl.h 头注: append-only 记录 + val/inv 双字互斥校验,
 * 任意断电只丢最后一步; 状态头 CRC 坏 → init 自愈重建。
 */
#include "et_bootctl.h"
#include "et_crc.h"
#include "port.h"

#if ET_MODULE_BOOTCTL

/* 状态扇区几何 */
#define ST_HDR_SIZE         16u                 /* magic(4)+ver/rsvd(4)+crc(4)+rsvd(4)
                                                 * 8B 对齐: G4 双字单次编程约束 (同 et_kv) */
#define ST_REC_SIZE         8u                  /* val + inv */

/* 事件 val 编码 (ASCII 标识 + 槽号) */
#define EV_STAGE            0x53544700u         /* 'STG' */
#define EV_CONFIRM          0x434E4600u         /* 'CNF' */
#define EV_ATTEMPT          0x41545400u         /* 'ATT' */

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t state_base(const et_bootctl_t *bc)
{
    return bc->cfg.state_sector * (uint32_t)PORT_FLASH_SECTOR_SIZE;
}

static uint32_t slot_base(const et_bootctl_t *bc, uint32_t slot)
{
    return bc->cfg.slot_sector[slot] * (uint32_t)PORT_FLASH_SECTOR_SIZE;
}

/* ---------------- 状态扇区头 ---------------- */

static bool state_hdr_valid(const et_bootctl_t *bc)
{
    uint8_t raw[ST_HDR_SIZE];

    if (!port_flash_read(state_base(bc), raw, ST_HDR_SIZE)) {
        return false;
    }
    if ((rd32(raw) != ET_BOOT_STATE_MAGIC) || (raw[4] != 1u) ||
        (raw[5] != 0u) || (raw[6] != 0u) || (raw[7] != 0u)) {
        return false;
    }
    return rd32(&raw[8]) == et_crc32(raw, 8u);
}

static bool state_hdr_write(et_bootctl_t *bc)
{
    uint8_t raw[ST_HDR_SIZE];

    wr32(raw, ET_BOOT_STATE_MAGIC);
    raw[4] = 1u;                            /* ver */
    raw[5] = 0u;
    raw[6] = 0u;
    raw[7] = 0u;
    wr32(&raw[8], et_crc32(raw, 8u));
    raw[12] = 0xFFu;                        /* 尾部保留区保持擦除态, 1→0 扩展 */
    raw[13] = 0xFFu;
    raw[14] = 0xFFu;
    raw[15] = 0xFFu;
    return port_flash_write(state_base(bc), raw, ST_HDR_SIZE) == ST_HDR_SIZE;
}

/* ---------------- 记录扫描 ---------------- */

typedef enum {
    EV_NONE = 0,
    EV_R_STAGE,
    EV_R_CONFIRM,
    EV_R_ATTEMPT
} ev_kind_t;

typedef struct {
    ev_kind_t kind;
    uint32_t  slot;
} ev_rec_t;

/* 解析一条记录头; 有效返回 true (kind/slot 填充) */
static bool rec_parse(const uint8_t *raw, ev_rec_t *ev)
{
    uint32_t val = rd32(raw);
    uint32_t inv = rd32(raw + 4u);

    if ((val == 0xFFFFFFFFu) && (inv == 0xFFFFFFFFu)) {
        return false;                       /* 未写区 */
    }
    if (inv != ~val) {
        return false;                       /* 半写/损坏 */
    }
    switch (val & 0xFFFFFF00u) {
    case EV_STAGE:   ev->kind = EV_R_STAGE;   break;
    case EV_CONFIRM: ev->kind = EV_R_CONFIRM; break;
    case EV_ATTEMPT: ev->kind = EV_R_ATTEMPT; break;
    default:
        return false;                       /* 未知事件: 视为脏尾终止 */
    }
    ev->slot = val & 0xFFu;
    return (ev->slot <= 1u);
}

/* 遍历记录区: 首条无效记录即停 (append-only 顺序保证); 统计最新状态。
 * 返回 true = 停在半写记录上 (需修复重放后才能继续追加) */
static bool state_scan(et_bootctl_t *bc, et_bootctl_state_t *st)
{
    uint8_t      raw[ST_REC_SIZE];
    uint32_t     off = ST_HDR_SIZE;
    const uint32_t limit = (uint32_t)PORT_FLASH_SECTOR_SIZE;
    int32_t      staged = -1, confirmed = -1;
    uint32_t     attempts = 0u;
    bool         dirty = false;

    while ((off + ST_REC_SIZE) <= limit) {
        ev_rec_t ev;

        if (!port_flash_read(state_base(bc) + off, raw, ST_REC_SIZE)) {
            break;
        }
        if ((rd32(raw) == 0xFFFFFFFFu) && (rd32(raw + 4u) == 0xFFFFFFFFu)) {
            break;                          /* 未写区: 干净结束 */
        }
        if (!rec_parse(raw, &ev)) {
            dirty = true;                   /* 半写/损坏记录: 脏尾待修复 */
            break;
        }
        switch (ev.kind) {
        case EV_R_STAGE:
            staged = (int32_t)ev.slot;      /* 最新 STG 生效 */
            attempts = 0u;                  /* 新一轮: 计数清零 */
            break;
        case EV_R_CONFIRM:
            confirmed = (int32_t)ev.slot;
            break;
        case EV_R_ATTEMPT:
            if (((int32_t)ev.slot == staged) && (confirmed < 0)) {
                attempts++;                 /* 仅 staged 未确认阶段计数 */
            }
            break;
        default:
            break;
        }
        off += ST_REC_SIZE;
    }
    bc->rec_off = off;                      /* 追加游标 = 首条无效记录处 */
    bc->dirty_log = dirty;                  /* 脏尾: 追加前须修复重放 */

    if (st != NULL) {
        st->staged_slot    = staged;
        st->confirmed_slot = confirmed;
        st->attempts       = attempts;
        st->state_ok       = state_hdr_valid(bc);
    }
    return bc->dirty_log;
}

/* 追加一条事件记录; 空间不足返回 false */
static bool rec_append(et_bootctl_t *bc, uint32_t val)
{
    uint8_t raw[ST_REC_SIZE];

    if ((bc->rec_off + ST_REC_SIZE) > (uint32_t)PORT_FLASH_SECTOR_SIZE) {
        return false;                       /* 记录区满: abandon 重建 */
    }
    wr32(raw, val);
    wr32(raw + 4u, ~val);
    if (port_flash_write(state_base(bc) + bc->rec_off, raw, ST_REC_SIZE) !=
        ST_REC_SIZE) {
        return false;
    }
    bc->rec_off += ST_REC_SIZE;         /* 追加成功即推进游标 */
    return true;
}

/* 修复脏记录区: 从有效前缀取快照 → 擦状态扇区 → 重放头+记录。
 * 半写记录槽位非 0xFF, 不重放则后续追加会触发 1→0 写违约。
 * 修复期间再断电: 最多回退到已重放的步 (状态机仍单调可恢复)。 */
static bool state_repair(et_bootctl_t *bc)
{
    et_bootctl_state_t snap;
    uint32_t           i;

    state_scan(bc, &snap);                  /* 先取有效前缀快照 */
    if (!port_flash_erase_sector(bc->cfg.state_sector)) {
        return false;
    }
    if (!state_hdr_write(bc)) {
        return false;
    }
    bc->rec_off   = ST_HDR_SIZE;
    bc->dirty_log = false;
    if (snap.staged_slot >= 0) {
        if (!rec_append(bc, EV_STAGE | (uint32_t)snap.staged_slot)) {
            return false;
        }
    }
    if (snap.confirmed_slot >= 0) {
        if (!rec_append(bc, EV_CONFIRM | (uint32_t)snap.confirmed_slot)) {
            return false;
        }
    }
    for (i = 0u; i < snap.attempts; i++) {
        if (snap.staged_slot < 0) {
            break;
        }
        if (!rec_append(bc, EV_ATTEMPT | (uint32_t)snap.staged_slot)) {
            return false;
        }
    }
    return true;
}

/* 读侧快照 (内部用): 扫描 + 必要时修复 */
static void bc_state(const et_bootctl_t *bc, int32_t *staged,
                     int32_t *confirmed, uint32_t *attempts)
{
    /* 扫描/修复需要更新游标: 去掉 const 限定 (游标属内部状态) */
    et_bootctl_state_t tmp;
    et_bootctl_t *m = (et_bootctl_t *)bc;

    if (bc->inited && bc->dirty_log) {
        (void)state_repair(m);
    }
    state_scan(m, &tmp);
    if (staged != NULL)    { *staged = tmp.staged_slot; }
    if (confirmed != NULL) { *confirmed = tmp.confirmed_slot; }
    if (attempts != NULL)  { *attempts = tmp.attempts; }
}

/* ---------------- 公开 API ---------------- */

bool et_bootctl_init(et_bootctl_t *bc, const et_bootctl_cfg_t *cfg)
{
    if ((bc == NULL) || (cfg == NULL)) {
        return false;
    }
    if ((cfg->state_sector >= PORT_FLASH_SECTOR_COUNT) ||
        (cfg->slot_sector[0] >= PORT_FLASH_SECTOR_COUNT) ||
        (cfg->slot_sector[1] >= PORT_FLASH_SECTOR_COUNT) ||
        (cfg->slot_sector[0] == cfg->slot_sector[1]) ||
        (cfg->slot_sector[0] == cfg->state_sector) ||
        (cfg->slot_sector[1] == cfg->state_sector) ||
        (cfg->slot_size < ET_BOOT_HDR_SIZE + 1u) ||
        (cfg->max_attempts == 0u)) {
        return false;                       /* cfg 非法 */
    }
    bc->cfg     = *cfg;
    bc->inited  = false;

    /* 状态扇区自愈: 头无效 (空/半写/坏 CRC) → 擦除重建 */
    if (!state_hdr_valid(bc)) {
        if (!port_flash_erase_sector(bc->cfg.state_sector)) {
            return false;
        }
        if (!state_hdr_write(bc)) {
            return false;
        }
    }
    /* 记录区脏尾自愈: 半写记录槽非 0xFF, 修复重放后才能继续追加 */
    if (state_scan(bc, NULL) && !state_repair(bc)) {
        return false;
    }
    bc->inited = true;
    return true;
}

bool et_bootctl_verify_image(et_bootctl_t *bc, uint32_t slot)
{
    uint8_t  raw[ET_BOOT_HDR_SIZE];
    uint8_t  blk[64];
    uint32_t base, crc, done, img_size;

    if ((bc == NULL) || (!bc->inited) || (slot > 1u)) {
        return false;
    }
    base = slot_base(bc, slot);
    if (!port_flash_read(base, raw, ET_BOOT_HDR_SIZE)) {
        return false;
    }
    if ((rd32(raw) != ET_BOOT_IMG_MAGIC) ||
        (raw[4] != (uint8_t)ET_BOOT_HDR_VER) ||
        ((uint16_t)(raw[6] | ((uint16_t)raw[7] << 8)) != ET_BOOT_HDR_SIZE) ||
        (rd32(&raw[24]) != 0u)) {
        return false;                       /* magic/版本/尺寸/保留域 */
    }
    if (rd32(&raw[28]) != et_crc32(raw, 28u)) {
        return false;                       /* 头 CRC */
    }
    img_size = rd32(&raw[8]);
    if ((img_size == 0u) ||
        (img_size > bc->cfg.slot_size - ET_BOOT_HDR_SIZE)) {
        return false;                       /* 尺寸合法域 */
    }

    /* 全镜像 CRC: 分块流式 */
    crc = ET_CRC32_INIT;
    done = 0u;
    while (done < img_size) {
        uint32_t chunk = img_size - done;

        if (chunk > sizeof(blk)) {
            chunk = sizeof(blk);
        }
        if (!port_flash_read(base + ET_BOOT_HDR_SIZE + done, blk, chunk)) {
            return false;
        }
        crc = et_crc32_update(crc, blk, chunk);
        done += chunk;
    }
    return (crc ^ ET_CRC32_INIT) == rd32(&raw[12]);   /* img_crc32 @0x0C */
}

bool et_bootctl_stage(et_bootctl_t *bc, uint32_t slot)
{
    int32_t staged, confirmed;

    if ((bc == NULL) || (!bc->inited) || (slot > 1u)) {
        return false;
    }
    bc_state(bc, &staged, &confirmed, NULL);
    if (confirmed >= 0) {
        return false;                       /* 已确认轮次: 先 abandon */
    }
    if (staged >= 0) {
        if (staged == (int32_t)slot) {
            return true;                    /* 幂等: 不重复写记录 */
        }
        return false;                       /* 双槽互斥: 换槽先 abandon */
    }
    if (!rec_append(bc, EV_STAGE | slot)) {
        return false;
    }
    state_scan(bc, NULL);                   /* 刷新游标 */
    return true;
}

bool et_bootctl_confirm(et_bootctl_t *bc, uint32_t slot)
{
    int32_t staged, confirmed;

    if ((bc == NULL) || (!bc->inited) || (slot > 1u)) {
        return false;
    }
    bc_state(bc, &staged, &confirmed, NULL);
    if ((staged != (int32_t)slot) || (confirmed >= 0)) {
        return false;                       /* 未 staged / 已确认 */
    }
    if (!rec_append(bc, EV_CONFIRM | slot)) {
        return false;
    }
    state_scan(bc, NULL);
    return true;
}

uint32_t et_bootctl_boot_attempt(et_bootctl_t *bc, uint32_t slot)
{
    int32_t  staged, confirmed;
    uint32_t attempts;

    if ((bc == NULL) || (!bc->inited) || (slot > 1u)) {
        return 0u;
    }
    bc_state(bc, &staged, &confirmed, &attempts);
    if ((staged != (int32_t)slot) || (confirmed >= 0)) {
        return 0u;                          /* 仅 staged 未确认槽计数 */
    }
    if (!rec_append(bc, EV_ATTEMPT | slot)) {
        return attempts;                    /* 满时返回现值, 不夸大 */
    }
    attempts++;
    state_scan(bc, NULL);
    return attempts;
}

bool et_bootctl_should_rollback(const et_bootctl_t *bc, uint32_t slot)
{
    int32_t  staged, confirmed;
    uint32_t attempts;

    if ((bc == NULL) || (!bc->inited) || (slot > 1u)) {
        return false;
    }
    bc_state(bc, &staged, &confirmed, &attempts);
    return ((staged == (int32_t)slot) && (confirmed < 0) &&
            (attempts >= (uint32_t)bc->cfg.max_attempts));
}

void et_bootctl_state(const et_bootctl_t *bc, et_bootctl_state_t *st)
{
    if ((bc == NULL) || (st == NULL) || (!bc->inited)) {
        return;
    }
    state_scan((et_bootctl_t *)bc, st);
}

bool et_bootctl_abandon(et_bootctl_t *bc)
{
    if ((bc == NULL) || (!bc->inited)) {
        return false;
    }
    if (!port_flash_erase_sector(bc->cfg.state_sector)) {
        return false;
    }
    if (!state_hdr_write(bc)) {
        return false;
    }
    state_scan(bc, NULL);
    return true;
}

#endif /* ET_MODULE_BOOTCTL */
