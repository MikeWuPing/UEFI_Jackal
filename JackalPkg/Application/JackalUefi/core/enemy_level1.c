/* core/enemy_level1.c：Level 1 完整化逐行翻译（Bank6:775-1255）。 */
#include "ram.h"
#include "enemy_level1.h"
#include "enemy_infantry.h"
#include "enemy_tank.h"
#include "enemy_ai.h"
#include "spawn.h"
#include "sound_stub.h"

/* tblLevel1BossTankSpawnDelay（:938-942，按难度） */
static const uint8_t tblLevel1BossTankSpawnDelay[4] = { 0xF0, 0xD0, 0xB0, 0x90 };
/* tblEndofLevelCheckBossID（:803-809） */
static const uint8_t tblEndofLevelCheckBossID[6] = { 0x0A, 0x18, 0x92, 0xC0, 0x26, 0x4B };

/* ---------------------------------------------------------------- AttackBoat（:840-898） */

static void Level1AttackBoatState0(uint8_t x) {  /* :850-866 */
  JackalRam.SpriteTypeIndex[x] = 0x3A;
  JackalRam.SpriteData2[x] = 0x3B;
  JackalRam.SpriteData1[x] = 0x60;
  JackalRam.SpriteData3[x] = 0x30;
  JackalRam.SpriteData4[x] = 0x30;
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  subCalculateObjectSpeed(x, 0x0C, 0x10);            /* :862-865 down-left、mult=$10 */
  subMoveSpriteToNextState(x);
}
static void Level1AttackBoatState1(uint8_t x) {  /* :868-898 */
  JackalRam.SpriteTypeIndex[x] = 0x3A;
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData2[x];   /* :875-876 动画帧 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subClearSpriteSpeed(x);                          /* :881-882 行进停止 */
  }
  JackalRam.SpriteData3[x]--;
  if (JackalRam.SpriteData3[x] != 0) {
    return;
  }
  JackalRam.SpriteData3[x] = JackalRam.SpriteData4[x];
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                          /* :887-888 屏外不射 */
  }
  JackalRam.SpriteWhatDirectionToShoot[x] =
      (uint8_t)(fctGetSpriteOrientationIndex(subCalculateDirectionTowardJeep(x)) * 4u);
  SpawnEnemyRoundBullet_Shell(x, 1);
}
void Level1AttackBoatSpriteLogic(uint8_t x) {  /* :840-848（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level1AttackBoatState0, Level1AttackBoatState1, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level1Boss 生成器（:913-996） */

static void Level1BossState0(uint8_t x) {  /* :922-936 */
  JackalRam.SpriteData1[x] = 4;
  JackalRam.LevelBossEntitiesRemaining = 4;
  JackalRam.SpriteData2[x] = tblLevel1BossTankSpawnDelay[JackalRam.DifficultyBasedOnWeapon];
  JackalRam.SpriteData3[x] = JackalRam.SpriteData2[x];
  JackalRam.ScreenScrollingForF0ToBoss = 0;          /* :931-934 锁滚 */
  JackalRam.ScreenVerticalScrollLockForBossFight = 1;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level1BossState1(uint8_t x) {  /* :944-996 */
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];   /* :947-948 重装 */
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] >= 0) {
    uint16_t rx;
    /* :951-961 生成器归位（不可见对象置原点便于 spawn 偏移） */
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = 0;
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0;
    JackalRam.SpriteVertScreenPosition[x] = 0;
    JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
    if ((JackalRam.SpriteData1[x] & 1u) != 0) {
      JackalRam.SpriteVertScreenPosition[x] = 0xEF;    /* 交替底部生成 */
    }
    /* :962-973 RNG 钳制 $50-$B0、*2 横坐标（16 位） */
    rx = JackalRam.RNG_INCEveryFrame;
    if (rx < 0x50u) { rx = 0x50; }
    if (rx >= 0xB0u) { rx = 0xB0; }
    JackalSpawnZp[0x00] = (uint8_t)(rx * 2u);
    JackalSpawnZp[0x01] = (uint8_t)((rx * 2u) >> 8);
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x08] = 0x0A;
    JackalSpawnZp[0x0F] = 0x02;                      /* palette 2 */
    JackalSpawnZp[0x35] = x;
    if (subSpawnObjectFromParent() == 0xFFu) {
      JackalRam.SpriteData1[x]++;                    /* :984-985 无槽重试 */
    }
    return;
  }
  /* :987-996 用尽 → spawn $46 → State+1（despawn） */
  JackalSpawnZp[0x08] = 0x46;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  if (subSpawnObjectFromParent() == 0xFFu) {
    JackalRam.SpriteData1[x]++;
    JackalRam.SpriteData2[x]++;
    return;
  }
  subMoveSpriteToNextState(x);
}

