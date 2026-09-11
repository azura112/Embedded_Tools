# API 清单 (自动生成, 勿手改)

> 由 `sh tools/apidump.sh` 生成; 一致性由 docsync `--check` 断言。
> 口径: 公开签名面 —— (et_|port_) 函数声明 / typedef / 公开 ET_/PORT_ 宏(含默认值, 注释剥离)。
> 冻结契约与上下文图例见 [API_STABILITY.md](API_STABILITY.md)。

## et_config.h

### 宏 (43)

- `ET_ASSERT(cond) ((void)0)`
- `ET_CRC_TABLE 0`
- `ET_DEPRECATED`
- `ET_DEPRECATED __attribute__((deprecated))`
- `ET_MEMPOOL_ALIGN ((uint32_t)sizeof(void *))`
- `ET_MEMPOOL_STRICT 1`
- `ET_MODULE_ATCMD 1`
- `ET_MODULE_BOOTCTL 1`
- `ET_MODULE_CRC 1`
- `ET_MODULE_EVENT 1`
- `ET_MODULE_FILTER 1`
- `ET_MODULE_FRAME 1`
- `ET_MODULE_FSM 1`
- `ET_MODULE_KEY 1`
- `ET_MODULE_KV 1`
- `ET_MODULE_LED 1`
- `ET_MODULE_LIST 1`
- `ET_MODULE_LOG 1`
- `ET_MODULE_MAP 1`
- `ET_MODULE_MEMPOOL 1`
- `ET_MODULE_QUEUE 1`
- `ET_MODULE_RINGBUF 1`
- `ET_MODULE_SCHED 1`
- `ET_MODULE_SELFTEST 0`
- `ET_MODULE_SHELL 1`
- `ET_MODULE_SMAP 1`
- `ET_MODULE_SOFTCLOCK 1`
- `ET_MODULE_SPWM 1`
- `ET_MODULE_STIMER 1`
- `ET_MODULE_WDT 1`
- `ET_MODULE_XMODEM 1`
- `ET_RINGBUF_POW2 0`
- `ET_SPWM_CH_MAX 4`
- `ET_VERSION ((ET_VERSION_MAJOR << 16) | (ET_VERSION_MINOR << 8) | (ET_VERSION_PATCH))`
- `ET_VERSION_MAJOR 2`
- `ET_VERSION_MINOR 0`
- `ET_VERSION_PATCH 0`
- `ET_VERSION_STR(x) ET_VERSION_STR_(x)`
- `ET_VERSION_STRING ET_VERSION_STR(ET_VERSION_MAJOR) "." ET_VERSION_STR(ET_VERSION_MINOR) "." ET_VERSION_STR(ET_VERSION_PATCH)`
- `ET_VERSION_STR_(x) #x`
- `PORT_FLASH_ERASE_MS_MAX 20u`
- `PORT_FLASH_SECTOR_COUNT 16u`
- `PORT_FLASH_SECTOR_SIZE 1024u`

## port/port.h

### 函数声明 (10)

- `bool port_flash_erase_sector(uint32_t sector_index)`
- `bool port_flash_read(uint32_t offset, void *buf, uint32_t len)`
- `bool port_wdt_disable(void)`
- `bool port_wdt_enable(uint32_t timeout_ms)`
- `port_tick_ms_t port_tick_get_ms(void)`
- `uint32_t port_flash_write(uint32_t offset, const void *buf, uint32_t len)`
- `void port_critical_enter(void)`
- `void port_critical_exit(void)`
- `void port_putc(char c)`
- `void port_wdt_feed(void)`

### 宏 (3)

- `PORT_CRITICAL_ENTER() port_critical_enter()`
- `PORT_CRITICAL_EXIT() port_critical_exit()`
- `PORT_TICK_WAIT_FOREVER 0xFFFFFFFFu`

## algorithm/et_filter.h

### 函数声明 (14)

