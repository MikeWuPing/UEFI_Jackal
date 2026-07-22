/* core/enemy_infantry.c：敌弹 + 步兵系逐行翻译。
   $06E0（SpriteWhichJeeptoAttack）在敌弹/火焰弹上复用为寿命/计时器（敌弹无目标吉普——
   RAM_Symbols 别名 EnemyBulletLifetime，ASM 原样共用地址）。
   弹种 6 表（:7742-7773）硬编码；SwampInfantry 的 Label543/547/550/553 与 MobileInfantry
   共用（ASM 跨 Logic JMP，C 侧抽 static 共用段）。 */
#include "ram.h"
#include "enemy_infantry.h"
#include "enemy_ai.h"
#include "spawn.h"
#include "sound_stub.h"

/* tblMobileInfantryInitialMeanderingTimerValue（:379-380） */
static const uint8_t tblMobileInfantryInitialMeanderingTimerValue[6] = {
  0x00, 0xC0, 0x80, 0x40, 0x20, 0x10,
};
/* tblMobileInfantryWalkAroundJeepPath（:632-633，X 关键点 8 项） */
static const int8_t tblMobileInfantryWalkAroundJeepPath[8] = {
  0x50, 0x39, 0x00, -57, -80, -57, 0x00, 0x39,
};
/* tblMobileInfantryNoShootRadiusFromJeep（:512-513） */
static const uint8_t tblMobileInfantryNoShootRadiusFromJeep[6] = {
  0x50, 0x40, 0x30, 0x20, 0x10, 0x00,
};
/* tblStationaryInfantryPreShootFlashFrames（:515-516） */
static const uint8_t tblStationaryInfantryPreShootFlashFrames[6] = {
  0x08, 0x08, 0x10, 0x09, 0x18, 0x11,
};
/* tblStationaryInfantryShootDelay（:718-720） */
static const uint8_t tblStationaryInfantryShootDelay[6] = {
  0x80, 0x70, 0x60, 0x50, 0x40, 0x30,
};
/* 弹种表（:7742-7773）：Display/Speed/Invis/Lifetime/Attr */
static const uint8_t tblBullet_ShellSpriteDisplayIndex[16] = {
  0x10, 0x10, 0x10, 0x4A, 0x10, 0x10, 0x10, 0x4A,
  0x4A, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
};
static const uint8_t tblBullet_ShellTravelSpeed[16] = {
  0x20, 0x22, 0x31, 0x30, 0x31, 0x20, 0x31, 0x31,
  0x40, 0x31, 0x40, 0x41, 0x20, 0x21, 0x20, 0x21,
};
static const uint8_t tblBullet_ShellInvisibilityFrameCountAfterSpawn[16] = {
  0x08, 0x10, 0x0C, 0x0C, 0x06, 0x08, 0x06, 0x00,
  0x08, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t tblBullet_ShellLifetimeInFrames[16] = {
  0x30, 0x60, 0x40, 0x40, 0x60, 0x40, 0x88, 0x50,
  0x40, 0x30, 0x18, 0x24, 0x60, 0x60, 0x60, 0x60,
};
static const uint8_t tblBullet_ShellGraphicsAttributes[16] = {
  0x02, 0x02, 0x03, 0x02, 0x02, 0x02, 0x03, 0x02,
  0x02, 0x02, 0x02, 0x03, 0x02, 0x03, 0x02, 0x03,
};
/* tblFlameSpriteMirroring_SpritIndex（Bank7:8329-8345） */
static const uint8_t tblFlameSpriteMirroring_SpritIndex[8] = {
  0x00, 0x04, 0x08, 0x44, 0x40, 0xC4, 0x88, 0x84,
};

/* ---------------------------------------------------------------- 敌弹（:7618-7824） */

static void EnemyBulletState0(uint8_t x) {  /* :7628-7660 */
  JackalRam.SpriteWhatDirectionToShoot[x] &= 0x1Fu;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x],
                          JackalRam.SpriteData6[x]);
  subUpdateSpritePositionForScrolling(x);
  /* :7637-7646：寿命=$06E0（WhichJeep 复用）= 原值+难度加成 */
  {
    uint8_t y = (uint8_t)((JackalRam.DifficultyBasedOnWeapon + JackalRam.CurrentLevel) >> 1);
    static const uint8_t tblEnemyBulletAdditionalLifetimeForDifficulty[6] = {
      0x00, 0x08, 0x10, 0x18, 0x20, 0x28,
    };
    JackalRam.SpriteWhichJeeptoAttack[x] += tblEnemyBulletAdditionalLifetimeForDifficulty[y];
  }
  /* :7647-7659：L5 boss 屏（Level=4、Screen≥$0B、Sub≥$C0）Data1+=2 换型 */
  if (JackalRam.CurrentLevel == 4u && JackalRam.CurrentLevelScreen >= 0x0Bu &&
      JackalRam.CurrentLevelScreenSubPosition >= 0xC0u) {
    JackalRam.SpriteData1[x]++;
    JackalRam.SpriteData1[x]++;
  }
  subMoveSpriteToNextState(x);
}

