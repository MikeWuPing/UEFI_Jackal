/* core/enemy_level2.c：Level 2 群逐行翻译（Bank6:1513-2063）。 */
#include "ram.h"
#include "enemy_level2.h"
#include "enemy_building.h"
#include "enemy_infantry.h"
#include "enemy_tank.h"
#include "enemy_ai.h"
#include "spawn.h"
#include "sound_stub.h"

/* ---------------------------------------------------------------- Level2Pillar（:1513-1595） */

static void Level2PillarState0(uint8_t x) {  /* :1526-1541 */
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x05;                   /* BG 更新 index（broken pillar） */
  JackalRam.SpriteData2[x] = 0x80;                   /* :1530-1531（ASM 注：无实际作用） */
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x06;    /* 默认右倒 */
  if (JackalRam.SpriteObjectID[x] != 0x1Fu) {        /* :1534-1538 非右倒 → 左倒 */
    JackalRam.SpriteWhatDirectionToShoot[x] = 0x0A;
  }
  JackalRam.SpriteData4[x] = 0x49;                   /* :1539-1540（ASM 注：无实际作用） */
  subMoveSpriteToNextState(x);
}

static void Level2PillarState1(uint8_t x) {  /* :1543-1578 */
  uint8_t dir;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((JackalRam.RNG_INCEveryFrame & 3u) != 0) {
    return;                                          /* :1546-1548 每 4 帧才查 */
  }
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                          /* 屏外 */
  }
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  dir = subCalculateDirectionTowardJeep(x);
  if (dir < 0x06u || dir >= 0x0Bu) {                 /* :1553-1557 方向不在 6-B → 换目标再查 */
    JackalRam.SpriteWhichJeeptoAttack[x] += 0x80u;
    if (subCheckEnemyTarget_AttackOtherJeepIfDead(x) != 0) {
      return;                                        /* :1565 BCS +++ */
    }
    dir = subCalculateDirectionTowardJeep(x);
    if (dir < 0x06u || dir >= 0x0Bu) {
      return;
    }
  }
  if (fctGetDistanceBetweenEnemyAndJeep(x) >= 0x70u) {   /* :1575-1577 距离门 */
    return;
  }
  subMoveSpriteToNextState(x);                       /* 破坏倒塌 */
}

static void Level2PillarState4(uint8_t x) {  /* :1580-1595 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalSpawnZp[0x08] = 0x1E;                        /* PillarTop */
  JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
  JackalSpawnZp[0x0F] = (JackalRam.SpriteWhatDirectionToShoot[x] & 8u) != 0 ? 0x41u : 0x01u;  /* :1589-1593 左倒镜像 */
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
  subMoveSpriteToNextState(x);                       /* :1595（驻留补 BG 更新） */
}

void Level2PillarSpriteLogic(uint8_t x) {  /* :1513-1524（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    Level2PillarState0, Level2PillarState1,
    subPlayMissileHittingGroundSound_LoadHoleInGroundBGGraphics,
    subProcessExplosionAnimation, Level2PillarState4,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level2PillarTop（:1609-1688） */

static void Level2PillarTopState0(uint8_t x) {  /* :1620-1634 */
  JackalRam.SpriteTypeIndex[x] = 0x47;
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x20;
  JackalRam.SpriteData2[x] = 0x30;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x31);
  JackalRam.SpriteData4[x] = 0x49;
  subMoveSpriteToNextState(x);
}

static void Level2PillarTopState1(uint8_t x) {  /* :1636-1660 */
  JackalRam.SpriteData1[x]--;
  if ((JackalRam.SpriteData1[x] & 3u) == 0) {
    subInvertSpriteVertSpeed(x);                     /* :1640-1641 zigzag */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] != 0) {
    if ((JackalRam.SpriteData1[x] & 3u) == 0) {
      subInvertSpriteVertSpeed(x);                   /* :1647（ASM 双次反转移回？——照翻） */
    }
    return;
  }
  /* :1648-1658 斜落完成 → 地面滚动 */
  subClearSpriteHorizSpeed(x);
  JackalRam.SpriteVertSpeedLB[x] = 0xC0;
  JackalRam.SpriteVertSpeedUB[x] = 0x01;
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x08;
  JackalRam.SpriteTypeIndex[x]++;
  subMoveSpriteToNextState(x);
}

static void Level2PillarTopState2(uint8_t x) {  /* :1662-1688 */
  JackalRam.SpriteTypeIndex[x] = 0x48;
  JackalRam.SpriteData2[x]--;
  if ((JackalRam.SpriteData2[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData4[x];   /* :1670-1671 滚动动画交替 */
  }
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) != 0) {
    subClearSpriteSpeed(x);
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.Raw[0xD7] == 0 && JackalRam.SpriteData2[x] != 0) {
    return;                                          /* - :1679 RTS */
  }
  /* :1680-1687 滚动完成 → 碰撞停留（Label247 SEC：贴其他对象才停，EnemyPoints=0 防 farming） */
  JackalRam.SpriteData4[x] = 0x48;
  subClearSpriteVertSpeed(x);
  if (label247(x) == 0) {                            /* :1684 BCC + RTS（无碰撞不停留） */
    return;
  }
  JackalRam.EnemyPoints[x] = 0;
  subMoveSpriteToNextState(x);
}

