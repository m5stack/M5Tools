// ESP32 (無印) で IRAM 最終リージョン (0x4009e000-0x400a0000) のコード配置後の
// 端数がちょうど 212〜219 バイトになると、heap_caps_init が tlsf の境界バグ
// (control_construct が pool_overhead 8 バイトを見込まない) を踏み、
// tlsf_add_pool のエラー printf → stdio 初期化 → ヒープ未登録中の malloc 失敗
// → abort で起動不能になる (リンク結果依存で確率的に発生)。
// IRAM 末尾の端数をヒープ登録から除外して恒久回避する。
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32
#include "soc/soc.h"
#include "heap_memory_layout.h"

extern char _iram_end[];
SOC_RESERVE_MEMORY_REGION((intptr_t)_iram_end, SOC_IRAM_HIGH, iram_tail_unused);
#endif
