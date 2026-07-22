/* core/jeep.h：吉普控制与 BG 碰撞（Bank7.ASM:4440-4581、5364-5910）。
   JeepState 分派：0=Dead/1=Normal/2=Collision/3=Exploding/4=WaitingToRespawn；
   本任务实装 0/1 与移动本体，2/3/4（死亡/爆炸/重生）Task 3.9。 */
#ifndef JACKAL_CORE_JEEP_H
#define JACKAL_CORE_JEEP_H

#include <stdint.h>

/* fctGetCollisionTypeFromRAM300_4FF（:4470）：探测点=精灵位置+速度+$16/$17 偏移，
   $0300/$0400 碰撞图（1 字节=4 个 2-bit tile）查 2-bit 碰撞类型（3=水）；X 经 $39 槽基偏移 */
uint8_t fctGetCollisionTypeFromRAM300_4FF(uint8_t x);
/* Label1210（:4462）：$16/$17 清零 + X 直接取 $39 槽基（不叠加） */
uint8_t Label1210(uint8_t x);
/* fctTestForMovementCollision（:4552）：AbsVertUB!=0→0；$12 选页（0=$0300/否=$0400）、
   $11=X、$14=Y 传试探点（Label244 与敌人 AI 共用） */
uint8_t fctTestForMovementCollision(uint8_t x);

/* subProcessJeepState（:5364）：JeepState 函数指针分派（ASM dw 顺序） */
void subProcessJeepState(uint8_t x);

/* Label995（:4964）：GPM3 吉普主逻辑入口。ASM 全量=双吉普 subProcessDeadJeep
   （死亡/借命，Task 3.9）+ 单/双人滚动协同；单人段 Label1096 已实装（Task 4.5），
   双人段（:4972-5252）Phase 6 实装。 */
void Label995(void);

/* Label1096（:5156-5245）：单人滚动协同（吉普近屏边滚动/归位 + screen 推进/回退） */
void Label1096(void);

#endif