static void EnemyBulletState1(uint8_t x) {  /* :7668-7707 */
  uint8_t hitGround = 0;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.Raw[0xD7] = 0;
  JackalRam.SpriteWhichJeeptoAttack[x]--;            /* $06E0 寿命 */
  if (JackalRam.SpriteWhichJeeptoAttack[x] == 0) {
    hitGround = 1;
  } else if ((JackalRam.SpriteWhichJeeptoAttack[x] & 1u) != 0) {
    if (JackalRam.SpriteData1[x] < 0x0Eu) {          /* :7678-7683 类型 0E/0F 不查碰撞 */
      if (fctGetCollision_WithSpeed_NearLookAhead_BG(x) == 1u) {
        hitGround = 1;
      } else if (JackalRam.CurrentLevel == 5u && JackalRam.SpriteData1[x] < 0x0Au &&
                 JackalRam.Raw[0xD7] == 3u) {        /* :7684-7691 L6 type3 */
        hitGround = 1;
      }
    }
  }
  if (hitGround) {                                   /* ++ :7699-7707 */
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData4[x];
    JackalRam.SpriteData8[x] = 0x08;
    JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
    subClearSpriteSpeed(x);
    subMoveSpriteToNextState(x);
    return;
  }
  JackalRam.SpriteData8[x]--;                        /* 隐身帧递减 → 显示 Data5 */
  if (JackalRam.SpriteData8[x] == 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  }
}

static void EnemyBulletState2(uint8_t x) {  /* :7709-7713 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] == 0) {
    subDespawnSprite(x);
  }
}

void EnemyBulletSpriteLogic(uint8_t x) {  /* :7618-7626 */
  static void (*const tbl[4])(uint8_t) = {
    EnemyBulletState0, EnemyBulletState1, EnemyBulletState2, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

void SpawnEnemyRoundBullet_Shell(uint8_t x, uint8_t y) {  /* :7715-7739 */
  JackalSpawnZp[0x08] = 0x36;
  JackalSpawnZp[0x09] = 0x11;
  JackalSpawnZp[0x0A] = tblBullet_ShellSpriteDisplayIndex[y];
  JackalSpawnZp[0x0B] = tblBullet_ShellTravelSpeed[y];
  JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
  JackalSpawnZp[0x0D] = (uint8_t)(tblBullet_ShellInvisibilityFrameCountAfterSpawn[y] + 1u);
  JackalSpawnZp[0x0E] = tblBullet_ShellLifetimeInFrames[y];
  JackalSpawnZp[0x0F] = tblBullet_ShellGraphicsAttributes[y];
  JackalSpawnZp[0x10] = y;
  if (y < 0x0Bu) {                                   /* :7736-7738 NoOffset */
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
  }
  (void)subSpawnObjectFromParent();
}

/* ---------------------------------------------------------------- 移动步兵（:334-633） */

/* Label256（:396-403）：Label244 后按 $D7 推进（共用段） */
static void label256(uint8_t x) {
  Label244(x);
  if (JackalRam.Raw[0xD7] != 0) {
    JackalRam.Raw[0x08] = 1;
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 1);
    return;                                          /* JMP subCalculateObjectSpeed 尾调用 */
  }
  subMoveSpriteToNextState(x);                       /* Label257 */
}

/* Label543（:388-403，A=$D7 入）：碰撞 → 转向+清速度 → Label256 */
static void label543(uint8_t x) {
  if (JackalRam.Raw[0xD7] == 0) {
    label256(x);
    return;
  }
  JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)(
      (JackalRam.SpriteWhatDirectionToShoot[x] + JackalRam.SpriteData2[x]) & 0x1Fu);
  subClearSpriteSpeed(x);
  label256(x);
}

