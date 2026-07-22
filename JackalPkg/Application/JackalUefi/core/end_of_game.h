/* core/end_of_game.h：GCS9 结局（Bank0:2329-2680 subProcessEndOfGameLogic）。
   12 子状态机：State0-3（横滚收拢+congrats 文本）、State4-6（EOG 场景+heli 入场）、
   State7-11（credits 流 + Start 回 Level 1 HARD 模式）。零页借用（结局上下文）：
   $50/$52=文本指针（JeepMainWeapon/JeepIFrameTimer 复用）、$56/$58=行 PPU 地址
   （JeepHorizPosition/JeepVertPosition 复用）、$5A=文本游标（JeepAtHelipadDropoff）、
   $54=帧计时（JeepPOWCount）、$60=闪烁计数（JeepPOWHeliDropOffCount）、
   $5E=三角标 palette（JeepPOWDropoffDelay）、$066F=State1 计时、$0660=heli 终 TypeIndex。 */
#ifndef JACKAL_CORE_END_OF_GAME_H
#define JACKAL_CORE_END_OF_GAME_H

#include <stdint.h>

void subProcessEndOfGameLogic(void);   /* Bank0:2329-2344（dw 12 项分发） */

#endif
