/* core/interact.h：吉普-对象交互（Bank7.ASM:4762-4928 Label1005）。
   得分复用 weapon.c 的 subGetObjectPointsValue_AddToPlayerScore（Task 3.7）。 */
#ifndef JACKAL_CORE_INTERACT_H
#define JACKAL_CORE_INTERACT_H

#include <stdint.h>

/* Label1005（:4762）：敌槽 X（label979 $35 游标语义）与双吉普（Y=1,0）碰撞分发：
   步兵碾压/POW/helipad/三种星/电扶梯/碰撞致死 */
void Label1005(uint8_t x);

#endif