void Level1BossSpriteLogic(uint8_t x) {  /* :913-920（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    Level1BossState0, Level1BossState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level1BossTank（:1011-1152） */

/* subLevel1BossTankUpdatePalette_LowHealth（:1132-1152） */
static void subLevel1BossTankUpdatePalette_LowHealth(uint8_t x) {
  if ((uint8_t)(JackalRam.SpriteHealthHP[x] & 0x3Fu) < 6u) {
    JackalRam.SpriteGraphicsAttributes[x] = 1;       /* 残血变色 */
  }
  if ((uint8_t)(JackalRam.SpriteTypeIndex[x] - 0x33u) == 1u &&
      (int8_t)JackalRam.Raw[0x6A + x] < 0) {         /* :1142-1147 受击变形恢复 */
    JackalRam.Raw[0x6A + x] &= 0x7Fu;
    JackalRam.SpriteTypeIndex[x] += 5;
  }
}

static void Level1BossTankState0(uint8_t x) {  /* :1026-1052 */
  JackalRam.SpriteTypeIndex[x] = 0x33;
  JackalRam.SpriteData6[x] = 0x33;
  subCheckWhichJeepToAttack(x);
  if ((int8_t)JackalRam.SpriteVertScreenPosition[x] < 0) {
    JackalRam.SpriteData1[x] = 0x18;                 /* 底部生成朝上 */
    JackalRam.SpriteWhatDirectionToShoot[x] = 0x18;
  } else {
    JackalRam.SpriteData1[x] = 0x30;                 /* 默认朝下 */
    JackalRam.SpriteWhatDirectionToShoot[x] = 0x08;
  }
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  subLevel1BossTankUpdatePalette_LowHealth(x);
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData8[x] = 0;
  JackalRam.SpriteData5[x] = 0x0F;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x20);
  /* Label299（:188-196）：Data4=槽奇偶 ±4、State+1 */
  JackalRam.SpriteData4[x] = (x & 1u) != 0 ? 0xFCu : 0x04u;
  subMoveSpriteToNextState(x);
}

static void Level1BossTankState1(uint8_t x) {  /* :1054-1065 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) != 0 || label247(x) != 0) {
    JackalRam.SpriteData1[x] = 0x08;                 /* + :1062 重装（碰撞） */
    return;
  }
  subMoveSpriteToNextState(x);                       /* +++（Label247 CLC） */
}

static void Level1BossTankState2(uint8_t x) {  /* :1067-1082 */
  uint8_t z08 = (uint8_t)(label228(x) << 7);         /* :1069-1071 LDA #0 ROR STA $08 */
  if (JackalRam.SpriteObjectID[x] != 0) {
    subLevel1BossTankUpdatePalette_LowHealth(x);
  }
  if ((int8_t)z08 < 0) {                             /* SEC → 行进 */
    JackalRam.SpriteData1[x] = 0x28;
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x20);
    subMoveSpriteToNextState(x);
  }
  /* CLC（$08 bit7=0）→ RTS */
}

static void Level1BossTankState3(uint8_t x) {  /* :1084-1090 */
  subTurnTankTowardsJeep(x);                        /* :1085 */
  if (JackalRam.SpriteObjectID[x] != 0) {           /* :1086-1088 */
    subLevel1BossTankUpdatePalette_LowHealth(x);
  }
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x20);  /* :1089-1090 */
  if (JackalRam.Raw[0xD7] == 0) {                   /* :220-222 BEQ + → State4 */
    subMoveSpriteToNextState(x);
  }
}

static void Level1BossTankState4(uint8_t x) {  /* :1092-1123 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  subLevel1BossTankUpdatePalette_LowHealth(x);
  JackalRam.SpriteData1[x]--;
  if ((JackalRam.SpriteData1[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;               /* :1101-1105 行进动画 */
  }
  JackalRam.SpriteData1[x]++;                        /* :1106 INC（净效应：&8 抽样帧） */
  label232(x);
  if (JackalRam.Raw[0xD7] != 0) {                    /* ++ :1118-1123 */
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
    subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
    subSetSpriteState(x, 2);
    return;
  }
  if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0 && (int8_t)JackalRam.SpriteState[x] >= 0) {
    SpawnEnemyRoundBullet_Shell(x, 2);               /* :1115-1116 */
  }
}

static void Level1BossTankState7(uint8_t x) {  /* :1125-1130 */
  subSpriteExplosion(x);
  if (JackalRam.SpriteObjectID[x] == 0) {
    JackalRam.LevelBossEntitiesRemaining--;          /* :1129 一坦克死计数 */
  }
}

void Level1BossTankSpriteLogic(uint8_t x) {  /* :1011-1024（dw 8 项） */
  static void (*const tbl[8])(uint8_t) = {
    Level1BossTankState0, Level1BossTankState1, Level1BossTankState2, Level1BossTankState3,
    Level1BossTankState4, subCheckForBossDeath_MultipleBossEnemies, subSpriteDeath,
    Level1BossTankState7,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- EndofLevelCheck（:775-800） */

static void EndofLevelState0(uint8_t x) {  /* :783-800 */
  int8_t i;
  if ((uint8_t)(x + JackalRam.RNG_INCEveryFrame) & 0x0Fu) {
    return;                                          /* :786-788 非每帧（槽位+RNG&$0F==0 才查） */
  }
  for (i = 0x0F; i >= 0; i--) {
    if (JackalRam.SpriteObjectID[i] == tblEndofLevelCheckBossID[JackalRam.CurrentLevel]) {
      return;                                        /* :794 boss 实体在场 */
    }
  }
  JackalRam.LevelBossEntitiesRemaining = 0;          /* :797-798 关卡完成 */
}

void EndofLevelCheckSpriteLogic(uint8_t x) {  /* :775-781（dw 2 项） */
  static void (*const tbl[2])(uint8_t) = { EndofLevelState0, subDespawnSprite };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}