void Level2PillarTopSpriteLogic(uint8_t x) {  /* :1609-1618（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    Level2PillarTopState0, Level2PillarTopState1, Level2PillarTopState2,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level2BossStatueHead（:1703-1802） */

static void Level2BossStatueHeadState0(uint8_t x) {  /* :1718-1727 */
  JackalRam.SpriteData2[x] = 0xC0;
  JackalRam.SpriteData3[x] = 0xC0;
  JackalRam.SpriteData1[x] = 0x06;                   /* boss 头 destroyed BG index */
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x08;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level2BossStatueHeadState1(uint8_t x) {  /* :1729-1743 */
  if (JackalRam.SpriteVertScreenPosition[x] < 0xE0u) {
    subUpdateBGGraphicsFromSprite(x, 0x0B);          /* 嘴闭 BG */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] == 0) {
    JackalRam.SpriteData2[x] = 0x18;                 /* 闪眼时间 */
    subMoveSpriteToNextState(x);
  }
}

static void Level2BossStatueHeadState2(uint8_t x) {  /* :1745-1762 */
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteVertScreenPosition[x] < 0xE0u) {
    if ((JackalRam.SpriteData2[x] & 2u) != 0) {
      subUpdateBGGraphicsFromSprite(x, 0x08);        /* 亮眼 */
    } else {
      subUpdateBGGraphicsFromSprite(x, 0x09);        /* 常眼 */
    }
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData2[x] == 0) {
    JackalRam.SpriteData2[x] = 0x18;
    subMoveSpriteToNextState(x);
  }
}

static void Level2BossStatueHeadState3(uint8_t x) {  /* :1764-1797 */
  if (JackalRam.SpriteVertScreenPosition[x] < 0xE0u) {
    subUpdateBGGraphicsFromSprite(x, 0x0A);          /* 张嘴 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    /* :1777-1795 spawn 导弹（homing，偏移 Y=$10） */
    JackalSpawnZp[0x08] = 0x39;
    JackalSpawnZp[0x0A] = 0x4B;
    JackalSpawnZp[0x0B] = 0x01;
    JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
    JackalSpawnZp[0x0F] = 0x00;
    JackalSpawnZp[0x00] = 0x00;
    JackalSpawnZp[0x01] = 0x00;
    JackalSpawnZp[0x02] = 0x10;
    JackalSpawnZp[0x03] = 0x00;
    JackalSpawnZp[0x35] = x;
    (void)subSpawnObjectFromParent();
  }
  subSetSpriteState(x, 1);
}

