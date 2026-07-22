#ifndef JACKAL_H
#define JACKAL_H

/* NES_BTN_* 与 JACKAL_INPUT 已移入纯 C 头 core/input_def.h（Task 2.6），
   本文件保留屏幕/帧率常量并转引，platform 层 include 路径不变。 */
#include "core/input_def.h"

#define JACKAL_SCREEN_W   256
#define JACKAL_SCREEN_H   240
#define JACKAL_SCALE      2
#define JACKAL_FPS        60

#endif
