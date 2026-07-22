/* core/enemy_tank.c：坦克/炮塔/吉普系逐行翻译。
   共用骨架：Label315（State0 朝向初始化）、Label228（转向判定，SEC=前进/CLC=停留）、
   lblCalculateTank_JeepSpeed（mult 速度+推进）、subTurnTankTowardsJeep（:285-309
   行进+受阻转向：碰撞 → $D7=1 清速度、Data8&Data5 门控 dir+=Data4）+
   lblCalculateTank_JeepSpeed_CheckforCollision（:215-222 速度重算、$D7==0 → 进态）、
   Label232（行进：Data1 倒数/FarLookAhead/Label247 → $D7=1 清速度）；
   坦克表 recoil/弹道偏移硬编码。 */
#include "ram.h"
#include "enemy_tank.h"
#include "enemy_infantry.h"
#include "enemy_ai.h"
#include "spawn.h"
#include "sound_stub.h"

#define GRENADE_THROW_CLIP 0x11u
#define BOMB_HIT_GROUND_CLIP 0x15u

/* tblGrayTurretLockOnTime（:6865-6866） */
static const uint8_t tblGrayTurretLockOnTime[6] = { 0x40, 0x38, 0x30, 0x28, 0x20, 0x18 };
/* tblTurretBulletSpawnHoriz/VertSpawnLocationOffset（:7806-7810，方向 0-7） */
static const int8_t tblTurretBulletSpawnHorizSpawnLocationOffset[8] = {
  0x14, 0x0C, 0x00, -12, -20, -12, 0x00, 0x0C,
};
static const int8_t tblTurretBulletSpawnVertSpawnLocationOffset[8] = {
  0x00, 0x0C, 0x14, 0x0C, 0x00, -12, -20, -12,
};
/* tblTurretHorizontal/VerticalRecoilSpeedLB（:7127-7130） */
static const int8_t tblTurretHorizontalRecoilSpeedLB[4] = { -96, -96, 0x00, 0x60 };
static const int8_t tblTurretVerticalRecoilSpeedLB[4] = { 0x00, -96, -96, -96 };

/* ---------------------------------------------------------------- 坦克共用段（:179-333） */

/* Label315（:179-196）：State0 朝向初始化（subCalculateDirectionTowardJeep+
   subUpdateSpriteForDirectionChange、WhatDirectionToShoot=Y*4、Data4=槽奇偶 ±4） */
static void label315(uint8_t x) {
  uint8_t y;
  subCheckWhichJeepToAttack(x);
  y = subCalculateDirectionTowardJeep(x);
  subUpdateSpriteForDirectionChange(x, y);
  JackalRam.SpriteWhatDirectionToShoot[x] =
      (uint8_t)(fctGetSpriteOrientationIndex(y) * 4u);
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData4[x] = (x & 1u) != 0 ? 0xFCu : 0x04u;   /* :188-195 */
  subMoveSpriteToNextState(x);
}

/* Label247（:3877-3966）：坦克间碰撞预测（前眺 2×速度坐标，扫他槽 ID≠0 且 <$34 且非己，
   |dx|<$10 且 |dy|<$10 → SEC） */
