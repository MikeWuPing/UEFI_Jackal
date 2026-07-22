/* NES 手柄位定义与帧输入包。按键掩码值同 Global.ASM:16-23（Right/Left/Down/Up
   = $01/$02/$04/$08，Start/Select/B/A = $10/$20/$40/$80）。
   纯 C99 头：core 与 host 测试直接 include；platform 层经 Jackal.h 间接引用。 */
#ifndef JACKAL_CORE_INPUT_DEF_H
#define JACKAL_CORE_INPUT_DEF_H

#include <stdint.h>

#define NES_BTN_RIGHT     0x01
#define NES_BTN_LEFT      0x02
#define NES_BTN_DOWN      0x04
#define NES_BTN_UP        0x08
#define NES_BTN_START     0x10
#define NES_BTN_SELECT    0x20
#define NES_BTN_B         0x40
#define NES_BTN_A         0x80

typedef struct {
  uint8_t Current;   /* 本帧按住（NES 按键位）——core 只消费本字段 */
  uint8_t Pressed;   /* 本帧新按下（platform 辅助量；core 自行边沿检测，不读） */
  uint8_t Escape;    /* ESC 扫描码（platform 层用，core 不消费） */
} JACKAL_INPUT;

#endif
