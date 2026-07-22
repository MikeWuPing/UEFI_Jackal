/* NMI 帧序（Bank7.ASM:125-166 NMI_VECTOR）与游戏控制状态机。
   覆盖：GCS0-4 标题/attract（:625-920）、GCS5 的 GPM 分发（:3269-3283）、
   GPM0/1（:3410-3464）、GPM4-10 Chinook 与 InitializeLevel（:3285-3542）。
   GCS6-9 与 GPM2/3 为桩（Phase 6 / Task 2.8 实装），桩静默无副作用，
   迁移经 TRACE_STATE 可见。 */
#ifndef JACKAL_CORE_GAME_CONTROL_H
#define JACKAL_CORE_GAME_CONTROL_H

#include "input_def.h"

/* RESET_VECTOR（:47-114）：RAM 清零 + HighScore 初值 $5000 + InitPPU + StopMusic。
   :80-102 的 $07F0-$07FF 探测/填充段在反汇编注释中即"不知用途、他处不读写"，
   归约为无副作用不翻译。 */
void JackalReset(void);

/* NMI_VECTOR 帧序。in->Current = 本帧键盘映射的 NES 按键位（pad1；UEFI 单键盘，
   pad2 恒 0 传入）。 */
void JackalNmiFrame(const JACKAL_INPUT *in);

void subProcessGameControl(void);
void subTitleScreenInitialization(void);
void subEraseAllSpriteData(void);   /* :1497：$6A-$89、$0500-$06FF、$0700-$076B */
void subEraseInGameJeepData(void);  /* :1512：$0050-$00C1 */
void Label1011(void);               /* :3833 in-game pattern 流式更新（Task 6.4 实装） */
uint8_t label1007(void);            /* :3740 直升机坪灯光 palette 轮换（返 1=落 BG 动画/0=RTS） */
uint8_t fctCountDownScreenTimer(void);            /* :1146（end_of_game.c 共用） */
void subIncrementSubGameState_SetScreenTimeLB(uint8_t a); /* :1122（同上） */
void subTransitionFromEndOfGameToLevel1(void);  /* :1059（GCS7/结局共用） */

#endif