uint8_t label247(uint8_t x) {
  int8_t i;
  int16_t px = (int16_t)((uint16_t)JackalRam.SpriteAbsoluteHorizPositionLB[x] |
                         ((uint16_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 8)) +
               (int16_t)((int8_t)JackalRam.SpriteHorizSpeedUB[x]) * 2;
  int16_t py = (int16_t)((uint16_t)JackalRam.SpriteVertScreenPosition[x] |
                         ((uint16_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 8)) +
               (int16_t)((int8_t)JackalRam.SpriteVertSpeedUB[x]) * 2;
  int16_t otherX, otherY;
  for (i = 0x0F; i >= 0; i--) {
    if (JackalRam.SpriteObjectID[i] == 0 || JackalRam.SpriteObjectID[i] >= 0x34u ||
        i == (int8_t)x) {
      continue;
    }
    otherX = (int16_t)((uint16_t)JackalRam.SpriteAbsoluteHorizPositionLB[i] |
                       ((uint16_t)JackalRam.SpriteAbsoluteHorizPositionUB[i] << 8));
    otherY = (int16_t)((uint16_t)JackalRam.SpriteVertScreenPosition[i] |
                       ((uint16_t)JackalRam.SpriteAbsoluteVertPositionUB[i] << 8));
    if (otherX > px - 0x10 && otherX < px + 0x10 &&
        otherY > py - 0x10 && otherY < py + 0x10) {
      return 1;                                        /* SEC（:3958） */
    }
  }
  return 0;                                            /* CLC（:3964） */
}

/* Label228（:245-283）：转向判定——目标倒数/换目标、方向差 bit4 ±4 趋近（Data8&Data5 门控）、
   无差且 NearLookAhead 无碰撞 → SEC（前进）；返回 1=SEC/0=CLC */
uint8_t label228(uint8_t x) {
  int8_t diff;
  uint8_t aligned = 0;
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  diff = (int8_t)(subCalculateDirectionTowardJeep(x) - JackalRam.SpriteWhatDirectionToShoot[x]);
  if (diff == 0) {
    if (fctGetCollision_WithSpeed_NearLookAhead_BG(x) == 0) {
      aligned = 1;                                     /* + :267 → SEC 前进 */
    }
  }
  if (!aligned) {
    JackalRam.SpriteData8[x]--;
    if ((JackalRam.SpriteData8[x] & JackalRam.SpriteData5[x]) == 0) {
      if ((diff & 0x10u) != 0) {
        JackalRam.SpriteWhatDirectionToShoot[x] =
            (uint8_t)((JackalRam.SpriteWhatDirectionToShoot[x] - 4u) & 0x1Fu);
      } else {
        JackalRam.SpriteWhatDirectionToShoot[x] =
            (uint8_t)((JackalRam.SpriteWhatDirectionToShoot[x] + 4u) & 0x1Fu);
      }
      JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
      subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
      Label244(x);
      return 1;                                        /* SEC（:275，转向后仍前进） */
    }
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
    subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
    Label244(x);
    return 0;                                          /* CLC（:277，停留） */
  }
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  Label244(x);
  return 1;                                            /* SEC（:273-275） */
}

/* subTurnTankTowardsJeep（:285-310）+ lblCalculateTank_JeepSpeed_CheckforCollision */
/* subTurnTankTowardsJeep（:285-309）：行进+受阻转向——FarLookAhead/Label247 碰撞
   → $D7=1 清速度；Label244 移动；$D7!=0 时 Data8 倒数到 (Data8&Data5)==0 →
   dir += Data4（槽奇偶 ±4，避开障碍）。速度计算在调用方（lblCalculateTank_
   JeepSpeed_CheckforCollision :215-222：STA $08/calc/$D7==0 → 进态）。 */
void subTurnTankTowardsJeep(uint8_t x) {
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) != 0) {
    subClearSpriteSpeed(x);                          /* :291 BNE ++（$D7 由 lookahead 写） */
  } else if (label247(x) != 0) {
    JackalRam.Raw[0xD7] = 1;                         /* :294-295 */
    subClearSpriteSpeed(x);
  }
  Label244(x);                                       /* :297 */
  if (JackalRam.Raw[0xD7] != 0) {                    /* :298-309 受阻转向 */
    JackalRam.SpriteData8[x]--;
    if ((JackalRam.SpriteData8[x] & JackalRam.SpriteData5[x]) == 0) {
      JackalRam.SpriteWhatDirectionToShoot[x] =
          (uint8_t)((JackalRam.SpriteWhatDirectionToShoot[x] + JackalRam.SpriteData4[x]) & 0x1Fu);
    }
  }
}

/* Label232（:311-321）：行进行碰判定（Data1 倒数到 0 → $D7=1；FarLookAhead/Label247 → 清速度） */
void label232(uint8_t x) {
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.Raw[0xD7] = 1;
    subClearSpriteSpeed(x);
  } else {
    if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) != 0 || label247(x) != 0) {
      subClearSpriteSpeed(x);
    } else {
      Label244(x);
      return;
    }
  }
  Label244(x);
}