- `bool et_lpf1_init(et_lpf1_t *f, uint16_t k_q15)`
- `bool et_movavg_init(et_movavg_t *f, int32_t *storage, uint32_t window)`
- `bool et_slew_init(et_slew_t *f, uint32_t max_step)`
- `int32_t et_lpf1_output(const et_lpf1_t *f)`
- `int32_t et_lpf1_update(et_lpf1_t *f, int32_t x)`
- `int32_t et_movavg_update(et_movavg_t *f, int32_t x)`
- `int32_t et_slew_output(const et_slew_t *f)`
- `int32_t et_slew_update(et_slew_t *f, int32_t x)`
- `uint32_t et_movavg_count(const et_movavg_t *f)`
- `uint32_t et_movavg_window(const et_movavg_t *f)`
- `void et_lpf1_reset(et_lpf1_t *f)`
- `void et_lpf1_set_k(et_lpf1_t *f, uint16_t k_q15)`
- `void et_movavg_reset(et_movavg_t *f)`
- `void et_slew_reset(et_slew_t *f)`

### 类型 (3)

- `et_lpf1_t`
- `et_movavg_t`
- `et_slew_t`

## algorithm/et_fsm.h

### 函数声明 (3)

- `bool et_fsm_dispatch(et_fsm_t *f, et_fsm_event_t ev)`
- `bool et_fsm_init(et_fsm_t *f, et_fsm_state_t init, const et_fsm_trans_t *table, uint32_t count, void *user)`
- `et_fsm_state_t et_fsm_state(const et_fsm_t *f)`

### 类型 (4)

- `et_fsm_event_t`
- `et_fsm_state_t`
- `et_fsm_t`
- `et_fsm_trans_t`

## core/et_list.h

### 函数声明 (10)

- `bool et_list_is_empty(const et_list_t *l)`
- `bool et_list_push_back(et_list_t *l, et_list_node_t *n)`
- `bool et_list_push_front(et_list_t *l, et_list_node_t *n)`
- `bool et_list_remove(et_list_t *l, et_list_node_t *n)`
- `et_list_node_t *et_list_back(const et_list_t *l)`
- `et_list_node_t *et_list_front(const et_list_t *l)`
- `uint32_t et_list_count(const et_list_t *l)`
- `void et_list_foreach(et_list_t *l, et_list_visit_fn fn, void *user)`
- `void et_list_init(et_list_t *l)`
- `void et_list_node_init(et_list_node_t *n)`

### 类型 (3)

- `et_list_node_t`
- `et_list_t`
- `et_list_visit_fn()`

### 宏 (1)

- `ET_LIST_CONTAINER(node_ptr, type, member) ((type *)((char *)(node_ptr) - offsetof(type, member)))`

## core/et_map.h

### 函数声明 (7)

- `bool et_map_del(et_map_t *m, uint32_t key)`
- `bool et_map_foreach(const et_map_t *m, et_map_visit_fn fn, void *user)`
- `bool et_map_get(const et_map_t *m, uint32_t key, uint32_t *val)`
- `bool et_map_init(et_map_t *m, et_map_slot_t *storage, uint32_t cap, uint32_t probe_limit)`
- `bool et_map_put(et_map_t *m, uint32_t key, uint32_t val)`
- `uint32_t et_map_count(const et_map_t *m)`
- `void et_map_clear(et_map_t *m)`

### 类型 (3)

- `et_map_slot_t`
- `et_map_t`
- `et_map_visit_fn()`

### 宏 (2)

- `ET_MAP_KEY_EMPTY 0u`
- `ET_MAP_KEY_TOMB 0xFFFFFFFFu`

## core/et_mempool.h

### 函数声明 (6)

- `bool et_mempool_contains(const et_mempool_t *mp, const void *ptr)`
- `bool et_mempool_free(et_mempool_t *mp, void *ptr)`
- `bool et_mempool_init(et_mempool_t *mp, void *storage, size_t storage_size, uint32_t block_size, uint32_t block_count)`
- `size_t et_mempool_bytes_needed(uint32_t block_size, uint32_t block_count)`
- `uint32_t et_mempool_free_count(const et_mempool_t *mp)`
- `void *et_mempool_alloc(et_mempool_t *mp)`