static void Level2BossStatueHeadState7(uint8_t x) {  /* :1799-1802 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.LevelBossEntitiesRemaining--;
  subMoveSpriteToNextState(x);
}

void Level2BossStatueHeadSpriteLogic(uint8_t x) {  /* :1703-1716（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    Level2BossStatueHeadState0, Level2BossStatueHeadState1, Level2BossStatueHeadState2,
    Level2BossStatueHeadState3, subCheckForBossDeath_MultipleBossEnemies,
    subPlayExplosionSound_LoadNewBGGraphics, subProcessExplosionAnimation,
    Level2BossStatueHeadState7, subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level2StatueHead（:1817-1904） */

static void Level2StatueHeadState0(uint8_t x) {  /* :1830-1851 */
  JackalRam.SpriteData2[x] = (uint8_t)(x * 4u);      /* :1831-1834 槽位*4 初始延时 */
  JackalRam.SpriteData3[x] = 0x40;
  JackalRam.SpriteData4[x] = (uint8_t)(JackalRam.SpriteObjectID[x] - 0xA1u);  /* :1838-1840（$21→0 射击/$22→1 idle） */
  JackalRam.SpriteData1[x] = 0x07;                   /* destroyed statue head BG index */
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x04;
  if (JackalRam.SpriteAbsoluteHorizPositionUB[x] != 0) {
    JackalRam.SpriteWhatDirectionToShoot[x] = 0x0C;  /* 右屏 → 下左 */
  }
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level2StatueHeadState1(uint8_t x) {  /* :1853-1868 */
  if (JackalRam.SpriteVertScreenPosition[x] < 0xE0u) {
    subUpdateBGGraphicsFromSprite(x, 0x0B);          /* 嘴闭 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData4[x] != 0 &&
      JackalRam.DifficultyBasedOnWeapon < 2u) {      /* :1861-1865 idle 头（$22）且难度<2 → 不射击 */
    return;
  }
  JackalRam.SpriteData2[x]--;                        /* lblLevel2StatueStateTimer（:1736-1741） */
  if (JackalRam.SpriteData2[x] == 0) {
    JackalRam.SpriteData2[x] = 0x18;
    subMoveSpriteToNextState(x);
  }
}

static void Level2StatueHeadState3(uint8_t x) {  /* :1870-1904 */
  if (JackalRam.SpriteVertScreenPosition[x] < 0xE0u) {
    subUpdateBGGraphicsFromSprite(x, 0x0A);          /* 张嘴 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    /* :1884-1902 spawn 导弹（直射，偏移 Y=$10） */
    JackalSpawnZp[0x08] = 0x39;
    JackalSpawnZp[0x0A] = 0x4B;
    JackalSpawnZp[0x0B] = 0x00;
    JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
    JackalSpawnZp[0x0F] = 0x00;
    JackalSpawnZp[0x00] = 0x00;
    JackalSpawnZp[0x01] = 0x00;
    JackalSpawnZp[0x02] = 0x10;
    JackalSpawnZp[0x03] = 0x00;
    JackalSpawnZp[0x35] = x;
    (void)subSpawnObjectFromParent();
  }
  subSetSpriteState(x, 1);
}

void Level2StatueHeadSpriteLogic(uint8_t x) {  /* :1817-1828（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    Level2StatueHeadState0, Level2StatueHeadState1, Level2BossStatueHeadState2,
    Level2StatueHeadState3, subPlayExplosionSound_LoadNewBGGraphics,
    subProcessExplosionAnimation, subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Level2Boss（:1918-2029） */

/* tblLevel2BossStatueHeadHorizontalSpawnPositionLB/UB（:1980-1985） */
static const uint8_t tblLevel2BossStatueHeadHorizontalSpawnPositionLB[4] = {
  0xBC, 0x0C, 0xE4, 0x34,
};
static const uint8_t tblLevel2BossStatueHeadHorizontalSpawnPositionUB[4] = {
  0x00, 0x01, 0x00, 0x01,
};

static void Level2BossState0(uint8_t x) {  /* :1928-1943 */
  JackalRam.SpriteTypeIndex[x] = 0x00;               /* 不可见 */
  JackalRam.SpriteData1[x] = 4;
  JackalRam.LevelBossEntitiesRemaining = 4;
  JackalRam.SpriteData2[x] = 0x18;
  JackalRam.SpriteData3[x] = 0x18;
  JackalRam.ScreenScrollingForF0ToBoss = 0;
  JackalRam.ScreenVerticalScrollLockForBossFight = 1;
  JackalRam.SpriteVertScreenPosition[x] = 0x1C;      /* :1942 头部 spawn 行 */
  subMoveSpriteToNextState(x);
}

static void Level2BossState1(uint8_t x) {  /* :1945-1978 */
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] >= 0) {
    uint8_t y = JackalRam.SpriteData1[x];
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = tblLevel2BossStatueHeadHorizontalSpawnPositionLB[y];
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = tblLevel2BossStatueHeadHorizontalSpawnPositionUB[y];
    JackalSpawnZp[0x08] = 0x18;
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x35] = x;
    if (subSpawnObjectFromParent() == 0xFFu) {
      JackalRam.SpriteData1[x]++;                    /* :1964 重试 */
    }
    return;
  }
  /* :1967-1977 用尽 → spawn $46（快重试 Data2=1） */
  JackalSpawnZp[0x08] = 0x46;
  if (subSpawnObjectFromParent() == 0xFFu) {
    JackalRam.SpriteData1[x]++;
    JackalRam.SpriteData2[x] = 0x01;
    return;
  }
  subMoveSpriteToNextState(x);
}

static void Level2BossState2(uint8_t x) {  /* :1987-2029 */
  uint16_t rx;
  if (JackalRam.DifficultyBasedOnWeapon < 2u) {
    return;                                          /* :1990 BCC - RTS */
  }
  JackalRam.SpriteData4[x]--;
  if (JackalRam.SpriteData4[x] != 0) {
    return;
  }
  JackalRam.SpriteData5[x]++;
  if ((JackalRam.SpriteData5[x] & 1u) != 0) {
    return;                                          /* :1996 BNE -- RTS（每 $200 帧一次） */
  }
  /* :1998-2024 底部 spawn 红坦克（RNG 钳制横坐标，EnemyPoints=0 防 farming） */
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = 0;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  JackalRam.SpriteVertScreenPosition[x] = 0xEF;
  rx = JackalRam.RNG_INCEveryFrame;
  if (rx < 0x50u) { rx = 0x50; }
  if (rx >= 0xB0u) { rx = 0xB0; }
  JackalSpawnZp[0x00] = (uint8_t)(rx * 2u);
  JackalSpawnZp[0x01] = (uint8_t)((rx * 2u) >> 8);
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x08] = 0x07;
  JackalSpawnZp[0x0F] = 0x01;
  JackalSpawnZp[0x35] = x;
  {
    uint8_t slot = subSpawnObjectFromParent();
    if (slot != 0xFFu) {
      JackalRam.EnemyPoints[slot] = 0;               /* :2027-2028 */
    }
  }
}

void Level2BossSpriteLogic(uint8_t x) {  /* :1918-1926（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level2BossState0, Level2BossState1, Level2BossState2, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}
