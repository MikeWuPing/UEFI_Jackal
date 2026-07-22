/* core/enemy_level3.c：Level 3 群逐行翻译（Bank6:2064-2997）。 */
#include "ram.h"
#include "enemy_level3.h"
#include "enemy_building.h"
#include "enemy_infantry.h"
#include "enemy_tank.h"
#include "enemy_ai.h"
#include "spawn.h"
#include "sound_stub.h"

#define LASER_BLAST_CLIP 0x4Du

/* ---------------------------------------------------------------- InfantryTruck（:2064-2134） */

static void InfantryTruckState0(uint8_t x) {  /* :2076-2086 */
  JackalRam.SpriteTypeIndex[x] = 0x54;
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x20;
  JackalRam.SpriteData2[x] = 0x81;
  JackalRam.SpriteData3[x] = 0x08;
  subMoveSpriteToNextState(x);
}
static void InfantryTruckState1(uint8_t x) {  /* :2088-2098 */
  Label244(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteHorizSpeedLB[x] = 0xA0;
  JackalRam.SpriteData1[x] = 0x90;
  subMoveSpriteToNextState(x);
}
static void InfantryTruckState2(uint8_t x) {  /* :2100-2112 */
  Label244(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subClearSpriteHorizSpeed(x);               /* :2108 行驶完成 */
    return;
  }
  if (JackalRam.SpriteData1[x] == 0xF0u) {
    JackalRam.SpriteData1[x] = JackalRam.SpriteData2[x];
    JackalRam.SpriteTypeIndex[x]++;
    subMoveSpriteToNextState(x);
  }
}
static void InfantryTruckState3(uint8_t x) {  /* :2114-2134 */
  Label244(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteData1[x] = JackalRam.SpriteData2[x];
  if (JackalRam.SpriteData3[x] == 0) {
    return;
  }
  JackalRam.SpriteData3[x]--;
  /* :2123-2134 spawn 步兵（左偏移 $EC/$FF） */
  JackalSpawnZp[0x00] = 0xEC;
  JackalSpawnZp[0x01] = 0xFF;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x08] = 0x01;
  JackalSpawnZp[0x0F] = 0x01;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
}
void InfantryTruckSpriteLogic(uint8_t x) {  /* :2064-2074（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    InfantryTruckState0, InfantryTruckState1, InfantryTruckState2,
    InfantryTruckState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Submarine（:2149-2332） */

static void SubmarineState0(uint8_t x) {  /* :2164-2171 */
  JackalRam.SpriteTypeIndex[x] = 0x5E;
  JackalRam.SpriteData1[x] = 0x20;
  subUpdateSpritePositionForScrolling(x);
  subCheckWhichJeepToAttack(x);
  subMoveSpriteToNextState(x);
}
static void SubmarineState1(uint8_t x) {  /* :2173-2193 */
  JackalRam.SpriteTypeIndex[x] = 0x5E;
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] = 0x00;       /* :2179-2180 潜没闪烁 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x20;
  JackalRam.SpriteTypeIndex[x] = 0x58;         /* 上浮帧 1 */
  JackalRam.SpriteData5[x] = 0x58;
  JackalRam.SpriteHitboxShapeIndex[x] = 0x04;
  subMoveSpriteToNextState(x);
}
static void SubmarineState2(uint8_t x) {  /* :2195-2217 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  JackalRam.SpriteData1[x]--;
  if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
    JackalRam.SpriteData5[x]++;                /* :2202 每 $10 帧上浮一档 */
  }
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;         /* :2207-2210 水下部分闪烁 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteData1[x] = 0x20;
    subMoveSpriteToNextState(x);
  }
}
static void SubmarineState3(uint8_t x) {  /* :2219-2254 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x40;
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    /* :2238-2253 spawn 导弹（朝向*4） */
    JackalSpawnZp[0x08] = 0x39;
    JackalSpawnZp[0x0A] = 0x4B;
    JackalSpawnZp[0x0B] = 0x03;
    JackalSpawnZp[0x0C] =
        (uint8_t)(fctGetSpriteOrientationIndex(subCalculateDirectionTowardJeep(x)) * 4u);
    JackalSpawnZp[0x0F] = 0x00;
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x35] = x;
    (void)subSpawnObjectFromParent();
  }
  subMoveSpriteToNextState(x);
}
static void SubmarineState4(uint8_t x) {  /* :2256-2273 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteData1[x] = 0x30;
    subMoveSpriteToNextState(x);
  }
}
static void SubmarineState5(uint8_t x) {  /* :2275-2314 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
      JackalRam.SpriteData5[x]--;
    }
    if ((JackalRam.SpriteData1[x] & 1u) != 0) {
      JackalRam.SpriteTypeIndex[x] += 3;
    }
  }
  /* :2291-2293（++ 公共段）：subUpdate 总跑，Data1!=0 → RTS */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  JackalRam.SpriteTypeIndex[x] = 0x5E;
  if (JackalRam.SpriteData4[x] != 3u &&
      (int8_t)JackalRam.SpriteAbsoluteVertPositionUB[x] < 0) {   /* :2301-2302 屏上方才上移 */
    JackalRam.SpriteData4[x]++;
    JackalRam.SpriteData1[x] = 0x80;
    subCalculateObjectSpeed(x, 0x18, 0x10);      /* :2306-2309 向上 mult=$10 */
    subMoveSpriteToNextState(x);
    return;
  }
  JackalRam.SpriteData1[x] = 0x60;
  subSetSpriteState(x, 1);                       /* :2312-2314 回 State1 */
}
static void SubmarineState6(uint8_t x) {  /* :2316-2332 */
  JackalRam.SpriteTypeIndex[x] = 0x5E;
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] = 0x00;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalRam.SpriteData1[x] = 0x60;
  subClearSpriteSpeed(x);
  subSetSpriteState(x, 1);
}
void SubmarineSpriteLogic(uint8_t x) {  /* :2149-2162（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    SubmarineState0, SubmarineState1, SubmarineState2, SubmarineState3,
    SubmarineState4, SubmarineState5, SubmarineState6,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level3Laser（:2347-2474） */

/* tblLaserX_LB/UB（:2426-2434） */
static const uint8_t tblLaserX_LB[3] = { 0xF0, 0x30, 0x70 };
static const uint8_t tblLaserX_UB[3] = { 0x00, 0x01, 0x01 };

static void Level3LaserState0(uint8_t x) {  /* :2357-2379 */
  if ((uint8_t)(JackalRam.RNG_INCEveryFrame & 3u) == 3u) {
    JackalRam.SpriteData5[x] = 0;              /* :2362 恰好 3 → 从 1 号起 */
  }
  JackalRam.SpriteData3[x] = 0x5F;             /* 长激光 */
  JackalRam.SpriteData4[x] = 0x40;
  if (JackalRam.CurrentLevelScreen != 7u) {    /* :2368-2375 短激光（screen 8） */
    JackalRam.SpriteData2[x]++;
    JackalRam.SpriteData3[x]++;
    JackalRam.SpriteData4[x] = 0x38;
  }
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x10;
  subMoveSpriteToNextState(x);
}
static void Level3LaserState1(uint8_t x) {  /* :2381-2424 */
  uint8_t y = JackalRam.SpriteData5[x];
  JackalRam.SpriteData5[x]++;
  if (JackalRam.SpriteData5[x] == 3u) {
    JackalRam.SpriteData5[x] = 0;              /* :2387-2388 3 位轮换 */
  }
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = tblLaserX_LB[y];
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = tblLaserX_UB[y];
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    /* :2396-2419 双侧 spawn $0D 闪光（±Data4 偏移） */
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = JackalRam.SpriteData4[x];
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x08] = 0x0D;
    JackalSpawnZp[0x0F] = 0x03;
    JackalSpawnZp[0x35] = x;
    (void)subSpawnObjectFromParent();
    JackalSpawnZp[0x02] = (uint8_t)(0u - JackalRam.SpriteData4[x]);   /* :2413 fctInvertA */
    JackalSpawnZp[0x03] = 0xFF;
    (void)subSpawnObjectFromParent();
    JackalRam.SpriteData1[x] = 0x20;
    subMoveSpriteToNextState(x);
  }
}
static void Level3LaserState2(uint8_t x) {  /* :2436-2451 */
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteHitboxShapeIndex[x] = 0x7A;  /* :2440-2441 激光可伤人不可杀 */
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData3[x];
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteData1[x] = 0x18;
    subInitiateSoundClip(LASER_BLAST_CLIP);      /* stub */
    subMoveSpriteToNextState(x);
  }
}
static void Level3LaserState3(uint8_t x) {  /* :2453-2474 */
  JackalRam.SpriteGraphicsAttributes[x]++;
  JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteTypeIndex[x] = 0;
    JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;  /* :2465-2466 失效 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0) {
    JackalRam.SpriteData1[x] = 0x10;
    subSetSpriteState(x, 1);                     /* :2472-2473 回 State1 轮换下一位置 */
  }
}
void Level3LaserSpriteLogic(uint8_t x) {  /* :2347-2355（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level3LaserState0, Level3LaserState1, Level3LaserState2, Level3LaserState3,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- ChargingFlashes（:2489-2521） */

static void Level3LaserChargingFlashesState0(uint8_t x) {  /* :2498-2504 */
  JackalRam.SpriteData2[x] = 0x61;
  JackalRam.SpriteData1[x] = 0x10;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}
static void Level3LaserChargingFlashesState1(uint8_t x) {  /* :2506-2521 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData2[x];
  JackalRam.SpriteData1[x]--;
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] = 0x00;         /* :2513-2514 闪烁交替 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0) {
    subDespawnSprite(x);                         /* :2517 BEQ - → dw[2] */
    return;
  }
  if (JackalRam.SpriteData1[x] == 0x08u) {
    JackalRam.SpriteData2[x]++;                  /* :2520 半时换形 */
  }
}
void Level3LaserChargingFlashesSpriteLogic(uint8_t x) {  /* :2489-2496（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    Level3LaserChargingFlashesState0, Level3LaserChargingFlashesState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- SpreadTurret（:2535-2709） */

static void SpreadTurretState0(uint8_t x) {  /* :2548-2582 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.CurrentLevel == 5u ? 0xA1u : 0x50u;
  JackalRam.SpriteGraphicsAttributes[x] = 2;
  JackalRam.SpriteData1[x] = 0x08;
  JackalRam.SpriteData2[x] = (uint8_t)(x * 4u);
  JackalRam.SpriteData3[x] = 0x18;
  if (JackalRam.SpriteData4[x] != 0) {           /* boss 型（:2565-2575） */
    JackalRam.SpriteData3[x] = (uint8_t)(x * 2u + 0x10u);
    JackalRam.SpriteTypeIndex[x] = 0x63;
    JackalRam.SpriteHealthHP[x] = 0x86;
  }
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  if (JackalRam.SpriteData4[x] == 0) {
    JackalRam.SpriteHealthHP[x] = 0x86;          /* :2578-2581（ASM 注：本应分流，最终同值） */
  }
  subMoveSpriteToNextState(x);
}
static void SpreadTurretState1(uint8_t x) {  /* :2584-2611 */
  if (JackalRam.SpriteData4[x] == 0 &&
      JackalRam.CurrentLevelScreen == 0x0Bu &&
      JackalRam.CurrentLevelScreenSubPosition >= 0xE0u) {
    subDespawnSprite(x);                         /* :2594-2595 boss 屏普通炮塔消亡 */
    return;
  }
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] < 0 && JackalRam.SpriteData1[x] != 0xC0u &&
      (JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
    JackalRam.SpriteHitboxShapeIndex[x] = 0x04;  /* :2603-2605 升起档开 hitbox */
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0xC0u) {
    subMoveSpriteToNextState(x);
  }
}
static void SpreadTurretState2(uint8_t x) {  /* :2613-2659 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    if (JackalRam.SpriteData2[x] < 9u) {         /* :2620-2625 临射闪烁 */
      JackalRam.SpriteGraphicsAttributes[x]++;
      JackalRam.SpriteGraphicsAttributes[x] &= 3u;
    }
    return;
  }
  /* ++ :2628-2659 散射 5 弹 */
  JackalRam.SpriteGraphicsAttributes[x] = 2;
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    uint8_t dir = (uint8_t)(subCalculateDirectionTowardJeep(x) - 4u) & 0x1Fu;
    uint8_t n;
    JackalRam.SpriteData6[x] = dir;
    for (n = 0; n < 5; n++) {
      JackalRam.SpriteWhatDirectionToShoot[x] = JackalRam.SpriteData6[x];
      SpawnEnemyRoundBullet_Shell(x, (uint8_t)(JackalRam.SpriteData4[x] * 2u + 4u));
      JackalRam.SpriteData6[x] = (uint8_t)((JackalRam.SpriteData6[x] + 2u) & 0x1Fu);
    }
  }
  JackalRam.SpriteData1[x] = 0x10;
  subMoveSpriteToNextState(x);
}
static void SpreadTurretState3(uint8_t x) {  /* :2661-2688 */
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] < 0 && JackalRam.SpriteData1[x] != 0xC0u &&
      (JackalRam.SpriteData1[x] & 0x0Fu) == 0) {
    JackalRam.SpriteTypeIndex[x]--;              /* :2669 降回 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] != 0xC0u) {
    return;
  }
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  JackalRam.SpriteData1[x] = 0x50;
  if (JackalRam.LevelBossEntitiesRemaining < 2u) {   /* :2679-2686 残 boss 快射 */
    JackalRam.SpriteData1[x] = 0x01;
    JackalRam.SpriteData2[x] = 0x09;
  }
  subSetSpriteState(x, 1);
}
static void SpreadTurretState6(uint8_t x) {  /* :2690-2709 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  if (JackalRam.SpriteData4[x] != 0) {           /* boss 型（:2701-2708） */
    JackalRam.SpriteTypeIndex[x] = 0x66;
    subUpdateBGGraphicsFromSprite(x, 0x0E);      /* :2706-2707 战舰破洞 BG */
    JackalRam.LevelBossEntitiesRemaining--;
  }
  subDespawnSprite(x);
}
void SpreadTurretSpriteLogic(uint8_t x) {  /* :2535-2546（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    SpreadTurretState0, SpreadTurretState1, SpreadTurretState2, SpreadTurretState3,
    subCheckForBossDeath_MultipleBossEnemies, subSpriteDeath, SpreadTurretState6,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- LargeAttackBoat（:2724-2777） */

static void Level3LargeAttackBoatState0(uint8_t x) {  /* :2734-2750 */
  JackalRam.SpriteTypeIndex[x] = 0x56;
  JackalRam.SpriteData2[x] = 0x57;
  JackalRam.SpriteData1[x] = 0x60;
  JackalRam.SpriteData3[x] = 0x30;
  JackalRam.SpriteData4[x] = 0x30;
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  subCalculateObjectSpeed(x, 0x08, 0x11);        /* :2746-2749 向下 */
  subMoveSpriteToNextState(x);
}
static void Level3LargeAttackBoatState1(uint8_t x) {  /* :2752-2777 */
  JackalRam.SpriteTypeIndex[x] = 0x56;
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData2[x];
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subClearSpriteSpeed(x);
  }
  JackalRam.SpriteData3[x]--;
  if (JackalRam.SpriteData3[x] != 0) {
    return;
  }
  JackalRam.SpriteData3[x] = JackalRam.SpriteData4[x];
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;
  }
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  SpawnEnemyRoundBullet_Shell(x, 1);
}
void Level3LargeAttackBoatSpriteLogic(uint8_t x) {  /* :2724-2732（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level3LargeAttackBoatState0, Level3LargeAttackBoatState1,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level3Boss（:2792-2906+） */

/* tblLevel3BossSpreadTurretSpawn（:2861-2870） */
static const uint8_t tblLevel3BossSpreadTurretHorizontalSpawnLocationLB[6] = {
  0x70, 0xB8, 0xB8, 0xE8, 0xE8, 0x28,
};
static const uint8_t tblLevel3BossSpreadTurretHorizontalSpawnLocationUB[6] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};
static const uint8_t tblLevel3BossSpreadTurretVerticalSpawnLocation[6] = {
  0x46, 0x36, 0x56, 0x36, 0x56, 0x46,
};