static void MobileInfantryState0(uint8_t x) {  /* :349-365 */
  uint8_t r = JackalRam.RNG_INCEveryFrame;
  JackalRam.SpriteTypeIndex[x] = 0x12;
  JackalRam.SpriteGraphicsAttributes[x] = 1;
  JackalRam.SpriteData1[x] = (uint8_t)(r & 7u);
  JackalRam.SpriteData2[x] = (r & 1u) != 0 ? 1u : 0xFFu;   /* :357-362（Y=1/$FF） */
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

/* Label254（:547-628）：环绕路径目标点（Data3-6）+ 朝向/速度（mult=1） */
static void label254(uint8_t x) {
  uint8_t y = (int8_t)JackalRam.SpriteWhichJeeptoAttack[x] < 0 ? 1u : 0u;
  uint16_t jeepX = (uint16_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] +
                   JackalRam.ScreenLeftScrollPosition;
  int16_t target;
  JackalRam.SpriteData1[x] = (uint8_t)((JackalRam.SpriteData1[x] + JackalRam.SpriteData2[x]) & 7u);
  target = (int16_t)jeepX + tblMobileInfantryWalkAroundJeepPath[JackalRam.SpriteData1[x]];
  /* :582-591 钳制：UB==2 → $01FF；UB 负 → $0000 */
  if (target >= 0x200) {
    target = 0x1FF;
  } else if (target < 0) {
    target = 0;
  }
  JackalRam.SpriteData3[x] = (uint8_t)target;
  JackalRam.SpriteData4[x] = (uint8_t)(target >> 8);
  target = (int16_t)JackalRam.SpriteVertScreenPosition[0x10 + y] +
           tblMobileInfantryWalkAroundJeepPath[(uint8_t)(JackalRam.SpriteData1[x] + 6u) & 7u];
  JackalRam.SpriteData5[x] = (uint8_t)target;
  JackalRam.SpriteData6[x] = (uint8_t)(target >> 8);
  /* :611-625：WithPresets（$10-$15=目标点）+ 速度（mult=1） */
  JackalRam.Raw[0x10] = 0;
  JackalRam.Raw[0x11] = JackalRam.SpriteData3[x];
  JackalRam.Raw[0x12] = JackalRam.SpriteData4[x];
  JackalRam.Raw[0x13] = 0;
  JackalRam.Raw[0x14] = JackalRam.SpriteData5[x];
  JackalRam.Raw[0x15] = JackalRam.SpriteData6[x];
  JackalRam.Raw[0x08] = 1;
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionWithPresets(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 1);
}

static void MobileInfantryState1(uint8_t x) {  /* :367-377 */
  JackalRam.SpriteData8[x] =
      tblMobileInfantryInitialMeanderingTimerValue[
          (uint8_t)((JackalRam.DifficultyBasedOnWeapon + JackalRam.CurrentLevel) >> 1)];
  label254(x);
  subMoveSpriteToNextState(x);
}

static void MobileInfantryState2(uint8_t x) {  /* :382-387 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  (void)fctGetCollision_WithSpeed_NearLookAhead_BG(x);
  label543(x);
}

/* Label547（:423，State3/4 共用段） */
static void label547(uint8_t x) {
  if (JackalRam.Raw[0xD7] != 0) {
    subClearSpriteSpeed(x);
  }
  Label244(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] == 0) {               /* - :427-437 */
    JackalRam.SpriteData8[x] = 0x80;
    if ((JackalRam.SpriteState[x] & 0x7Fu) == 4u) {
      JackalRam.SpriteData8[x] = 0x40;
      subClearSpriteSpeed(x);
    }
    subMoveSpriteToNextState(x);
    return;
  }
  /* ++ :438-477 */
  if (JackalRam.Raw[0xD7] == 0) {
    JackalRam.Raw[0x08] = 1;
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 1);
    subSetSpriteState(x, 2);
    return;
  }
  if ((JackalRam.SpriteData8[x] & 1u) == 0) {        /* :447-448 LSR BEQ ++ */
    return;
  }
  /* 环绕点到达判断（:449-477）：X 到 → 方向 & $10|$08；Y 到 → 方向 +8 后 & $10 */
  if (JackalRam.SpriteData3[x] == JackalRam.SpriteAbsoluteHorizPositionLB[x] &&
      JackalRam.SpriteData4[x] == JackalRam.SpriteAbsoluteHorizPositionUB[x]) {
    uint8_t d = JackalRam.SpriteWhatDirectionToShoot[x];
    if ((d & 0x0Fu) == 0) {
      return;                                        /* BEQ -（:427 重装段——ASM 回跳，C 归约为 RTS 等效） */
    }
    JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)((d & 0x10u) | 0x08u);
  }
  if (JackalRam.SpriteData5[x] == JackalRam.SpriteVertScreenPosition[x] &&
      JackalRam.SpriteData6[x] == JackalRam.SpriteAbsoluteVertPositionUB[x]) {
    uint8_t d = (uint8_t)(JackalRam.SpriteWhatDirectionToShoot[x] + 0x08u);
    if ((d & 0x0Fu) == 0) {
      return;
    }
    JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)(d & 0x10u);
  }
}