/* ---------------------------------------------------------------- RedMediumTank（:158-243） */

static void RedMediumTankState0(uint8_t x) {  /* :170-196 */
  JackalRam.SpriteTypeIndex[x] = 0x22;
  JackalRam.SpriteData6[x] = 0x22;
  JackalRam.SpriteData2[x] = 0x20;
  JackalRam.SpriteData3[x] = 0x20;
  JackalRam.SpriteData5[x] = 0x1F;
  label315(x);
}
static void RedMediumTankState1(uint8_t x) {  /* :198-210 */
  if (label228(x) == 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x50;
  JackalRam.Raw[0x08] = 0x12;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x12);
  subMoveSpriteToNextState(x);
}
static void RedMediumTankState2(uint8_t x) {  /* :212-222 */
  subTurnTankTowardsJeep(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x12);  /* :214-218 */
  if (JackalRam.Raw[0xD7] == 0) {
    subMoveSpriteToNextState(x);
  }
}
static void RedMediumTankState3(uint8_t x) {  /* :225-243 */
  label232(x);
  if (JackalRam.Raw[0xD7] != 0) {              /* ++ :238-243 */
    subSetSpriteState(x, JackalRam.SpriteObjectID[x] == 0x07u ? 1u : 2u);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                    /* 屏外不射（:234 BMI +） */
  }
  SpawnEnemyRoundBullet_Shell(x, 1);
}
void RedMediumTankSpriteLogic(uint8_t x) {  /* :158-168（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    RedMediumTankState0, RedMediumTankState1, RedMediumTankState2,
    RedMediumTankState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- SilverLargeTank（:1184-1241） */

static void SilverLargeTankState0(uint8_t x) {  /* :1196-1207 */
  JackalRam.SpriteTypeIndex[x] = 0x44;
  JackalRam.SpriteData6[x] = 0x44;
  JackalRam.SpriteGraphicsAttributes[x] = 2;
  JackalRam.SpriteData2[x] = 0x20;
  JackalRam.SpriteData3[x] = 0x20;
  JackalRam.SpriteData5[x] = 0x0F;
  label315(x);
}
static void SilverLargeTankState1(uint8_t x) {  /* :1209-1216 */
  if (label228(x) == 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x50;
  JackalRam.Raw[0x08] = 0x11;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x11);
  subMoveSpriteToNextState(x);
}
static void SilverLargeTankState2(uint8_t x) {  /* :1218-1221 */
  subTurnTankTowardsJeep(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x11);
  if (JackalRam.Raw[0xD7] == 0) {
    subMoveSpriteToNextState(x);
  }
}
static void SilverLargeTankState3(uint8_t x) {  /* :1223-1241 */
  label232(x);
  if (JackalRam.Raw[0xD7] != 0) {
    subSetSpriteState(x, JackalRam.SpriteObjectID[x] == 0x0Bu ? 1u : 2u);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;
  }
  SpawnEnemyRoundBullet_Shell(x, 3);
}
void SilverLargeTankSpriteLogic(uint8_t x) {  /* :1184-1194（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    SilverLargeTankState0, SilverLargeTankState1, SilverLargeTankState2,
    SilverLargeTankState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- FlameTank（:5480-5530） */

static void FlameTankState0(uint8_t x) {  /* :5492-5502 */
  JackalRam.SpriteTypeIndex[x] = 0x44;
  JackalRam.SpriteData6[x] = 0x44;
  JackalRam.SpriteGraphicsAttributes[x] = 1;
  JackalRam.SpriteData2[x] = 0x08;
  JackalRam.SpriteData5[x] = 0x7F;
  label315(x);
}
static void FlameTankState1(uint8_t x) {  /* :5504-5511 */
  if (label228(x) == 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x30;
  JackalRam.Raw[0x08] = 0x10;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x10);
  subMoveSpriteToNextState(x);
}
static void FlameTankState2(uint8_t x) {  /* :5513-5516 */
  subTurnTankTowardsJeep(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x10);
  if (JackalRam.Raw[0xD7] == 0) {
    subMoveSpriteToNextState(x);
  }
}
static void FlameTankState3(uint8_t x) {  /* :5518-5530 */
  JackalRam.SpriteData2[x] = 0x08;
  label232(x);
  if (JackalRam.Raw[0xD7] == 0) {
    return;                                  /* ++ :5530 RTS（行进中不喷火） */
  }
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    subSpawnFlame(x);                        /* :5527 屏内喷火 */
  }
  subSetSpriteState(x, 1);
}
void FlameTankSpriteLogic(uint8_t x) {  /* :5480-5490（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    FlameTankState0, FlameTankState1, FlameTankState2,
    FlameTankState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- EnemyJeep（:1437-1499） */

static void EnemyJeepState0(uint8_t x) {  /* :1449-1462 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.CurrentLevel == 5u ? 0xB5u : 0x3Du;
  JackalRam.SpriteData6[x] = JackalRam.SpriteTypeIndex[x];
  JackalRam.SpriteData2[x] = 0x40;
  JackalRam.SpriteData3[x] = 0x40;
  JackalRam.SpriteData5[x] = 0x07;
  label315(x);
}
static void EnemyJeepState1(uint8_t x) {  /* :1464-1472 */
  if (label228(x) == 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x20;
  JackalRam.Raw[0x08] = 0x30;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x30);
  subMoveSpriteToNextState(x);
}
static void EnemyJeepState2(uint8_t x) {  /* :1474-1477 */
  subTurnTankTowardsJeep(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x30);
  if (JackalRam.Raw[0xD7] == 0) {
    subMoveSpriteToNextState(x);
  }
}
/* subSpawnJeep_BomberBomb（:1406-1422）：父在屏才投弹；$09=ID-$3A（$3B 显示为投掷，
   :1413 注原游戏 quirk）；公开共享（EnemyJeepState3/AttackPlaneState1 双调用点） */
void subSpawnJeep_BomberBomb(uint8_t x) {
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                  /* :1407 父屏外中止 */
  }
  JackalSpawnZp[0x08] = 0x37;
  JackalSpawnZp[0x09] = (uint8_t)(JackalRam.SpriteObjectID[x] - 0x3Au);
  JackalSpawnZp[0x0C] = subCalculateDirectionTowardJeep(x);
  JackalSpawnZp[0x0E] = (uint8_t)(JackalRam.SpriteWhichJeeptoAttack[x] & 0x80u);
  JackalSpawnZp[0x0F] = 0x01;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
}

static void EnemyJeepState3(uint8_t x) {  /* :1479-1499 */
  if (fctGetDistanceBetweenEnemyAndJeep(x) < 0x20u) {
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x20);
  }
  label232(x);
  if (JackalRam.Raw[0xD7] != 0) {
    subSetSpriteState(x, 1);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  subSpawnJeep_BomberBomb(x);                /* :1497-1499 JMP（内联版未设 $35 父槽，已修） */
}
void EnemyJeepSpriteLogic(uint8_t x) {  /* :1437-1447（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    EnemyJeepState0, EnemyJeepState1, EnemyJeepState2,
    EnemyJeepState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- FallingBomb（:1331-1403） */

static void FallingBombState0(uint8_t x) {  /* :1342-1363 */
  JackalRam.SpriteTypeIndex[x] = 0x41;
  subInitiateSoundClip(GRENADE_THROW_CLIP);          /* stub */
  JackalRam.SpriteData1[x] = 0x30;
  if (JackalRam.SpriteData4[x] != 0) {               /* 吉普/anywhere 机投掷变体 */
    JackalRam.SpriteTypeIndex[x] = 0x40;
    JackalRam.SpriteData1[x] = 0x40;
  }
  JackalRam.Raw[0x6A + x] = (uint8_t)(JackalRam.RNG_INCEveryFrame & 0xC0u);  /* :1355-1357 */
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x11);
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}
static void FallingBombState1(uint8_t x) {  /* :1365-1391 */
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
      JackalRam.SpriteTypeIndex[x]++;
      JackalRam.Raw[0x6A + x] = (uint8_t)(JackalRam.Raw[0x6A + x] + 0x40u);
    }
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
    return;
  }
  /* 落地（:1380-1390） */
  JackalRam.SpriteData1[x] = 0x18;
  JackalRam.SpriteHitboxShapeIndex[x] = 0x74;        /* 落地后可伤人（Task 3.8 注） */
  JackalRam.SpriteGraphicsAttributes[x] = 3;
  JackalRam.SpriteTypeIndex[x] = 0x19;
  subClearSpriteSpeed(x);
  subInitiateSoundClip(BOMB_HIT_GROUND_CLIP);        /* stub */
  subMoveSpriteToNextState(x);
}
static void FallingBombState2(uint8_t x) {  /* :1393-1403 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subDespawnSprite(x);
    return;
  }
  if ((JackalRam.SpriteData1[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
}
void FallingBombSpriteLogic(uint8_t x) {  /* :1331-1340（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    FallingBombState0, FallingBombState1, FallingBombState2,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 灰炮塔（:6760-7130 + :7775-7810） */

void SpawnTurretProjectile(uint8_t x) {  /* :7775-7803 */
  uint8_t y = JackalRam.SpriteData6[x];
  JackalSpawnZp[0x00] = (uint8_t)tblTurretBulletSpawnHorizSpawnLocationOffset[y];
  JackalSpawnZp[0x01] = tblTurretBulletSpawnHorizSpawnLocationOffset[y] < 0 ? 0xFFu : 0x00u;
  JackalSpawnZp[0x02] = (uint8_t)tblTurretBulletSpawnVertSpawnLocationOffset[y];
  JackalSpawnZp[0x03] = tblTurretBulletSpawnVertSpawnLocationOffset[y] < 0 ? 0xFFu : 0x00u;
  if (JackalRam.SpriteObjectID[x] == 0x4Fu) {        /* :7790-7795 L6 Boss Tank Turret */
    SpawnEnemyRoundBullet_Shell(x, 0x0B);
    return;
  }
  /* :7797-7802：ID|$80 - $85 + $0C（$85→$0C 白弹、$86→$0D 黄弹） */
  SpawnEnemyRoundBullet_Shell(x,
      (uint8_t)((JackalRam.SpriteObjectID[x] | 0x80u) - 0x85u + 0x0Cu));
}

/* Label490（:7051-7105）：白弹弹道——生成点偏移后朝向吉普重算，再恢复位置；
   黄弹走 Label698（:7101-7104：Data6*4 直射） */
void Label490(uint8_t x) {
  uint8_t y;
  uint8_t lb, ub, vpos, vub;
  uint16_t wide;
  if ((uint8_t)(JackalRam.SpriteObjectID[x] | 0x80u) == 0x86u) {   /* :7052-7055 */
    JackalRam.SpriteWhatDirectionToShoot[x] =
        (uint8_t)(JackalRam.SpriteData6[x] * 4u);                  /* Label698 */
    return;
  }
  y = JackalRam.SpriteData6[x];
  lb = JackalRam.SpriteAbsoluteHorizPositionLB[x];
  ub = JackalRam.SpriteAbsoluteHorizPositionUB[x];
  vpos = JackalRam.SpriteVertScreenPosition[x];
  vub = JackalRam.SpriteAbsoluteVertPositionUB[x];
  /* :7064-7089 偏移到炮口（LB CLC ADC、UB ADC 符号扩展，16 位加） */
  wide = (uint16_t)(((uint16_t)ub << 8) | lb);
  wide = (uint16_t)(wide + (int16_t)tblTurretBulletSpawnHorizSpawnLocationOffset[y]);
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wide;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wide >> 8);
  wide = (uint16_t)(((uint16_t)vub << 8) | vpos);
  wide = (uint16_t)(wide + (int16_t)tblTurretBulletSpawnVertSpawnLocationOffset[y]);
  JackalRam.SpriteVertScreenPosition[x] = (uint8_t)wide;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = (uint8_t)(wide >> 8);
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  /* :7092-7099 恢复原值 */
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = lb;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = ub;
  JackalRam.SpriteVertScreenPosition[x] = vpos;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = vub;
}