### 类型 (1)

- `et_mempool_t`

## core/et_queue.h

### 函数声明 (8)

- `bool et_queue_init(et_queue_t *q, void *storage, size_t storage_size, uint32_t item_size)`
- `bool et_queue_is_empty(const et_queue_t *q)`
- `bool et_queue_is_full(const et_queue_t *q)`
- `bool et_queue_pop(et_queue_t *q, void *item)`
- `bool et_queue_push(et_queue_t *q, const void *item)`
- `uint32_t et_queue_capacity(const et_queue_t *q)`
- `uint32_t et_queue_count(const et_queue_t *q)`
- `void et_queue_reset(et_queue_t *q)`

### 类型 (1)

- `et_queue_t`

## core/et_ringbuf.h

### 函数声明 (13)

- `bool et_ringbuf_init(et_ringbuf_t *rb, void *storage, uint32_t size)`
- `bool et_ringbuf_is_empty(const et_ringbuf_t *rb)`
- `bool et_ringbuf_is_full(const et_ringbuf_t *rb)`
- `const uint8_t *et_ringbuf_read_peek(const et_ringbuf_t *rb, uint32_t want, uint32_t *got)`
- `uint32_t et_ringbuf_free_space(const et_ringbuf_t *rb)`
- `uint32_t et_ringbuf_peek(const et_ringbuf_t *rb, void *out, uint32_t len)`
- `uint32_t et_ringbuf_read(et_ringbuf_t *rb, void *data, uint32_t len)`
- `uint32_t et_ringbuf_used(const et_ringbuf_t *rb)`
- `uint32_t et_ringbuf_write(et_ringbuf_t *rb, const void *data, uint32_t len)`
- `uint8_t *et_ringbuf_write_reserve(et_ringbuf_t *rb, uint32_t want, uint32_t *got)`
- `void et_ringbuf_drop(et_ringbuf_t *rb, uint32_t len)`
- `void et_ringbuf_reset(et_ringbuf_t *rb)`
- `void et_ringbuf_write_commit(et_ringbuf_t *rb, uint32_t len)`

### 类型 (1)

- `et_ringbuf_t`

## core/et_smap.h

### 函数声明 (11)

- `bool et_smap_del(et_smap_t *m, const char *key)`
- `bool et_smap_del_ci(et_smap_t *m, const char *key)`
- `bool et_smap_foreach(const et_smap_t *m, et_smap_visit_fn fn, void *user)`
- `bool et_smap_get(const et_smap_t *m, const char *key, uint32_t *val)`
- `bool et_smap_get_ci(const et_smap_t *m, const char *key, uint32_t *val)`
- `bool et_smap_init(et_smap_t *m, et_smap_slot_t *storage, uint32_t cap, uint8_t *keybuf, uint32_t keybuf_size, uint32_t probe_limit)`
- `bool et_smap_put(et_smap_t *m, const char *key, uint32_t val)`
- `bool et_smap_put_ci(et_smap_t *m, const char *key, uint32_t val)`
- `uint32_t et_smap_count(const et_smap_t *m)`
- `uint32_t et_smap_pool_free(const et_smap_t *m)`
- `void et_smap_clear(et_smap_t *m)`

### 类型 (3)

- `et_smap_slot_t`
- `et_smap_t`
- `et_smap_visit_fn()`

### 宏 (4)

- `ET_SMAP_KEY_MAX ET_SMAP_KEY_MAX_DEFAULT`
- `ET_SMAP_KEY_MAX_DEFAULT 16u`
- `ET_SMAP_KOFF_EMPTY 0u`
- `ET_SMAP_KOFF_TOMB 0xFFFFFFFFu`

## debug/et_assert.h

### 函数声明 (2)