static void MobileInfantryState3_4(uint8_t x) {  /* :405-422 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  if ((JackalRam.SpriteData8[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  JackalRam.Raw[0x08] = 1;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 1);
  (void)fctGetCollision_WithSpeed_NearLookAhead_BG(x);
  label547(x);
}

/* Label550（:482，State5 共用段） */
static void label550(uint8_t x) {
  uint8_t y;
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  Label244(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  y = (uint8_t)((JackalRam.DifficultyBasedOnWeapon + JackalRam.CurrentLevel) >> 1);
  JackalRam.SpriteData8[x] = tblStationaryInfantryPreShootFlashFrames[y];
  if (subCheckEnemyTarget_AttackOtherJeepIfDead(x) != 0) {
    return;                                          /* +++ C=1 RTS */
  }
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                          /* 屏外 */
  }
  if (fctGetDistanceBetweenEnemyAndJeep(x) < tblMobileInfantryNoShootRadiusFromJeep[y]) {
    return;                                          /* BCC +++ */
  }
  subMoveSpriteToNextState(x);
}

/* Label553（:521，State6 共用段） */
static void label553(uint8_t x) {
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  Label244(x);
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 7u) == 0) {        /* ++ :539 */
    SpawnEnemyRoundBullet_Shell(x, 0);
    return;
  }
  if ((int8_t)JackalRam.SpriteData8[x] >= 0) {       /* + :534-538 闪烁 */
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
    return;
  }
  if (JackalRam.SpriteData8[x] == 0xF9u) {           /* +++ :541-544 恢复+回 State1 */
    JackalRam.SpriteGraphicsAttributes[x] = 1;
    subSetSpriteState(x, 1);
  }
}