/* Label493（:7107-7125）：recoil 速度（Data6&3 查表、bit7 符号、Data6&4 反转） */
void Label493(uint8_t x) {
  uint8_t y = (uint8_t)(JackalRam.SpriteData6[x] & 3u);
  JackalRam.SpriteHorizSpeedLB[x] = (uint8_t)tblTurretHorizontalRecoilSpeedLB[y];
  JackalRam.SpriteHorizSpeedUB[x] = tblTurretHorizontalRecoilSpeedLB[y] < 0 ? 0xFFu : 0x00u;
  JackalRam.SpriteVertSpeedLB[x] = (uint8_t)tblTurretVerticalRecoilSpeedLB[y];
  JackalRam.SpriteVertSpeedUB[x] = tblTurretVerticalRecoilSpeedLB[y] < 0 ? 0xFFu : 0x00u;
  if ((JackalRam.SpriteData6[x] & 4u) != 0) {
    subInvertSpriteVertAndHorizSpeed(x);
  }
}

static void grayTurretState0Common(uint8_t x, uint8_t l6Palette, uint8_t writeData6) {  /* :6773-6805/:6962-6990 */
  JackalRam.SpriteObjectID[x] &= 0x7Fu;              /* 去优先级（生成时 bit7 置） */
  JackalRam.SpriteTypeIndex[x] = 0x1C;
  JackalRam.SpriteData5[x] = 0x08;                   /* 朝下 */
  subUpdateSpriteForDirectionChange(x, 0x08);        /* → TypeIndex=$1E（$1C+2） */
  /* 白弹 :6786-6787 为 `TYA LDA SpriteData6,X`——TYA 被 LDA 覆盖的死代码（原游戏 quirk，
     Data6 保持生成值 0）；黄弹 :6971-6972 为 `TYA STA SpriteData6,X`（正常写入 Y） */
  if (writeData6) {
    JackalRam.SpriteData6[x] = fctGetSpriteOrientationIndex(0x08);
  }
  JackalRam.SpriteData3[x] = 0x0D;
  JackalRam.SpriteData2[x] =
      (uint8_t)((0x0Du - JackalRam.DifficultyBasedOnWeapon - JackalRam.CurrentLevel) >> 1);
  JackalRam.SpriteGraphicsAttributes[x] = 2;
  if (JackalRam.CurrentLevel == 5u) {
    JackalRam.SpriteGraphicsAttributes[x] = l6Palette;   /* L6 变体 palette */
  }
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void LargeGrayTurretWhiteBulletsState0(uint8_t x) {
  grayTurretState0Common(x, 0, 0);                   /* :6801 L6 palette 0；白弹 TYA 死代码不写 Data6 */
}

/* 炮塔转向共用段（:6807-6846 白 / :6992-7038 黄）：yellow=1 有限位 */
static void grayTurretState1Turn(uint8_t x, uint8_t yellow) {
  uint8_t dir;
  JackalRam.SpriteTypeIndex[x] = 0x1C;
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteData5[x]);
  JackalRam.SpriteData6[x] = fctGetSpriteOrientationIndex(JackalRam.SpriteData5[x]);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  dir = subCalculateDirectionTowardJeep(x);
  if (dir == JackalRam.SpriteData5[x]) {             /* +++ :6847/:7039 锁定开火 */
    if ((int8_t)JackalRam.SpriteState[x] < 0) {
      return;
    }
    JackalRam.SpriteData1[x] =
        tblGrayTurretLockOnTime[(uint8_t)((JackalRam.DifficultyBasedOnWeapon +
                                           JackalRam.CurrentLevel) >> 1)];
    Label490(x);
    subMoveSpriteToNextState(x);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] =
      (uint8_t)((JackalRam.SpriteData3[x] - JackalRam.DifficultyBasedOnWeapon -
                 JackalRam.CurrentLevel) >> 1);
  if (((uint8_t)(dir - JackalRam.SpriteData5[x]) & 0x10u) == 0) {
    /* + :6833-6839/:7019-7026 顺时针趋近（黄弹限位 Y>=4 不转） */
    uint8_t nd = (uint8_t)((JackalRam.SpriteData5[x] + 1u) & 0x1Fu);
    if (!yellow || fctGetSpriteOrientationIndex(nd) < 4u) {
      JackalRam.SpriteData5[x] = nd;
    }
  } else {
    /* ++ :6840-6844/:7030-7037 逆时针趋近（黄弹限位 Y==0 不转） */
    uint8_t nd = (uint8_t)((JackalRam.SpriteData5[x] - 1u) & 0x1Fu);
    if (!yellow || fctGetSpriteOrientationIndex(nd) != 0u) {
      JackalRam.SpriteData5[x] = nd;
    }
  }
}