- `void et_assert_fail(const char *file, int line, const char *expr)`
- `void et_assert_install(et_assert_fail_fn fn, void *user)`

### 类型 (1)

- `et_assert_fail_fn()`

### 宏 (2)

- `ET_DBG_ASSERT(cond) ((void)0)`
- `ET_DBG_ASSERT(cond) do { if (!(cond)) { et_assert_fail(__FILE__, __LINE__, #cond); } } while (0)`

## debug/et_log.h

### 函数声明 (5)

- `et_log_level_t et_log_get_level(void)`
- `int et_log_output(et_log_level_t lv, const char *tag, const char *fmt, ...)`
- `int et_log_raw(const char *fmt, ...)`
- `void et_log_hexdump(et_log_level_t lv, const char *tag, const void *data, uint32_t len)`
- `void et_log_set_level(et_log_level_t lv)`

### 类型 (1)

- `et_log_level_t`

### 宏 (17)

- `ET_LOGD(tag, ...) ((void)0)`
- `ET_LOGD(tag, ...) et_log_output(ET_LOG_LEVEL_DEBUG, (tag), __VA_ARGS__)`
- `ET_LOGE(tag, ...) ((void)0)`
- `ET_LOGE(tag, ...) et_log_output(ET_LOG_LEVEL_ERROR, (tag), __VA_ARGS__)`
- `ET_LOGI(tag, ...) ((void)0)`
- `ET_LOGI(tag, ...) et_log_output(ET_LOG_LEVEL_INFO, (tag), __VA_ARGS__)`
- `ET_LOGT(tag, ...) ((void)0)`
- `ET_LOGT(tag, ...) et_log_output(ET_LOG_LEVEL_TRACE, (tag), __VA_ARGS__)`
- `ET_LOGW(tag, ...) ((void)0)`
- `ET_LOGW(tag, ...) et_log_output(ET_LOG_LEVEL_WARN, (tag), __VA_ARGS__)`
- `ET_LOG_LEVEL_DEBUG 1`
- `ET_LOG_LEVEL_ERROR 4`
- `ET_LOG_LEVEL_INFO 2`
- `ET_LOG_LEVEL_NONE 5`
- `ET_LOG_LEVEL_TRACE 0`
- `ET_LOG_LEVEL_WARN 3`
- `ET_LOG_MAX_LEVEL ET_LOG_LEVEL_TRACE`

## debug/et_selftest.h

### 函数声明 (7)

- `bool et_selftest_register(const char *name, et_selftest_suite_fn fn)`
- `bool et_selftest_run_all(et_selftest_report_fn report, void *user)`
- `bool et_selftest_run_suite(const char *name, et_selftest_report_fn report, void *user)`
- `uint16_t et_selftest_suite_count(void)`
- `void et_selftest_note_fail(et_selftest_report_fn report, void *user, const char *suite, uint32_t line)`
- `void et_selftest_set_bootctl_cfg(const et_bootctl_cfg_t *cfg)`
- `void et_selftest_set_kv_layout(const et_kv_layout_t *lay)`

### 类型 (3)

- `et_selftest_evt_t`
- `et_selftest_report_fn()`
- `et_selftest_suite_fn()`

## debug/et_shell.h

### 函数声明 (10)

- `bool et_shell_feed(et_shell_t *sh, char ch)`
- `bool et_shell_init(et_shell_t *sh, et_atcmd_proc_t *at, et_shell_putc_fn putc, void *user)`
- `bool et_shell_set_history(et_shell_t *sh, char *storage, uint16_t entries, uint16_t entry_cap)`
- `void et_shell_help_cmd(char *args, void *user)`
- `void et_shell_print_help(et_shell_t *sh)`
- `void et_shell_prompt(et_shell_t *sh)`
- `void et_shell_puts(et_shell_t *sh, const char *s)`
- `void et_shell_set_echo(et_shell_t *sh, bool on)`
- `void et_shell_set_erase(et_shell_t *sh, bool on)`
- `void et_shell_set_prompt(et_shell_t *sh, const char *prompt)`

