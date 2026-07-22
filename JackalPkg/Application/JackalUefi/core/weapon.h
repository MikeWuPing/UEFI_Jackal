/* core/weapon.h：吉普武器系统（Bank7.ASM:4583-4759、5313-5362、5911-6425、1539-1637、1689-1757）。
   槽位语义：子弹 X=0-5（sprite 槽 $12-$17，P1=0-2/P2=3-5）；
   主武器 X=0-7（sprite 槽 $18-$1F：0/1=主武器、2/3=H splash、4/5=V splash）；
   GPM3 循环以 Raw[0x39] 同步槽基（$17-$12 子弹段 / $1F-$18 武器段）。 */
#ifndef JACKAL_CORE_WEAPON_H
#define JACKAL_CORE_WEAPON_H

#include <stdint.h>

/* subProcessDeadJeep 射击段（:5313-5362）：JeepState>=2 → 直接 subProcessJeepState；
   B 键子弹（边沿或按住+RNG&$1F==0）、A 键主武器（仅边沿、State==0 门控）；
   死亡处理/借命段（:5253-5312）Task 3.9 并入完整 subProcessDeadJeep。 */
void subProcessJeepFireButtons(uint8_t x);

void subProcessJeepBullet(uint8_t x);       /* :5911（X=子弹索引 0-5） */
void subProcessJeepMainWeapon(uint8_t x);   /* :5969（X=武器索引 0-7，分派表 12 项） */

void Label1001(uint8_t x);                  /* :4583 子弹碰撞（X=子弹索引） */
void Label996(uint8_t x);                   /* :4648 主武器碰撞（X=武器索引） */

/* 得分（:4930 subGetObjectPointsValue 内联化）：EnemyPoints*2 查 tblPointValues 加 BCD 分 */
void subGetObjectPointsValue_AddToPlayerScore(uint8_t player, uint8_t enemySlot);
/* subAddToPlayerScore（:1539）：add3={低,中,高} BCD 字节；TitleScreenMode!=0 不加 */
void subAddToPlayerScore(uint8_t player, const uint8_t add3[3]);

/* Label1055（:1750）：$39 槽 State/TypeIndex 清 0（消弹/消武器共用） */
void Label1055(void);
/* 位置助手（:1689-1748）：均操作 Raw[0x39] 槽 */
void subUpdateGrenade_Bazooka_BulletPositionForSpeed(void);
void subUpdateGrenade_Bazooka_BulletPositionForScroll(void);
void subZeroOutSpriteSpeed(void);

#endif