static void LargeGrayTurretWhiteBulletsState1(uint8_t x) {
  grayTurretState1Turn(x, 0);
}
static void LargeGrayTurretYellowBulletState1(uint8_t x) {
  grayTurretState1Turn(x, 1);
}

static void LargeGrayTurretWhiteBulletsState2(uint8_t x) {  /* :6868-6885 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
      JackalRam.SpriteData8[x] = 0x03;               /* recoil 帧 */
      JackalRam.SpriteTypeIndex[x] += 3;
      SpawnTurretProjectile(x);
      Label493(x);
      subMoveSpriteToNextState(x);                   /* :6883 JMP subMoveSpriteToNextState——
         初版漏调：停 State2 每 16 帧连发且 TypeIndex 累加 +3 花屏（用户报告 bug #2 关联） */
    }
    return;
  }
  subSetSpriteState(x, 1);                           /* :6884 */
}

static void LargeGrayTurretWhiteBulletsState3(uint8_t x) {  /* :6887-6895 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteData8[x] = 0x03;
  subInvertSpriteVertAndHorizSpeed(x);
  subMoveSpriteToNextState(x);
}

static void LargeGrayTurretWhiteBulletsState4(uint8_t x) {  /* :6897-6910 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0) {
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
    return;
  }
  JackalRam.SpriteTypeIndex[x] -= 3;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subClearSpriteSpeed(x);
  subSetSpriteState(x, 2);
}

static void LargeGrayTurretWhiteBulletsState5(uint8_t x) {  /* :6912-6935 */
  subClearSpriteSpeed(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x] = 0x18;
  JackalRam.SpriteTypeIndex[x] = 0x19;
  JackalRam.SpriteGraphicsAttributes[x] = 3;
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  if (!(JackalRam.CurrentLevel == 5u && JackalRam.CurrentLevelScreen == 0x0Du)) {
    subInitiateSoundClip(0x17u);                     /* MainWeaponExplosionOnEnemy stub */
  }
  subMoveSpriteToNextState(x);
}

void LargeGrayTurretWhiteBulletsSpriteLogic(uint8_t x) {  /* :6760-6771（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    LargeGrayTurretWhiteBulletsState0, LargeGrayTurretWhiteBulletsState1,
    LargeGrayTurretWhiteBulletsState2, LargeGrayTurretWhiteBulletsState3,
    LargeGrayTurretWhiteBulletsState4, LargeGrayTurretWhiteBulletsState5,
    subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

static void LargeGrayTurretYellowBulletState0(uint8_t x) {
  grayTurretState0Common(x, 1, 1);                   /* :6986 L6 palette 1；黄弹 TYA STA 写 Data6 */
}

void LargeGrayTurretYellowBulletSpriteLogic(uint8_t x) {  /* :6949-6960（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    LargeGrayTurretYellowBulletState0, LargeGrayTurretYellowBulletState1,
    LargeGrayTurretWhiteBulletsState2, LargeGrayTurretWhiteBulletsState3,
    LargeGrayTurretWhiteBulletsState4, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}