static void Level3BossState0(uint8_t x) {  /* :2802-2815 */
  JackalRam.SpriteTypeIndex[x] = 0x00;
  JackalRam.SpriteData1[x] = 6;
  JackalRam.LevelBossEntitiesRemaining = 6;
  JackalRam.SpriteData2[x] = 0x18;
  JackalRam.SpriteData3[x] = 0x18;
  JackalRam.ScreenScrollingForF0ToBoss = 0;
  JackalRam.ScreenVerticalScrollLockForBossFight = 1;
  subMoveSpriteToNextState(x);
}
static void Level3BossState1(uint8_t x) {  /* :2817-2859 */
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] >= 0) {
    uint8_t y = JackalRam.SpriteData1[x];
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = tblLevel3BossSpreadTurretHorizontalSpawnLocationLB[y];
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = tblLevel3BossSpreadTurretHorizontalSpawnLocationUB[y];
    JackalRam.SpriteVertScreenPosition[x] = tblLevel3BossSpreadTurretVerticalSpawnLocation[y];
    JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
    JackalSpawnZp[0x08] = 0x92;
    JackalSpawnZp[0x09] = 0x01;                  /* boss 型标志（Data4=1） */
    JackalSpawnZp[0x0F] = 0x02;
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x35] = x;
    if (subSpawnObjectFromParent() == 0xFFu) {
      JackalRam.SpriteData1[x]++;
    }
    return;
  }
  JackalSpawnZp[0x08] = 0x46;
  if (subSpawnObjectFromParent() == 0xFFu) {
    JackalRam.SpriteData1[x]++;
    JackalRam.SpriteData2[x] = 0x01;
    return;
  }
  subMoveSpriteToNextState(x);
}
static void Level3BossState2(uint8_t x) {  /* :2872-2910+ */
  uint16_t rx;
  if (JackalRam.DifficultyBasedOnWeapon < 2u) {
    return;
  }
  JackalRam.SpriteData4[x]--;
  if (JackalRam.SpriteData4[x] != 0) {
    return;
  }
  JackalRam.SpriteData5[x]--;
  if ((JackalRam.SpriteData5[x] & 1u) != 0) {
    return;
  }
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = 0;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  JackalRam.SpriteVertScreenPosition[x] = 0xEF;
  rx = JackalRam.RNG_INCEveryFrame;
  if (rx < 0x40u) { rx = 0x40; }
  if (rx >= 0xC0u) { rx = 0xC0; }
  JackalSpawnZp[0x00] = (uint8_t)(rx * 2u);
  JackalSpawnZp[0x01] = (uint8_t)((rx * 2u) >> 8);
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x08] = 0x07;
  JackalSpawnZp[0x0F] = 0x01;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
}
void Level3BossSpriteLogic(uint8_t x) {  /* :2792-2800（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level3BossState0, Level3BossState1, Level3BossState2, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}
