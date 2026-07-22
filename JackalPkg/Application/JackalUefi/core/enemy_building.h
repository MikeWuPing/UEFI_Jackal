/* core/enemy_building.h：建筑/POW/星/门 + BG 更新子系统
   （Bank6:2910-3381、5545-5576、7145-7891；Bank7:3985-4160、6833-6878、7687-7739）。 */
#ifndef JACKAL_CORE_ENEMY_BUILDING_H
#define JACKAL_CORE_ENEMY_BUILDING_H

#include <stdint.h>

/* BG 更新子系统（Bank7:6833-6878 + :3985-4160） */
void subUpdateBGGraphicsFromSprite(uint8_t x, uint8_t idx);
void subUpdateBGGraphicsFromSprite_Every7thRNGFrame(uint8_t x, uint8_t idx);

/* 公共例程（dw 表项级，:7687-7739） */
void subPlayBarracksOpeningSound_LoadNewBuildingBGGraphics(uint8_t x);
void subPlayMissileHittingGroundSound_LoadHoleInGroundBGGraphics(uint8_t x);
void subPlayExplosionSound_LoadNewBGGraphics(uint8_t x);
void subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision(uint8_t x);
void subProcessExplosionAnimation(uint8_t x);
void subScrollSprite_CheckForDespawn_UpdateBG(uint8_t x);

/* Logic */
void POWBuildingSpriteLogic(uint8_t x);              /* :7145（13/14/1C/1D） */
void POWPowerUpBuildingSpriteLogic(uint8_t x);       /* :7211（15） */
void POWBuildingWithTankInsideSpriteLogic(uint8_t x);/* :5545（19） */
void subSpawnPOW(uint8_t x);                         /* :2998 */
void POWWalkingSpriteLogic(uint8_t x);               /* :3027（16） */
void POWLoadingIntoHeliSpriteLogic(uint8_t x);       /* :3220（27） */
void POWSpawnOnJeepDeathSpriteLogic(uint8_t x);      /* :3307（28） */
void POWDropOffSpriteLogic(uint8_t x);               /* :7512（3F） */
void GateSpriteLogic(uint8_t x);                     /* :7588（1B） */
void PowerUpStarSpriteLogic(uint8_t x);              /* :7825（50/51/52） */
void FlyingOverheadHeliSpriteLogic(uint8_t x);       /* :7250（3C） */
void LandedHeliSpriteLogic(uint8_t x);               /* :7307（3D/3E） */
void ParkedJeepTankSpriteLogic(uint8_t x);           /* :4104（4E/53） */

#endif
