/* core/enemy_level2.h：Level 2 群（Bank6:1513-2063）。 */
#ifndef JACKAL_CORE_ENEMY_LEVEL2_H
#define JACKAL_CORE_ENEMY_LEVEL2_H

#include <stdint.h>

void Level2PillarSpriteLogic(uint8_t x);            /* :1513（10/1F） */
void Level2PillarTopSpriteLogic(uint8_t x);         /* :1609（1E） */
void Level2BossStatueHeadSpriteLogic(uint8_t x);    /* :1703（18） */
void Level2StatueHeadSpriteLogic(uint8_t x);        /* :1817（21/22） */
void Level2BossSpriteLogic(uint8_t x);              /* :1918（20） */

#endif
