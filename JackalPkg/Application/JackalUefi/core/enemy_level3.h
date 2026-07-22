/* core/enemy_level3.h：Level 3 群（Bank6:2064-2997）。 */
#ifndef JACKAL_CORE_ENEMY_LEVEL3_H
#define JACKAL_CORE_ENEMY_LEVEL3_H

#include <stdint.h>

void InfantryTruckSpriteLogic(uint8_t x);             /* :2064（11） */
void SubmarineSpriteLogic(uint8_t x);                 /* :2149（29） */
void Level3LaserSpriteLogic(uint8_t x);               /* :2347（38） */
void Level3LaserChargingFlashesSpriteLogic(uint8_t x); /* :2489（0D） */
void SpreadTurretSpriteLogic(uint8_t x);              /* :2535（12/$92 boss） */
void Level3LargeAttackBoatSpriteLogic(uint8_t x);     /* :2724（1A） */
void Level3BossSpriteLogic(uint8_t x);                /* :2792（25） */

#endif
