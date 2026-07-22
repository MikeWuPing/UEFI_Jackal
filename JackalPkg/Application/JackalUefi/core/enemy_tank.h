/* core/enemy_tank.h：坦克/炮塔/吉普系（Bank6:158-333、1184-1255、1331-1512、
   5480-5544、6760-7130、7775-7810）。 */
#ifndef JACKAL_CORE_ENEMY_TANK_H
#define JACKAL_CORE_ENEMY_TANK_H

#include <stdint.h>

void RedMediumTankSpriteLogic(uint8_t x);              /* :158（ID $07/$24 变体） */
void SilverLargeTankSpriteLogic(uint8_t x);            /* :1184（ID $0B/$23 变体） */
void FlameTankSpriteLogic(uint8_t x);                  /* :5480（ID $0E） */
void EnemyJeepSpriteLogic(uint8_t x);                  /* :1437（ID $0F） */
void FallingBombSpriteLogic(uint8_t x);                /* :1331（ID $37） */
void LargeGrayTurretWhiteBulletsSpriteLogic(uint8_t x); /* :6760（ID $05） */
void LargeGrayTurretYellowBulletSpriteLogic(uint8_t x); /* :6949（ID $06） */

void SpawnTurretProjectile(uint8_t x);                 /* :7775 炮塔弹道生成 */
void subSpawnJeep_BomberBomb(uint8_t x);               /* :1406 轰炸机/敌吉普投弹 spawn */
void Label490(uint8_t x);                              /* :7051 炮口偏移后朝向吉普重算 */
void Label493(uint8_t x);                              /* :7107 炮塔 recoil 速度 */

/* 坦克公共标签（多 Logic 共享：RedMediumTank/SilverLargeTank/FlameTank/EnemyJeep/Level1BossTank） */
uint8_t label228(uint8_t x);                           /* :245-283 转向判定（1=SEC 前进/0=CLC 停留） */
void label232(uint8_t x);                              /* :311-321 行进行碰（Data1 倒数/FarLookAhead/label247） */
uint8_t label247(uint8_t x);                           /* :3877-3966 坦克间碰撞预测（1=SEC） */
void subTurnTankTowardsJeep(uint8_t x);                /* :285-309 行进+受阻转向（速度计算在调用方） */

#endif
