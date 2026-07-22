/* core/enemy_level1.h：Level 1 完整化（Bank6:775-1255）。 */
#ifndef JACKAL_CORE_ENEMY_LEVEL1_H
#define JACKAL_CORE_ENEMY_LEVEL1_H

#include <stdint.h>

void Level1AttackBoatSpriteLogic(uint8_t x);   /* :840（ID $08） */
void Level1BossSpriteLogic(uint8_t x);         /* :913（ID $09，不可见生成器） */
void Level1BossTankSpriteLogic(uint8_t x);     /* :1011（ID $0A） */
void EndofLevelCheckSpriteLogic(uint8_t x);    /* :775（ID $46） */

#endif