static void MobileInfantryState5(uint8_t x) {  /* :479-481 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  label550(x);
}
static void MobileInfantryState6(uint8_t x) {  /* :518-520 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  label553(x);
}

void MobileInfantrySpriteLogic(uint8_t x) {  /* :334-348（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    MobileInfantryState0, MobileInfantryState1, MobileInfantryState2,
    MobileInfantryState3_4, MobileInfantryState3_4, MobileInfantryState5,
    MobileInfantryState6, subInfantryDeath, subInfantryDeathAnimation,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 站桩步兵（:646-760） */

static void StationaryInfantryState0(uint8_t x) {  /* :657-674 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  JackalRam.SpriteGraphicsAttributes[x] = 1;
  JackalRam.SpriteData1[x] = (uint8_t)(x * 8u);      /* :662-666 槽位*8 初始延时 */
  if (JackalRam.SpriteObjectID[x] != 0x02u) {
    JackalRam.SpriteGraphicsAttributes[x] = 3;       /* 喷火 palette */
  }
  subCheckWhichJeepToAttack(x);
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void StationaryInfantryState1(uint8_t x) {  /* :676-715 */
  uint8_t y;
  subCountDownForJeepTargetBy1(x);
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  JackalRam.SpriteTypeIndex[x] = 0x12;
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalRam.SpriteData6[x] = fctGetSpriteOrientationIndex(JackalRam.SpriteWhatDirectionToShoot[x]);  /* :685-686 TYA */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  y = (uint8_t)((JackalRam.DifficultyBasedOnWeapon + JackalRam.CurrentLevel) >> 1);
  if (JackalRam.SpriteObjectID[x] != 0x02u) {
    y = 0;                                           /* 喷火固定 index（:699-700） */
  }
  JackalRam.SpriteData1[x] = tblStationaryInfantryShootDelay[y];
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    return;                                          /* 屏外（:703-704 BMI ++） */
  }
  JackalRam.SpriteData8[x] = tblStationaryInfantryPreShootFlashFrames[y];
  if (fctGetDistanceBetweenEnemyAndJeep(x) < 0x40u) {
    return;                                          /* 距离不足（:712-713 BCC ++） */
  }
  subMoveSpriteToNextState(x);
}

static void StationaryInfantryState2(uint8_t x) {  /* :722-760 */
  JackalRam.SpriteTypeIndex[x] = 0x12;
  JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalRam.SpriteData6[x] = fctGetSpriteOrientationIndex(JackalRam.SpriteWhatDirectionToShoot[x]);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] == 0xF9u) {           /* ++ :752-760 恢复+回 State1 */
    JackalRam.SpriteGraphicsAttributes[x] = 1;
    if (JackalRam.SpriteObjectID[x] != 0x02u) {
      JackalRam.SpriteGraphicsAttributes[x] = 3;
    }
    subSetSpriteState(x, 1);
    return;
  }
  if ((JackalRam.SpriteData8[x] & 7u) != 0) {        /* 闪烁（:736-742） */
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
    return;
  }
  if (JackalRam.SpriteObjectID[x] == 0x02u) {
    SpawnEnemyRoundBullet_Shell(x, 5);               /* :743-748 */
  } else {
    subSpawnFlame(x);                                /* :750 */
  }
}

void StationaryInfantrySpriteLogic(uint8_t x) {  /* :646-655（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    StationaryInfantryState0, StationaryInfantryState1, StationaryInfantryState2,
    subInfantryDeath, subInfantryDeathAnimation,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 沼泽步兵（:4457-4550） */

/* 沼泽形态判定（各态公共前缀）：碰撞==3 → TypeIndex=$74，否则 $12 */
static void swampFormCheck(uint8_t x, uint8_t typeBase) {
  if (fctGetCollisionType_SwampInfantry(x) == 3u) {
    JackalRam.SpriteTypeIndex[x] = 0x74;
  } else {
    JackalRam.SpriteTypeIndex[x] = typeBase;
  }
}

static void MobileSwampInfantryState2(uint8_t x) {  /* :4472-4488 */
  swampFormCheck(x, 0x12);
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) == 3u) {
    JackalRam.Raw[0xD7] = 0;                         /* 沼泽可行（:4482-4484） */
  }
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  label543(x);
}

static void MobileSwampInfantryState3_4(uint8_t x) {  /* :4490-4520 */
  swampFormCheck(x, 0x12);
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  (void)fctGetCollision_WithSpeed_FarLookAhead_BG(x);
  if (JackalRam.Raw[0xD7] == 3u) {                   /* :4503-4508 沼泽半速 */
    JackalRam.Raw[0xD7] = 0;
    JackalRam.Raw[0x08] = 2;
  } else {
    JackalRam.Raw[0x08] = 1;
  }
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], JackalRam.Raw[0x08]);
  if ((JackalRam.SpriteData8[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  label547(x);
}

static void MobileSwampInfantryState5(uint8_t x) {  /* :4522-4535 */
  swampFormCheck(x, 0x12);
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) == 3u) {
    JackalRam.Raw[0xD7] = 0;
  }
  label550(x);
}

static void MobileSwampInfantryState6(uint8_t x) {  /* :4537-4550 */
  swampFormCheck(x, 0x12);
  if (fctGetCollision_WithSpeed_FarLookAhead_BG(x) == 3u) {
    JackalRam.Raw[0xD7] = 0;
  }
  label553(x);
}

