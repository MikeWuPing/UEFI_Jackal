/* core/enemy_infantry.h：敌弹 + 步兵系（Bank6:334-774、4457-4564、7618-7824；
   Bank7:8201-8345 火焰弹）。 */
#ifndef JACKAL_CORE_ENEMY_INFANTRY_H
#define JACKAL_CORE_ENEMY_INFANTRY_H

#include <stdint.h>

void MobileInfantrySpriteLogic(uint8_t x);        /* Bank6:334（ID $01） */
void StationaryInfantrySpriteLogic(uint8_t x);    /* :646（ID $02/$03 喷火共用） */
void MobileSwampInfantrySpriteLogic(uint8_t x);   /* :4457（ID $04） */
void EnemyBulletSpriteLogic(uint8_t x);           /* :7618（ID $36） */
void FlameThrowerSpriteLogic(uint8_t x);          /* Bank7:8201（ID $34） */

/* :7715：Y=弹种 0-15（6 表：Display/Speed/Invis/Lifetime/Attr/偏移变体） */
void SpawnEnemyRoundBullet_Shell(uint8_t x, uint8_t y);
/* Bank7:8302：火焰生成（喷火步兵 $03/火焰坦克调用） */
void subSpawnFlame(uint8_t x);

#endif