### 类型 (2)

- `et_shell_putc_fn()`
- `et_shell_t`

### 宏 (2)

- `ET_SHELL_HISTORY_N 0`
- `ET_SHELL_TAB 0`

## drivers/et_key.h

### 函数声明 (2)

- `bool et_key_init(et_key_t *k, et_key_read_fn read, et_key_event_fn on_event, void *user, const et_key_params_t *prm)`
- `void et_key_scan(et_key_t *k, uint32_t now)`

### 类型 (5)

- `et_key_event_fn()`
- `et_key_event_t`
- `et_key_params_t`
- `et_key_read_fn()`
- `et_key_t`

## drivers/et_led.h

### 函数声明 (6)

- `bool et_led_init(et_led_t *l, et_led_write_fn write, void *user)`
- `bool et_led_set_blink(et_led_t *l, uint32_t period_ms, uint8_t duty_pct, uint16_t times)`
- `bool et_led_set_breath(et_led_t *l, uint32_t period_ms)`
- `void et_led_poll(et_led_t *l, uint32_t now)`
- `void et_led_set_off(et_led_t *l)`
- `void et_led_set_on(et_led_t *l)`

### 类型 (3)

- `et_led_mode_t`
- `et_led_t`
- `et_led_write_fn()`

## drivers/et_spwm.h

### 函数声明 (6)

- `bool et_spwm_init(uint8_t ch, et_spwm_write_fn fn, void *user, uint16_t period_ms)`
- `bool et_spwm_set(uint8_t ch, uint8_t duty)`
- `uint16_t et_spwm_get_period(uint8_t ch)`
- `uint8_t et_spwm_get_duty(uint8_t ch)`
- `void et_spwm_deinit(uint8_t ch)`
- `void et_spwm_poll(uint32_t now)`

### 类型 (1)

- `et_spwm_write_fn()`

## protocol/et_atcmd.h

### 函数声明 (4)

- `bool et_atcmd_feed(et_atcmd_proc_t *p, char ch)`
- `bool et_atcmd_init(et_atcmd_proc_t *p, const et_atcmd_entry_t *cmds, uint16_t cmd_count, char *linebuf, uint16_t line_cap, void *user)`
- `char *et_atcmd_next_arg(char **cursor)`
- `void et_atcmd_reset(et_atcmd_proc_t *p)`

### 类型 (4)

- `et_atcmd_entry_t`
- `et_atcmd_fn()`
- `et_atcmd_proc_t`
- `et_atcmd_unknown_fn()`

## protocol/et_crc.h

### 函数声明 (8)

- `uint16_t et_crc16_ccitt(const void *data, uint32_t len)`
- `uint16_t et_crc16_ccitt_update(uint16_t crc, const void *data, uint32_t len)`
- `uint16_t et_crc16_modbus(const void *data, uint32_t len)`
- `uint16_t et_crc16_modbus_update(uint16_t crc, const void *data, uint32_t len)`
- `uint32_t et_crc32(const void *data, uint32_t len)`
- `uint32_t et_crc32_update(uint32_t crc, const void *data, uint32_t len)`
- `uint8_t et_crc8(const void *data, uint32_t len)`
- `uint8_t et_crc8_update(uint8_t crc, const void *data, uint32_t len)`

### 宏 (4)

- `ET_CRC16_CCITT_INIT 0xFFFFu`
- `ET_CRC16_MODBUS_INIT 0xFFFFu`
- `ET_CRC32_INIT 0xFFFFFFFFu`
- `ET_CRC8_INIT 0x00u`

## protocol/et_frame.h

### 函数声明 (5)

