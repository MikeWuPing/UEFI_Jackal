/* core/enemy_level6.h：Level 6 群 + 通用载具（Bank6:20-157、1256-1317、3382-3967、
   6093-6747；Bank7:1530-1537、6808-6831、8154-8196、8347-8532）。
   $D5（ram.h Level6BossTurretStatus）：MSB=换 palette、LSB=双激光炮塔存活；
   Label474/subFinalBossBuildingTransitionToFinalBossTank 共用 :8402 爆炸生成尾段。 */
#ifndef JACKAL_CORE_ENEMY_LEVEL6_H
#define JACKAL_CORE_ENEMY_LEVEL6_H

#include <stdint.h>

void BlackAndWhite_SmallMissileSpriteLogic(uint8_t x);   /* :20（ID $39，弹种 Data6=0-3） */
void AttackPlaneSpriteLogic(uint8_t x);                  /* :1256（ID $3A 定点/$3B 随机） */
void Level6AttackHelicopterSpriteLogic(uint8_t x);       /* :6093（ID $43） */
void EscalatorSpriteLogic(uint8_t x);                    /* Bank7:8154（ID $44） */
void Level6BossLoadSpriteLogic(uint8_t x);               /* :6535（ID $45） */
void Level6BossLaserTurretSpriteLogic(uint8_t x);        /* :6317（ID $47） */
void Level6BossLaserTurretBlastSpriteLogic(uint8_t x);   /* :6468（ID $48） */
void Level6BossLaserTurretGraphicsLoadSpriteLogic(uint8_t x); /* :3382（ID $49） */
void Level6FinalBossSpriteLogic(uint8_t x);              /* :6607（ID $4A） */
void Level6FinalBossTankSpriteLogic(uint8_t x);          /* :3411（ID $4B） */
void Level6FinalBossTankFlameShotSpriteLogic(uint8_t x); /* :3628（ID $4C） */
void Level6FinalBossTankFlameShotTipSpriteLogic(uint8_t x); /* :3699（ID $4D） */
void Level6BossTankTurretSpriteLogic(uint8_t x);         /* :3744（ID $4F） */
void Level6MissileLauncherSpriteLogic(uint8_t x);        /* :6227（ID $2E） */

/* Bank7 公共段（:8347-8532 + :1530）：Label474 坦克爆炸收尾（首帧布置/复帧爆炸+Label1364） */
uint8_t Label474(void);                                  /* :8347 返回 0=收尾完成 */
uint8_t subFinalBossBuildingTransitionToFinalBossTank(void); /* :8370 返回 0=过渡完成 */
void subDespawnAllObjectsExceptFinalBoss(void);          /* :8520 除 $4A/$4B 全清 */
void subEraseLevel6BossFlags(void);                      /* :1530 清 $0113-$0119 */
void subSpawnPlane_HeliPositionRelativeToJeep(uint8_t x); /* :6808 机/直相对吉普定位 */

#endif