void MobileSwampInfantrySpriteLogic(uint8_t x) {  /* :4457-4470（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    MobileInfantryState0, MobileInfantryState1, MobileSwampInfantryState2,
    MobileSwampInfantryState3_4, MobileSwampInfantryState3_4, MobileSwampInfantryState5,
    MobileSwampInfantryState6, subInfantryDeath, subInfantryDeathAnimation,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 火焰弹（Bank7:8201-8345） */

static void FlameThrowerState0(uint8_t x) {  /* :8213-8223 */
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData6[x] = (uint8_t)(JackalRam.SpriteData5[x] + 3u);   /* :8215 $06A0,X */
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x20);
  subMoveSpriteToNextState(x);
}

static void FlameThrowerState1(uint8_t x) {  /* :8225-8244 */
  JackalRam.Raw[0xD7] = 0;
  if ((JackalRam.SpriteWhichJeeptoAttack[x] & 1u) != 0) {
    (void)fctGetCollision_WithSpeed_NearLookAhead_BG(x);
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteWhichJeeptoAttack[x]--;
  if (JackalRam.SpriteWhichJeeptoAttack[x] != 0) {
    if (JackalRam.Raw[0xD7] == 1u) {
      subDespawnSprite(x);                           /* Label1350 */
    }
    return;
  }
  JackalRam.SpriteWhichJeeptoAttack[x] = 0x30;
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  subMoveSpriteToNextState(x);
}

static void FlameThrowerState2(uint8_t x) {  /* :8246-8270 */
  JackalRam.Raw[0xD7] = 0;
  JackalRam.SpriteWhichJeeptoAttack[x]--;
  if (JackalRam.SpriteWhichJeeptoAttack[x] != 0) {
    if ((JackalRam.SpriteWhichJeeptoAttack[x] & 0x0Fu) == 0) {
      JackalRam.SpriteTypeIndex[x]++;
    }
    if ((JackalRam.SpriteWhichJeeptoAttack[x] & 1u) != 0) {
      (void)fctGetCollision_WithSpeed_NearLookAhead_BG(x);
    }
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteWhichJeeptoAttack[x] != 0 && JackalRam.Raw[0xD7] != 1u) {
    return;
  }
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData6[x];
  JackalRam.SpriteWhichJeeptoAttack[x] = 0x10;
  subMoveSpriteToNextState(x);
}

static void FlameThrowerState3(uint8_t x) {  /* :8272-8286 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteWhichJeeptoAttack[x]--;
  if (JackalRam.SpriteWhichJeeptoAttack[x] != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[x] = 0x92;
  JackalRam.Raw[0x6A + x] = 0;                       /* SpriteAttributes,X */
  JackalRam.SpriteWhichJeeptoAttack[x] = 0x20;
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  subClearSpriteSpeed(x);
  subMoveSpriteToNextState(x);
}

static void FlameThrowerState4(uint8_t x) {  /* :8288-8300 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteWhichJeeptoAttack[x]--;
  if (JackalRam.SpriteWhichJeeptoAttack[x] == 0) {
    subMoveSpriteToNextState(x);                     /* → state5 = subDespawnSprite */
    return;
  }
  if ((JackalRam.SpriteWhichJeeptoAttack[x] & 7u) == 0) {
    JackalRam.Raw[0x6A + x] = (uint8_t)((JackalRam.Raw[0x6A + x] + 0x40u) & 0x40u);
  }
}

void FlameThrowerSpriteLogic(uint8_t x) {  /* Bank7:8201-8211 */
  static void (*const tbl[6])(uint8_t) = {
    FlameThrowerState0, FlameThrowerState1, FlameThrowerState2,
    FlameThrowerState3, FlameThrowerState4, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

void subSpawnFlame(uint8_t x) {  /* Bank7:8302-8327 */
  uint8_t y = fctGetSpriteOrientationIndex(JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalSpawnZp[0x08] = 0x34;
  JackalSpawnZp[0x0C] = (uint8_t)(y * 4u);           /* :8308-8310 */
  JackalSpawnZp[0x0F] = (uint8_t)(tblFlameSpriteMirroring_SpritIndex[y] & 0xF0u);
  JackalSpawnZp[0x0A] = (uint8_t)((tblFlameSpriteMirroring_SpritIndex[y] & 0x0Fu) + 0x86u);
  JackalSpawnZp[0x0E] = JackalRam.SpriteObjectID[x] == 0x03u ? 0x08u : 0x10u;
  JackalSpawnZp[0x00] = 0;                           /* NoOffset */
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  (void)subSpawnObjectFromParent();
}