- `bool et_frame_feed(et_frame_parser_t *p, uint8_t byte)`
- `bool et_frame_parser_init(et_frame_parser_t *p, const et_frame_cfg_t *cfg)`
- `uint16_t et_frame_pack(const et_frame_cfg_t *cfg, const uint8_t *payload, uint16_t len, uint8_t *out, uint16_t out_cap)`
- `uint32_t et_frame_write(et_frame_parser_t *p, const uint8_t *data, uint32_t len)`
- `void et_frame_reset(et_frame_parser_t *p)`

### 类型 (4)

- `et_frame_cfg_t`
- `et_frame_crc_t`
- `et_frame_on_frame_fn()`
- `et_frame_parser_t`

## protocol/et_xmodem.h

### 函数声明 (4)

- `et_xm_act_t et_xmodem_rx(et_xmodem_t *x, uint8_t ch, uint32_t now)`
- `et_xm_act_t et_xmodem_rx_tick(et_xmodem_t *x, uint32_t now)`
- `uint16_t et_xmodem_crc16(const uint8_t *data, uint32_t len)`
- `void et_xmodem_rx_init(et_xmodem_t *x, uint8_t *buf, uint32_t cap, et_xm_sink_fn sink, void *user)`

### 类型 (3)

- `et_xm_act_t`
- `et_xm_sink_fn()`
- `et_xmodem_t`

### 宏 (13)

- `ET_XM_1K 0`
- `ET_XM_ACK_BYTE 0x06u`
- `ET_XM_BLK128 128u`
- `ET_XM_CAN_BYTE 0x18u`
- `ET_XM_CRC_CH_BYTE 0x43u`
- `ET_XM_EOT 0x04u`
- `ET_XM_HDR_RAW 4u`
- `ET_XM_NAK_BYTE 0x15u`
- `ET_XM_PAD_BYTE 0x1Au`
- `ET_XM_PROMPT_MS 1000u`
- `ET_XM_SILENCE_MS 10000u`
- `ET_XM_SOH 0x01u`
- `ET_XM_STX 0x02u`

## protocol/et_xmodem_tx.h

### 函数声明 (5)

- `bool et_xmodem_tx_aborted(const et_xmodem_tx_t *x)`
- `bool et_xmodem_tx_done(const et_xmodem_tx_t *x)`
- `bool et_xmodem_tx_init(et_xmodem_tx_t *x, const et_xmodem_tx_cfg_t *cfg)`
- `et_xm_act_t et_xmodem_tx_poll(et_xmodem_tx_t *x, uint8_t ch, uint32_t now)`
- `et_xm_act_t et_xmodem_tx_tick(et_xmodem_tx_t *x, uint32_t now)`

### 类型 (4)

- `et_xmodem_putc_fn()`
- `et_xmodem_src_fn()`
- `et_xmodem_tx_cfg_t`
- `et_xmodem_tx_t`

## storage/et_bootctl.h

### 函数声明 (8)

- `bool et_bootctl_abandon(et_bootctl_t *bc)`
- `bool et_bootctl_confirm(et_bootctl_t *bc, uint32_t slot)`
- `bool et_bootctl_init(et_bootctl_t *bc, const et_bootctl_cfg_t *cfg)`
- `bool et_bootctl_should_rollback(const et_bootctl_t *bc, uint32_t slot)`
- `bool et_bootctl_stage(et_bootctl_t *bc, uint32_t slot)`
- `bool et_bootctl_verify_image(et_bootctl_t *bc, uint32_t slot)`
- `uint32_t et_bootctl_boot_attempt(et_bootctl_t *bc, uint32_t slot)`
- `void et_bootctl_state(const et_bootctl_t *bc, et_bootctl_state_t *st)`

### 类型 (4)

- `et_boot_img_hdr_t`
- `et_bootctl_cfg_t`
- `et_bootctl_state_t`
- `et_bootctl_t`

### 宏 (4)

- `ET_BOOT_HDR_SIZE 32u`
- `ET_BOOT_HDR_VER 1u`
- `ET_BOOT_IMG_MAGIC 0x49425445u`
- `ET_BOOT_STATE_MAGIC 0x53425445u`

## storage/et_kv.h

### 函数声明 (10)

- `bool et_kv_commit(et_kv_t *kv)`
- `bool et_kv_del(et_kv_t *kv, uint16_t key)`
- `bool et_kv_format(et_kv_t *kv, const et_kv_layout_t *layout)`
- `bool et_kv_get(et_kv_t *kv, uint16_t key, void *buf, uint16_t cap, uint16_t *out_len)`
- `bool et_kv_init(et_kv_t *kv, const et_kv_layout_t *layout)`
- `bool et_kv_iter_init(const et_kv_t *kv, et_kv_iter_t *it)`
- `bool et_kv_iter_next(const et_kv_t *kv, et_kv_iter_t *it, uint16_t *key, uint16_t *len)`
- `bool et_kv_set(et_kv_t *kv, uint16_t key, const void *val, uint16_t len)`
- `uint16_t et_kv_size(et_kv_t *kv, uint16_t key)`
- `void et_kv_stats(et_kv_t *kv, et_kv_stats_t *st)`

### 类型 (4)

- `et_kv_iter_t`
- `et_kv_layout_t`
- `et_kv_stats_t`
- `et_kv_t`

### 宏 (2)

- `ET_KV_KEY_MAX ((uint16_t)0x7FFEu)`
- `ET_KV_VAL_MAX ((uint32_t)PORT_FLASH_SECTOR_SIZE - 24u)`

## sys/et_event.h

### 函数声明 (5)

- `uint32_t et_event_peek(const et_event_group_t *g)`
- `uint32_t et_event_wait_and_clear(et_event_group_t *g, uint32_t mask)`
- `void et_event_clear(et_event_group_t *g, uint32_t bits)`
- `void et_event_init(et_event_group_t *g)`
- `void et_event_set(et_event_group_t *g, uint32_t bits)`

### 类型 (1)

- `et_event_group_t`

## sys/et_sched.h

### 函数声明 (5)

- `bool et_sched_register(et_task_t *t, et_task_fn fn, void *arg, uint32_t period_ms)`
- `bool et_sched_unregister(et_task_t *t)`
- `port_tick_ms_t et_sched_next_due(void)`
- `void et_sched_poll_once(void)`
- `void et_sched_reset(void)`

### 类型 (2)

- `et_task_fn()`
- `et_task_t`

## sys/et_softclock.h

### 函数声明 (5)

- `bool et_softclock_get_datetime(const et_softclock_t *sc, et_datetime_t *dt)`
- `bool et_softclock_init(et_softclock_t *sc, uint32_t unix_sec)`
- `uint32_t et_softclock_unix(const et_softclock_t *sc)`
- `void et_softclock_poll(et_softclock_t *sc, uint32_t now_ms)`
- `void et_softclock_set_unix(et_softclock_t *sc, uint32_t unix_sec)`

### 类型 (2)

- `et_datetime_t`
- `et_softclock_t`

## sys/et_stimer.h

### 函数声明 (8)

- `bool et_stimer_init(et_stimer_t *t, et_stimer_fn cb, void *arg)`
- `bool et_stimer_is_running(const et_stimer_t *t)`
- `bool et_stimer_start_oneshot(et_stimer_t *t, uint32_t delay_ms)`
- `bool et_stimer_start_periodic(et_stimer_t *t, uint32_t period_ms)`
- `bool et_stimer_stop(et_stimer_t *t)`
- `port_tick_ms_t et_stimer_next_due(void)`
- `void et_stimer_poll(uint32_t now)`
- `void et_stimer_reset_all(void)`

### 类型 (2)

- `et_stimer_fn()`
- `et_stimer_t`

## sys/et_wdt.h

### 函数声明 (4)

- `bool et_wdt_disable(void)`
- `bool et_wdt_enable(uint32_t timeout_ms)`
- `bool et_wdt_guard(et_wdt_job_fn fn, void *user)`
- `void et_wdt_feed(void)`

### 类型 (1)

- `et_wdt_job_fn()`
