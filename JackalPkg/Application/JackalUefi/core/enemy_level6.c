/* core/enemy_level6.c：Level 6 群 + 通用载具逐行翻译
   （Bank6:20-157 BW 导弹、:1256-1317 AttackPlane、:3382-3395 GraphicsLoad、
   :3411-3612 FinalBossTank+Label462/469+残血 palette、:3628-3728 FlameShot/Tip、
   :3744-3967 BossTankTurret+Label486、:6093-6212 直升机、:6227-6302 导弹井、
   :6317-6601 激光炮塔/弹/BossLoad、:6607-6747 FinalBoss+Label672；
   Bank7:1530-1537 擦标志、:6808-6831 机/直定位、:8154-8196 电扶梯、:8347-8532
   Label474/建筑过渡/Label1364/清场）。
   $D5（Level6BossTurretStatus）：MSB=换 palette、LSB=存活（双激光炮塔+坦克炮塔共用）。 */
#include "enemy_level6.h"
#include "ram.h"
#include "enemy_ai.h"
#include "enemy_tank.h"
#include "enemy_infantry.h"
#include "enemy_building.h"
#include "spawn.h"
#include "ppu.h"
#include "sound_stub.h"

#define TBL_ENEMY_POINTS_DEATHSTATE_CPU 0xFA96u  /* :8065，0x54 项 */
#include "bank.h"

#define BW_MISSILE_EXPLODE_CLIP 0x15u  /* BlackAndWhiteMissile_Bomb_LaserBlastHitsGroundSoundClip */
#define BOMBER_ROAR_CLIP        0x0Au  /* BomberPlaneRoarSoundClip */
#define HELI_BLADES_CLIP        0x0Cu  /* HelicopterBladesWhirringSoundClip */
#define LASER_BLAST_CLIP        0x0Bu  /* LaserBlastSoundClip */
#define FLAME_SHOT_CLIP         0x19u  /* Level6FinalBossTankFlameShotSoundClip */
#define FINAL_BOSS_MUSIC        0x48u  /* Level6FinalBossMusic */
#define FINAL_BOSS_BLOWUP_CLIP  0x27u  /* Level6FinalBossBuildingBlowingUpSoundClip */

/* ---------------------------------------------------------------- BW 小导弹（:20-144，ID $39）
   弹种（Data6）：0=石像直射、1=L2 boss 追踪、2=发射井/悬崖、3=潜艇（永不引爆） */

static const uint8_t tblBlackAndWhiteMissileSpeedMultiplier[4] =      { 0x40, 0x30, 0x40, 0x70 };
static const uint8_t tblBlackAndWhiteMissileTurningRadius[4] =        { 0x10, 0x0C, 0x06, 0x10 };
static const uint8_t tblBlackAndWhiteMissileInitialTurningRadius[4] = { 0x38, 0x0C, 0x10, 0x00 };
static const uint8_t tblBlackAndWhiteMissileLifetime[4] =             { 0x01, 0x28, 0x40, 0x01 };

static void BlackAndWhiteMissileState0(uint8_t x) {  /* :31-55 */
  uint8_t y;
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  JackalRam.SpriteGraphicsAttributes[x] = 0x02;
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  subUpdateSpritePositionForScrolling(x);
  y = JackalRam.SpriteData6[x];
  JackalRam.SpriteData2[x] = tblBlackAndWhiteMissileTurningRadius[y];
  JackalRam.SpriteData1[x] = tblBlackAndWhiteMissileInitialTurningRadius[y];
  JackalRam.SpriteData3[x] = 0x1F;                     /* :44 注：可硬编码 */
  JackalRam.SpriteData4[x] = tblBlackAndWhiteMissileLifetime[y];
  JackalRam.SpriteData8[x] = 0;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x],
                          tblBlackAndWhiteMissileSpeedMultiplier[y]);
  subCheckWhichJeepToAttack(x);
  subMoveSpriteToNextState(x);
}

static void BlackAndWhiteMissileState1(uint8_t x) {  /* :71-132 */
  uint8_t diff;
  subCountDownForJeepTargetBy1(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    goto move;                                         /* :74 BNE +++ */
  }
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & JackalRam.SpriteData3[x]) == 0) {
    JackalRam.SpriteData2[x]--;                        /* :79 每满一圈半径递减 */
  }
  JackalRam.SpriteData1[x] = JackalRam.SpriteData2[x];
  subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  diff = (uint8_t)(subCalculateDirectionTowardJeep(x) -
                   JackalRam.SpriteWhatDirectionToShoot[x]);
  if (diff != 0) {                                     /* :89 BEQ ++ */
    if ((diff & 0x10u) != 0) {
      JackalRam.SpriteWhatDirectionToShoot[x] =
          (uint8_t)((JackalRam.SpriteWhatDirectionToShoot[x] - 1u) & 0x1Fu);
    } else {
      JackalRam.SpriteWhatDirectionToShoot[x] =
          (uint8_t)((JackalRam.SpriteWhatDirectionToShoot[x] + 1u) & 0x1Fu);
    }
  }
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x],
                          tblBlackAndWhiteMissileSpeedMultiplier[JackalRam.SpriteData6[x]]);
move:
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData8[x] == JackalRam.SpriteData4[x]) {  /* :118 寿命尽 */
    JackalRam.SpriteData1[x] = 0x10;                   /* 爆炸计时 */
    JackalRam.SpriteHitboxShapeIndex[x] = 0x74;        /* :122 不可杀但爆炸仍伤人 */
    JackalRam.SpriteGraphicsAttributes[x] = 0x03;      /* 黄红 palette */
    JackalRam.SpriteTypeIndex[x] = 0x19;
    subClearSpriteSpeed(x);
    subInitiateSoundClip(BW_MISSILE_EXPLODE_CLIP);     /* stub */
    subMoveSpriteToNextState(x);
  }
}

static void BlackAndWhiteMissileState2(uint8_t x) {  /* :134-144 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  /* :136 注：此处缺消亡复查（原游戏 quirk，照译） */
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subDespawnSprite(x);
    return;
  }
  if ((JackalRam.SpriteData1[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
}

void BlackAndWhite_SmallMissileSpriteLogic(uint8_t x) {  /* :20-29（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    BlackAndWhiteMissileState0, BlackAndWhiteMissileState1, BlackAndWhiteMissileState2,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- AttackPlane（:1256-1317，ID $3A/$3B） */

void subSpawnPlane_HeliPositionRelativeToJeep(uint8_t x) {  /* Bank7:6808-6831 */
  uint8_t y = (JackalRam.SpriteWhichJeeptoAttack[x] & 0x80u) ? 1u : 0u;
  uint8_t off = 0x20;                                  /* 左半屏 +$20 */
  uint16_t acc;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0;
  if ((int8_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] < 0) {
    off = 0xE0;                                        /* 右半屏 -$20 */
  }
  /* :6823-6828 两次 CLC 各自独立（第一次进位被第二个 CLC 吃掉） */
  acc = (uint8_t)(JackalRam.SpriteHorizScreenPosition[0x10 + y] + off);
  acc = (uint16_t)((uint8_t)acc + 0x40u);
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)acc;
  if (acc > 0xFFu) {
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = 1;    /* INC（只在末次进位） */
  }
}

static void AttackPlaneState0(uint8_t x) {  /* :1266-1291 */
  subCheckWhichJeepToAttack(x);
  if (JackalRam.SpriteObjectID[x] != 0x3Au) {
    subSpawnPlane_HeliPositionRelativeToJeep(x);
  }
  JackalRam.SpriteTypeIndex[x] = 0x4E;
  JackalRam.SpriteData6[x] = 0x08;                     /* 下落 */
  if ((int8_t)JackalRam.SpriteVertScreenPosition[x] < 0) {
    JackalRam.Raw[0x6A + x] = 0x80;                    /* 底部 spawn：V 镜像 */
    JackalRam.SpriteData6[x] = 0x18;                   /* 上飞 */
  }
  subCalculateObjectSpeed(x, JackalRam.SpriteData6[x], 0x40);
  subUpdateSpritePositionForScrolling(x);
  if ((int8_t)JackalRam.SpriteState[x] < 0) {
    subDespawnSprite(x);                               /* :1290 屏外 spawn 即弃 */
    return;
  }
  subMoveSpriteToNextState(x);
}

static void AttackPlaneState1(uint8_t x) {  /* :1293-1317 */
  if ((JackalRam.SpriteData1[x] & 3u) == 0) {
    subInitiateSoundClip(BOMBER_ROAR_CLIP);            /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = 0x4E;
  JackalRam.SpriteData1[x]++;
  if ((JackalRam.SpriteData1[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;                    /* :1305 影子闪烁 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] == 0x10u || JackalRam.SpriteData1[x] == 0x30u ||
      JackalRam.SpriteData1[x] == 0x50u || JackalRam.SpriteData1[x] == 0x70u) {
    subSpawnJeep_BomberBomb(x);
  }
}

void AttackPlaneSpriteLogic(uint8_t x) {  /* :1256-1264（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    AttackPlaneState0, AttackPlaneState1, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- L6 攻击直升机（:6093-6212，ID $43） */

static const uint8_t tblLevel6AttackHelicopterEnterScreenSpeedFactors[7] = {
  0x50, 0x40, 0x30, 0x20, 0x10, 0x00, 0x00,
};

static void Level6AttackHelicopterState0(uint8_t x) {  /* :6104-6130 */
  subCheckWhichJeepToAttack(x);
  subSpawnPlane_HeliPositionRelativeToJeep(x);
  JackalRam.SpriteTypeIndex[x] = 0x9B;
  JackalRam.SpriteData6[x] = 0x08;                     /* 初始向下 */
  JackalRam.Raw[0x6A + x] = 0x80;                      /* V 镜像（下行姿态） */
  if ((int8_t)JackalRam.SpriteVertScreenPosition[x] < 0) {
    JackalRam.Raw[0x6A + x] = 0x00;                    /* 底部 spawn：上行无镜像 */
    JackalRam.SpriteData6[x] = 0x18;
  }
  JackalRam.SpriteData2[x] = 0;
  subCalculateObjectSpeed(x, JackalRam.SpriteData6[x],
                          tblLevel6AttackHelicopterEnterScreenSpeedFactors[0]);
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  subMoveSpriteToNextState(x);
}

static void Level6AttackHelicopterState1(uint8_t x) {  /* :6136-6190 */
  uint8_t dir;
  JackalRam.SpriteData1[x]++;
  if ((JackalRam.SpriteData1[x] & 3u) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);            /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = 0x9B;
  if ((JackalRam.SpriteData1[x] & 2u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;                    /* 旋翼动画 */
  }
  if ((JackalRam.SpriteData1[x] & 0x0Fu) == 0) {       /* 每 $10 帧降速档 */
    JackalRam.SpriteData2[x]++;
    subCalculateObjectSpeed(x, JackalRam.SpriteData6[x],
                            tblLevel6AttackHelicopterEnterScreenSpeedFactors[JackalRam.SpriteData2[x]]);
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((JackalRam.SpriteData1[x] & 0x1Fu) == 0) {       /* 每 $20 帧射击 */
    JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
    SpawnEnemyRoundBullet_Shell(x, 0x0A);
  }
  if (JackalRam.SpriteData2[x] != 6u) {
    return;
  }
  /* :6166-6188 斜向离场：默认 $04 右下；右半屏 → $0C 左下 + H 镜像 */
  dir = 0x04;
  if ((int8_t)JackalRam.SpriteHorizScreenPosition[x] < 0) {
    JackalRam.Raw[0x6A + x] |= 0x40u;
    dir = 0x0C;
  }
  if ((JackalRam.SpriteData6[x] & 0x10u) != 0) {
    /* :6182 上行者 fctInvert24Bit（$00-$02 三字节取反，$01/$02=0 → 低字节 &$1F 即方向） */
    dir = (uint8_t)((0u - dir) & 0x1Fu);
  }
  subCalculateObjectSpeed(x, (uint8_t)(dir & 0x1Fu), 0x31);
  subMoveSpriteToNextState(x);
}

static void Level6AttackHelicopterState2(uint8_t x) {  /* :6192-6212 */
  JackalRam.SpriteData1[x]++;
  if ((JackalRam.SpriteData1[x] & 3u) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);            /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = 0x9D;
  if ((JackalRam.SpriteData1[x] & 2u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((JackalRam.SpriteData1[x] & 0x1Fu) == 0) {
    JackalRam.SpriteWhatDirectionToShoot[x] = subCalculateDirectionTowardJeep(x);
    SpawnEnemyRoundBullet_Shell(x, 0x0A);
  }
}

void Level6AttackHelicopterSpriteLogic(uint8_t x) {  /* :6093-6102（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    Level6AttackHelicopterState0, Level6AttackHelicopterState1, Level6AttackHelicopterState2,
    subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- L6 导弹井（:6227-6302，ID $2E） */

static void Level6MissileLauncherState0(uint8_t x) {  /* :6239-6250 */
  JackalRam.SpriteTypeIndex[x] = 0x9F;
  JackalRam.SpriteData8[x] = 0x10;                     /* 发射延时 */
  JackalRam.SpriteData2[x] = 0x80;                     /* idle 计时 */
  JackalRam.SpriteData3[x] = 0x80;
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x18;      /* :6247 注：恒朝上（spawn 显式给） */
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level6MissileLauncherState1(uint8_t x) {  /* :6252-6262 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData8[x];
  JackalRam.SpriteHitboxShapeIndex[x] = 0x47;          /* 开门可伤 */
  JackalRam.SpriteTypeIndex[x]++;
  subMoveSpriteToNextState(x);
}

static void Level6MissileLauncherState2(uint8_t x) {  /* :6264-6289 */
  uint8_t slot;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {         /* :6270 屏外不发射 */
    JackalSpawnZp[0x08] = 0x39;                        /* BW 导弹 */
    JackalSpawnZp[0x0A] = 0x70;                        /* missile sprite type */
    JackalSpawnZp[0x0B] = 0x02;                        /* 弹种 2 */
    JackalSpawnZp[0x0C] = 0x18;                        /* 恒朝上 */
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x35] = x;
    slot = subSpawnObjectFromParent();
    if (slot == 0xFFu) {                               /* :6287 失败下帧重试 */
      JackalRam.SpriteData2[x]++;
      return;
    }
  }
  JackalRam.SpriteData2[x] = 0x08;
  subMoveSpriteToNextState(x);
}

static void Level6MissileLauncherState3(uint8_t x) {  /* :6291-6302 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[x]--;                      /* 关门 */
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;          /* 关门无敌 */
  subSetSpriteState(x, 1);
}

void Level6MissileLauncherSpriteLogic(uint8_t x) {  /* :6227-6237（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    Level6MissileLauncherState0, Level6MissileLauncherState1, Level6MissileLauncherState2,
    Level6MissileLauncherState3, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- L6 激光炮塔（:6317-6454，ID $47）
   $D5==0（双塔全灭）时各态经 subCheckForBothTurretsDead 消亡 */

static const uint8_t tblLevel6FinalBossLaserTurretDirectionFiringPattern[8] = {
  0x0C, 0x04, 0x0C, 0x08, 0x0C, 0x04, 0x04, 0x08,
};
static const uint8_t tblLevel6FinalBossLasterTurretBlastFlightTime[8] = {
  0x38, 0x54, 0x54, 0x44, 0x54, 0x38, 0x54, 0x44,
};

static void subCheckForBothTurretsDead(uint8_t x) {  /* :6451-6454 */
  if (JackalRam.Level6BossTurretStatus != 0) {
    return;
  }
  subDespawnSprite(x);
}

static void Level6BossLaserTurretState0(uint8_t x) {  /* :6332-6342 */
  JackalRam.SpriteTypeIndex[x] = 0xCA;
  JackalRam.SpriteData5[x] = 0x08;                     /* 初始朝下 */
  subUpdateSpriteForDirectionChange(x, 0x08);
  JackalRam.SpriteData3[x] = 0x07;
  JackalRam.SpriteData2[x] = 0x07;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level6BossLaserTurretState1(uint8_t x) {  /* :6344-6367 */
  uint8_t y;
  JackalRam.SpriteTypeIndex[x] = 0xCA;
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteData5[x]);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  /* :6351-6361 混洗：((RNG>>4)+RNG+Jeep1H+Jeep2H)&7 */
  y = (uint8_t)(((uint8_t)(JackalRam.RNG_INCEveryFrame >> 4) +
                 JackalRam.RNG_INCEveryFrame +
                 JackalRam.SpriteHorizScreenPosition[0x10] +
                 JackalRam.SpriteHorizScreenPosition[0x11]) & 7u);
  JackalRam.SpriteWhatDirectionToShoot[x] = tblLevel6FinalBossLaserTurretDirectionFiringPattern[y];
  JackalRam.SpriteData4[x] = tblLevel6FinalBossLasterTurretBlastFlightTime[y];
  subCheckForBothTurretsDead(x);
  subMoveSpriteToNextState(x);
}

static void Level6BossLaserTurretState2(uint8_t x) {  /* :6374-6409 */
  uint8_t diff;
  JackalRam.SpriteTypeIndex[x] = 0xCA;
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteData5[x]);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCheckForBothTurretsDead(x);
  diff = (uint8_t)(JackalRam.SpriteWhatDirectionToShoot[x] - JackalRam.SpriteData5[x]);
  if (diff == 0) {                                     /* :6384 +++ 到位 */
    JackalRam.SpriteData1[x] = 0x07;
    JackalRam.SpriteTypeIndex[x]++;
    JackalRam.SpriteTypeIndex[x]++;
    subMoveSpriteToNextState(x);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((diff & 0x10u) != 0) {
    JackalRam.SpriteData5[x] = (uint8_t)((JackalRam.SpriteData5[x] - 1u) & 0x1Fu);
  } else {
    JackalRam.SpriteData5[x] = (uint8_t)((JackalRam.SpriteData5[x] + 1u) & 0x1Fu);
  }
}

static void Level6BossLaserTurretState3(uint8_t x) {  /* :6411-6428 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCheckForBothTurretsDead(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  JackalSpawnZp[0x08] = 0x48;                          /* 激光弹 */
  JackalSpawnZp[0x09] = JackalRam.SpriteData4[x];      /* 飞行时间 */
  JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
  JackalSpawnZp[0x0F] = 0;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
  subSetSpriteState(x, 1);
}

static void Level6BossLaserTurretState5(uint8_t x) {  /* :6430-6446 死亡动画 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCheckForBothTurretsDead(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[x] = 0xD2;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  subMoveSpriteToNextState(x);
}

static void Level6BossLaserTurretState6(uint8_t x) {  /* :6448-6450 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCheckForBothTurretsDead(x);
}

void Level6BossLaserTurretSpriteLogic(uint8_t x) {  /* :6317-6330（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    Level6BossLaserTurretState0, Level6BossLaserTurretState1, Level6BossLaserTurretState2,
    Level6BossLaserTurretState3, subSpriteDeath, Level6BossLaserTurretState5,
    Level6BossLaserTurretState6, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 激光弹（:6468-6521，ID $48） */

static void Level6BossLaserTurretBlastState0(uint8_t x) {  /* :6479-6506 */
  JackalRam.SpriteData1[x] = 0x1A;
  JackalRam.SpriteData5[x] = 0xCF;
  if ((JackalRam.SpriteWhatDirectionToShoot[x] & 4u) == 0) {
    JackalRam.SpriteData5[x]++;                        /* :6493 ++ */
  } else if ((JackalRam.SpriteWhatDirectionToShoot[x] & 8u) != 0) {
    JackalRam.Raw[0x6A + x] = 0x40;                    /* H 镜像 */
  }
  JackalRam.SpriteData8[x] = 0x08;
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x51);
  subUpdateSpritePositionForScrolling(x);
  subCheckForBothTurretsDead(x);
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    subInitiateSoundClip(LASER_BLAST_CLIP);            /* stub */
  }
  subMoveSpriteToNextState(x);
}

static void Level6BossLaserTurretBlastState1(uint8_t x) {  /* :6508-6521 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subCheckForBothTurretsDead(x);
  JackalRam.SpriteData4[x]--;
  if (JackalRam.SpriteData4[x] == 0) {
    subMoveSpriteToNextState(x);                       /* → 洞坑 BG（dw[2]） */
    return;
  }
  /* :6514-6515 LSR BCC + 双支汇合（死代码 quirk，照译：Data8 每帧 DEC） */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] == 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData5[x];
  }
}

void Level6BossLaserTurretBlastSpriteLogic(uint8_t x) {  /* :6468-6477（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    Level6BossLaserTurretBlastState0, Level6BossLaserTurretBlastState1,
    subPlayMissileHittingGroundSound_LoadHoleInGroundBGGraphics,
    subProcessExplosionAnimation, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- BossLoad（:6535-6601，ID $45） */

static const uint8_t tblLevel6BossTurretHorizontalSpawnLocationLB[2] = { 0xD2, 0x2E };
static const uint8_t tblLevel6BossTurretHorizontalSpawnLocationUB[2] = { 0x00, 0x01 };

static void Level6BossLoadState0(uint8_t x) {  /* :6545-6553 */
  JackalRam.SpriteData1[x] = 0x02;
  JackalRam.ScreenScrollingForF0ToBoss = 0;            /* Boss Reached */
  JackalRam.ScreenVerticalScrollLockForBossFight = 1;
  subMoveSpriteToNextState(x);
}

static void Level6BossLoadState1(uint8_t x) {  /* :6555-6571 */
  uint8_t slot;
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = 0x00;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0x01;
  JackalRam.SpriteVertScreenPosition[x] = 0x2E;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0x00;
  JackalSpawnZp[0x08] = 0x4A;                          /* FinalBoss 本体 */
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot == 0xFFu) {
    return;                                            /* :6570 失败下帧重试 */
  }
  subMoveSpriteToNextState(x);                         /* :6553 -（BPL 回接 State0 尾） */
}

static void Level6BossLoadState2(uint8_t x) {  /* :6573-6594 */
  uint8_t y, slot;
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] < 0) {
    subMoveSpriteToNextState(x);                       /* → dw[3] despawn */
    return;
  }
  y = JackalRam.SpriteData1[x];
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = tblLevel6BossTurretHorizontalSpawnLocationLB[y];
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = tblLevel6BossTurretHorizontalSpawnLocationUB[y];
  JackalRam.SpriteVertScreenPosition[x] = 0x29;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0x00;
  JackalSpawnZp[0x08] = 0x47;                          /* 激光炮塔 */
  JackalSpawnZp[0x0F] = 0;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot == 0xFFu) {
    JackalRam.SpriteData1[x]++;                        /* :6592 失败重试 */
  }
}

void Level6BossLoadSpriteLogic(uint8_t x) {  /* :6535-6543（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    Level6BossLoadState0, Level6BossLoadState1, Level6BossLoadState2, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- FinalBoss（:6607-6747，ID $4A）
   建筑阶段全程存在；State3 建筑摧毁 → State4 爆炸过渡 → State5 spawn 坦克 → State6 消亡 */

static void Level6FinalBossState0(uint8_t x) {  /* :6620-6639 */
  JackalRam.SpriteData8[x] = 0;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  JackalRam.SpriteData2[x] = (uint8_t)(JackalRam.SpriteHealthHP[x] & 0x3Fu);
  subUpdateSpritePositionForScrolling(x);
  JackalRam.PPUGraphicsUpdateComplete = 0;
  JackalRam.PPUGraphicsUpdateTableIndex = 0x0E;
  JackalRam.Level6BossTurretStatus = 1;                /* :6634 炮塔存活、普通 palette */
  JackalRam.SpriteData3[x] = JackalRam.SpriteData4[x] =
      (uint8_t)(JackalRam.RNG_INCEveryFrame | 0xC0u);
  subMoveSpriteToNextState(x);
}

static void Label672(uint8_t x) {  /* :6722-6747 RNG 步兵生成（EnemyPoints=0） */
  uint8_t a, slot;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x02] = (uint8_t)(0xEFu - JackalRam.SpriteVertScreenPosition[x]);
  a = (uint8_t)(JackalRam.RNG_INCEveryFrame + JackalRam.SpriteData4[x] +
                JackalRam.SpriteVertScreenPosition[0x10] +
                JackalRam.SpriteVertScreenPosition[0x11]);
  JackalSpawnZp[0x00] = (uint8_t)(a >> 1);
  JackalSpawnZp[0x01] = (a & 1u) ? 0x00u : 0xFFu;      /* :6735-6739 ROL 后 SBC #1 符号扩展 */
  JackalSpawnZp[0x08] = 0x07;
  JackalSpawnZp[0x0F] = 0x01;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot != 0xFFu) {
    JackalRam.EnemyPoints[slot] = 0;                   /* :6746（Y=$FF 时 ASM 写 $01A9，C 侧护栏） */
  }
}

static void Level6FinalBossState1(uint8_t x) {  /* :6641-6666 */
  JackalRam.SpriteData3[x]--;
  if (JackalRam.SpriteData3[x] == 0) {
    JackalRam.SpriteData3[x] = JackalRam.SpriteData4[x];
    Label672(x);
  }
  JackalRam.SpriteTypeIndex[x] = 0;
  if (JackalRam.SpriteData2[x] != 0) {
    JackalRam.SpriteData2[x]--;
  } else {
    JackalRam.SpriteTypeIndex[x] = 0xD1;
    JackalRam.SpriteData8[x]++;
    if ((JackalRam.SpriteData8[x] & 7u) == 0) {
      JackalRam.SpriteData2[x] = (uint8_t)(JackalRam.SpriteHealthHP[x] & 0x3Fu);
    }
    JackalRam.SpriteGraphicsAttributes[x] =
        (uint8_t)((JackalRam.SpriteGraphicsAttributes[x] + 1u) & 3u);
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
}

static void Level6FinalBossState3(uint8_t x) {  /* :6668-6684 建筑 HQ 摧毁 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[x] = 0;
  JackalRam.Level6BossTurretStatus = 0;                /* :6681 建筑毁=双塔亡 */
  subStopMusic();                                      /* stub */
  subDespawnAllObjectsExceptFinalBoss();
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossState4(uint8_t x) {  /* :6686-6693 建筑爆炸过渡 */
  if (subFinalBossBuildingTransitionToFinalBossTank() != 0) {
    return;
  }
  subEraseLevel6BossFlags();
  JackalRam.ScreenVerticalScrollLockForBossFight = 1;  /* :6692 $3A=1 坦克战锁滚 */
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossState5(uint8_t x) {  /* :6695-6714 */
  uint8_t slot;
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = 0x00;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = 0x01;
  JackalRam.SpriteVertScreenPosition[x] = 0x30;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0x00;
  JackalSpawnZp[0x08] = 0x4B;                          /* 终 boss 坦克 */
  JackalSpawnZp[0x0F] = 0;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot == 0xFFu) {
    return;                                            /* :6710 失败下帧重试 */
  }
  JackalRam.SpriteData1[x] = 0x60;
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossState6(uint8_t x) {  /* :6716-6720 */
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subDespawnSprite(x);
  }
}

void Level6FinalBossSpriteLogic(uint8_t x) {  /* :6607-6618（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    Level6FinalBossState0, Level6FinalBossState1, subSpriteDeath, Level6FinalBossState3,
    Level6FinalBossState4, Level6FinalBossState5, Level6FinalBossState6,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- FinalBossTank（:3411-3612，ID $4B） */

static const uint8_t tblLevel6BossTankTreadPPUGraphicsUpdateIndex[4] = { 0x0A, 0x0B, 0x0C, 0x0D };
static const uint8_t tblLevel6FinalBossTankPaletteUpdateIndex[3] = { 0x30, 0x31, 0x00 };
static const uint8_t tblLevel6BossTankHealthTriggersForChangingPalettes[3] = { 0xAA, 0x95, 0x00 };

static uint8_t Label462(uint8_t x) {  /* :3572-3588 边沿判定（1=SEC 到边/0=CLC 可行） */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.Level6BossTankScroll_Next = JackalRam.SpriteHorizScreenPosition[x];
  if (JackalRam.SpriteHorizScreenPosition[x] < 0x30u) {
    if (JackalRam.SpriteHorizSpeedUB[x] == 0) {
      return 1;                                        /* :3581 BEQ ++ */
    }
    return (int8_t)JackalRam.SpriteHorizSpeedUB[x] >= 0 ? 0 : 1;  /* BPL - / BMI ++ */
  }
  if (JackalRam.SpriteHorizScreenPosition[x] >= 0xD0u) {
    return (int8_t)JackalRam.SpriteHorizSpeedUB[x] < 0 ? 0 : 1;   /* :3585 BMI - */
  }
  return 0;
}

static void Level6BossTank_CheckForPaletteUpdateOnLowHealth(uint8_t x) {  /* :3590-3604 */
  uint8_t y;
  if (JackalRam.SpriteHealthHP[x] >= JackalRam.SpriteData5[x]) {
    return;
  }
  y = JackalRam.SpriteData6[x];
  JackalRam.SpriteData6[x]++;
  Label152(tblLevel6FinalBossTankPaletteUpdateIndex[y]);
  JackalRam.SpriteData5[x] =
      tblLevel6BossTankHealthTriggersForChangingPalettes[JackalRam.SpriteData6[x]];
  JackalRam.Level6BossTurretStatus = 0x81;             /* :3602 炮塔换 palette（MSB）+ 存活 */
}

static void Level6FinalBossTankState0(uint8_t x) {  /* :3423-3445 */
  JackalRam.LevelBossEntitiesRemaining = 1;
  JackalRam.SpriteTypeIndex[x] = 0xD3;
  JackalRam.Level6BossTurretStatus = 1;
  JackalRam.SpriteData2[x] = 0;
  JackalSpawnZp[0x08] = 0x4F;                          /* 坦克炮塔 */
  JackalSpawnZp[0x0F] = 0;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  (void)subSpawnObjectFromParent();
  (void)Label462(x);
  JackalRam.Level6BossTankScroll_Current = JackalRam.Level6BossTankScroll_Next;
  JackalRam.SpriteData6[x] = 0;
  JackalRam.SpriteData5[x] = tblLevel6BossTankHealthTriggersForChangingPalettes[0];
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossTankState1(uint8_t x) {  /* :3447-3473 */
  uint8_t a;
  Level6BossTank_CheckForPaletteUpdateOnLowHealth(x);
  /* :3450-3457 行动随机化：和吉普位置混洗、&$7F、<$20 钳 $20 */
  a = (uint8_t)((JackalRam.RNG_INCEveryFrame +
                 JackalRam.SpriteHorizScreenPosition[0x10] +
                 JackalRam.SpriteHorizScreenPosition[0x11]) & 0x7Fu);
  if (a < 0x20u) {
    a |= 0x20u;
  }
  JackalRam.SpriteData1[x] = a;
  if (Label462(x) != 0) {
    JackalRam.SpriteData1[x] = 0x70;
  }
  JackalRam.SpriteHorizSpeedLB[x] = 0x40;
  JackalRam.SpriteHorizSpeedUB[x] = 0x01;
  JackalRam.SpriteData3[x] = 0xFF;
  if ((int8_t)JackalRam.SpriteHorizScreenPosition[x] < 0) {
    subInvertSpriteHorizSpeed(x);
    JackalRam.SpriteData3[x] = 0x01;
  }
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossTankState2(uint8_t x) {  /* :3475-3518 */
  uint8_t slot;
  Level6BossTank_CheckForPaletteUpdateOnLowHealth(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] != 0 && JackalRam.PPUGraphicsUpdateTableIndex == 0) {
    JackalRam.PPUGraphicsUpdateComplete = 0;           /* :3520-3525 履带 BG 轮换 */
    JackalRam.PPUGraphicsUpdateTableIndex =
        tblLevel6BossTankTreadPPUGraphicsUpdateIndex[JackalRam.SpriteData2[x]];
    JackalRam.SpriteData2[x] =
        (uint8_t)((JackalRam.SpriteData2[x] + JackalRam.SpriteData3[x]) & 3u);
  }
  if (Label462(x) != 0) {
    subClearSpriteSpeed(x);                            /* Label469 */
    subSetSpriteState(x, 1);
    return;
  }
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  /* :3496-3510 喷射火焰控制体（偏移 -4/+$2E） */
  subClearSpriteSpeed(x);
  JackalSpawnZp[0x08] = 0x4C;
  JackalSpawnZp[0x00] = 0xFC;
  JackalSpawnZp[0x01] = 0xFF;
  JackalSpawnZp[0x02] = 0x2E;
  JackalSpawnZp[0x03] = 0x00;
  JackalSpawnZp[0x0F] = 0;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot == 0xFFu) {
    JackalRam.SpriteData1[x]++;                        /* :3512 失败重试（保非零） */
    return;
  }
  JackalRam.SpriteData1[x] = 0;
  JackalRam.SpriteData4[x] = 0;
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossTankState3(uint8_t x) {  /* :3525-3551 焰尖喷射 */
  uint8_t slot;
  Level6BossTank_CheckForPaletteUpdateOnLowHealth(x);
  if (Label462(x) != 0) {
    subClearSpriteSpeed(x);                            /* Label469 */
    subSetSpriteState(x, 1);
    return;
  }
  if ((JackalRam.SpriteData1[x] & 0x0Fu) != 0) {
    JackalRam.SpriteData1[x]++;
    return;
  }
  JackalRam.SpriteData4[x]++;                          /* 焰尖计次（首尾各一） */
  if (JackalRam.SpriteData4[x] == 3u) {
    subClearSpriteSpeed(x);                            /* Label469 */
    subSetSpriteState(x, 1);
    return;
  }
  JackalSpawnZp[0x08] = 0x4D;                          /* 焰尖碰撞体 */
  JackalSpawnZp[0x00] = 0xFC;
  JackalSpawnZp[0x01] = 0xFF;
  JackalSpawnZp[0x02] = 0x2E;
  JackalSpawnZp[0x03] = 0x00;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  if (slot == 0xFFu) {
    return;                                            /* :3548 BMI +（失败不 INC） */
  }
  JackalRam.SpriteData1[x]++;
}

static void Level6FinalBossTankState4(uint8_t x) {  /* :3557-3569 爆炸收尾 */
  subClearSpriteSpeed(x);
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  JackalRam.Level6BossTurretStatus = 0;
  if (Label474() != 0) {
    return;
  }
  JackalRam.LevelBossEntitiesRemaining = 0;
  JackalRam.SpriteTypeIndex[x]++;
  subMoveSpriteToNextState(x);
}

void Level6FinalBossTankSpriteLogic(uint8_t x) {  /* :3411-3421（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    Level6FinalBossTankState0, Level6FinalBossTankState1, Level6FinalBossTankState2,
    Level6FinalBossTankState3, Level6FinalBossTankState4,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 火焰喷射（:3628-3686，ID $4C）
   WhichJeep 字段借作动画计数（ASM $06E0,X） */

static void Level6FinalBossTankFlameShotState0(uint8_t x) {  /* :3636-3650 */
  JackalRam.SpriteWhichJeeptoAttack[x] = 0;
  JackalRam.SpriteData8[x] = 0x10;
  JackalRam.SpriteData1[x] = 0xD5;                     /* 基础型 */
  JackalRam.SpriteData2[x] = 0xE5;                     /* 镜像基础型 */
  JackalRam.SpriteData3[x] = 0;
  subUpdateSpritePositionForScrolling(x);
  subInitiateSoundClip(FLAME_SHOT_CLIP);               /* stub */
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossTankFlameShotState1(uint8_t x) {  /* :3652-3686 */
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData1[x];
  JackalRam.SpriteWhichJeeptoAttack[x]++;
  if ((JackalRam.SpriteWhichJeeptoAttack[x] & 1u) != 0) {
    JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData2[x];   /* 奇帧镜像 */
  }
  if (JackalRam.SpriteWhichJeeptoAttack[x] == JackalRam.SpriteData8[x]) {
    /* :3665-3672 完全喷出：半速下移（8x8 tile 拼 8 像素/2=4，:3668 注） */
    JackalRam.SpriteVertSpeedUB[x] = 0x04;
    JackalRam.SpriteData3[x] = 1;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteHorizScreenPosition[x] < 0x20u ||
      JackalRam.SpriteHorizScreenPosition[x] >= 0xE0u) {
    subDespawnSprite(x);                               /* :3674 滚屏拉离即消亡 */
    return;
  }
  if (JackalRam.SpriteData3[x] == 0) {                 /* 生长期双型递进 */
    JackalRam.SpriteData1[x]++;
    JackalRam.SpriteData2[x]++;
  }
}

void Level6FinalBossTankFlameShotSpriteLogic(uint8_t x) {  /* :3628-3634（dw 2 项） */
  static void (*const tbl[2])(uint8_t) = {
    Level6FinalBossTankFlameShotState0, Level6FinalBossTankFlameShotState1,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 焰尖（:3699-3728，ID $4D） */

static void Level6FinalBossTankFlameShotTipState0(uint8_t x) {  /* :3707-3716 */
  /* :3709-3712 注：hitbox 不随火焰动画移动（原游戏 quirk，玩家不敏感，照译） */
  JackalRam.SpriteVertSpeedUB[x] = 0x08;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void Level6FinalBossTankFlameShotTipState1(uint8_t x) {  /* :3718-3728 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteHorizScreenPosition[x] < 0x20u ||
      JackalRam.SpriteHorizScreenPosition[x] >= 0xE0u) {
    subDespawnSprite(x);
  }
}

void Level6FinalBossTankFlameShotTipSpriteLogic(uint8_t x) {  /* :3699-3705（dw 2 项） */
  static void (*const tbl[2])(uint8_t) = {
    Level6FinalBossTankFlameShotTipState0, Level6FinalBossTankFlameShotTipState1,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 坦克炮塔（:3744-3874，ID $4F） */

static void Label486(uint8_t x) {  /* :3855-3874 跟随坦克横移（进位链两段） */
  uint16_t t = (uint16_t)JackalRam.Level6BossTankScroll_Next + 0x40u;
  uint16_t t2;
  uint8_t ub = (uint8_t)(t >> 8);                      /* :3860-3862 ROL 取进位 */
  t2 = (uint16_t)((uint8_t)t + 0x07u);
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)t2;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(ub + (t2 >> 8));
  if ((int8_t)JackalRam.Level6BossTurretStatus < 0) {
    JackalRam.SpriteGraphicsAttributes[x] = 1;         /* :3870 MSB=换 palette */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
}

static void Level6BossTankTurretState0(uint8_t x) {  /* :3757-3772 */
  JackalRam.SpriteTypeIndex[x] = 0xFC;
  JackalRam.SpriteData5[x] = 0x08;
  subUpdateSpriteForDirectionChange(x, 0x08);
  /* :3763-3764 TYA LDA Data6（死代码 quirk，同白弹 :6786——Data6 保持生成值） */
  JackalRam.SpriteData3[x] = 0x02;
  JackalRam.SpriteData2[x] = 0x02;
  subCheckWhichJeepToAttack(x);
  JackalRam.SpriteVertScreenPosition[x] = 0x20;
  Label486(x);
  subMoveSpriteToNextState(x);
}

static void Level6BossTankTurretState1(uint8_t x) {  /* :3774-3814 */
  uint8_t diff;
  JackalRam.SpriteTypeIndex[x] = 0xFC;
  subUpdateSpriteForDirectionChange(x, JackalRam.SpriteData5[x]);
  JackalRam.SpriteData6[x] = fctGetSpriteOrientationIndex(JackalRam.SpriteData5[x]);  /* :3779-3780 TYA STA */
  Label486(x);
  subCountDownForJeepTargetBy1(x);
  subCheckEnemyTarget_AttackOtherJeepIfDead(x);
  diff = (uint8_t)(subCalculateDirectionTowardJeep(x) - JackalRam.SpriteData5[x]);
  if (diff == 0) {                                     /* :3809 +++ 到位开火 */
    if ((int8_t)JackalRam.SpriteState[x] < 0) {
      return;                                          /* :3810 BMI -（屏外歇火） */
    }
    JackalRam.SpriteData1[x] = 0x40;
    Label490(x);
    subMoveSpriteToNextState(x);
    return;
  }
  JackalRam.SpriteData2[x]--;
  if (JackalRam.SpriteData2[x] != 0) {
    return;
  }
  JackalRam.SpriteData2[x] = JackalRam.SpriteData3[x];
  if ((diff & 0x10u) != 0) {
    JackalRam.SpriteData5[x] = (uint8_t)((JackalRam.SpriteData5[x] - 1u) & 0x1Fu);
  } else {
    JackalRam.SpriteData5[x] = (uint8_t)((JackalRam.SpriteData5[x] + 1u) & 0x1Fu);
  }
}

static void Level6BossTankTurretState2(uint8_t x) {  /* :3816-3829 */
  Label486(x);
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 0) {
    subSetSpriteState(x, 1);
    return;
  }
  if ((JackalRam.SpriteData1[x] & 0x0Fu) != 0) {
    return;
  }
  JackalRam.SpriteData8[x] = 0x03;                     /* recoil 帧数 */
  SpawnTurretProjectile(x);
  Label493(x);
  subMoveSpriteToNextState(x);
}

static void Level6BossTankTurretState3(uint8_t x) {  /* :3831-3839 */
  Label486(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteData8[x] = 0x03;
  subInvertSpriteVertAndHorizSpeed(x);
  subMoveSpriteToNextState(x);
}

static void Level6BossTankTurretState4(uint8_t x) {  /* :3841-3853 */
  JackalRam.SpriteData8[x]--;
  /* :3843 BNE + 双支汇合（死代码 quirk，Label486 恒执行） */
  Label486(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  subClearSpriteSpeed(x);
  if (JackalRam.Level6BossTurretStatus != 0) {         /* :3848 坦克存活 → 回火力态 */
    subSetSpriteState(x, 2);
    return;
  }
  subMoveSpriteToNextState(x);                         /* 坦克亡 → State5 死亡 */
}

void Level6BossTankTurretSpriteLogic(uint8_t x) {  /* :3744-3755（dw 7 项） */
  static void (*const tbl[7])(uint8_t) = {
    Level6BossTankTurretState0, Level6BossTankTurretState1, Level6BossTankTurretState2,
    Level6BossTankTurretState3, Level6BossTankTurretState4, subSpriteDeath, subSpriteExplosion,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 电扶梯（Bank7:8154-8196，ID $44） */

static const uint8_t tblLevel6EscalatorPPUGraphicsUpdateIndex[2] = { 0x08, 0x09 };

static void EscalatorState0(uint8_t x) {  /* :8163-8176 */
  if (JackalRam.CurrentLevelScreen >= 0x0Au) {         /* :8165 注：screen 10 起加大 hitbox */
    JackalRam.SpriteHitboxShapeIndex[x] = 0x7F;
  }
  subUpdateSpritePositionForScrolling(x);
  if (JackalRam.PPUGraphicsUpdateTableIndex == 0) {
    JackalRam.PPUGraphicsUpdateComplete = 0;
    JackalRam.PPUGraphicsUpdateTableIndex = 0x08;
  }
  subMoveSpriteToNextState(x);
}

static void EscalatorState1(uint8_t x) {  /* :8178-8193 */
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 7u) == 0 && JackalRam.PPUGraphicsUpdateTableIndex == 0) {
    JackalRam.PPUGraphicsUpdateTableIndex =
        tblLevel6EscalatorPPUGraphicsUpdateIndex[JackalRam.SpriteData1[x] & 1u];
    JackalRam.PPUGraphicsUpdateComplete = 0;
    JackalRam.SpriteData1[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
}

void EscalatorSpriteLogic(uint8_t x) {  /* :8154-8161（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    EscalatorState0, EscalatorState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- 激光炮塔图形装载（:3382-3395，ID $49） */

static void Level6BossLaserTurretGraphicsLoadState0(uint8_t x) {  /* :3390-3395 */
  JackalRam.PPUGraphicsUpdateComplete = 0;
  JackalRam.PPUGraphicsUpdateTableIndex = 0x10;
  subMoveSpriteToNextState(x);
}

void Level6BossLaserTurretGraphicsLoadSpriteLogic(uint8_t x) {  /* :3382-3388（dw 2 项） */
  static void (*const tbl[2])(uint8_t) = {
    Level6BossLaserTurretGraphicsLoadState0, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Bank7 公共段（:8347-8532 + :1530）
   建筑爆炸随机位置表：{UB, LB, Vert} 三元组，$FF 收尾（:8485-8502） */

static const uint8_t tblLevel6FinalBossBuilding_Random_ExplosionSpawnLocations[40] = {
  0x00, 0xC0, 0x20, 0x01, 0x00, 0x30, 0x00, 0xE0, 0x40, 0x01, 0x40, 0x30,
  0x00, 0xF0, 0x20, 0x01, 0x50, 0x30, 0x00, 0xB0, 0x40, 0x01, 0x20, 0x40,
  0x00, 0xF0, 0x20, 0x01, 0x00, 0x30, 0x00, 0xC0, 0x20, 0x01, 0x20, 0x40,
  0x00, 0xD0, 0x40, 0xFF,
};

void subDespawnAllObjectsExceptFinalBoss(void) {  /* :8520-8532 */
  uint8_t y;
  for (y = 0x10u; y-- > 0;) {                          /* Y=$0F..0 */
    if (JackalRam.SpriteObjectID[y] != 0x4Au && JackalRam.SpriteObjectID[y] != 0x4Bu) {
      JackalRam.SpriteObjectID[y] = 0;
      JackalRam.SpriteTypeIndex[y] = 0;
    }
  }
}

void subEraseLevel6BossFlags(void) {  /* :1530-1537 清 $0113-$0119（$0112 保留，BNE 不到 0） */
  uint8_t y;
  for (y = 7u; y >= 1u; y--) {
    JackalRam.Raw[0x112 + y] = 0;
  }
}

static uint8_t Label1364(void) {  /* :8475-8518 计时到 0 → 清场 + palette 轮换（返回 1=未完/0=完） */
  uint8_t t = (uint8_t)(JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilTankSpawns - 1u);
  if (t != 0) {
    JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilTankSpawns = t;
    return 1;
  }
  subDespawnAllObjectsExceptFinalBoss();
  if (JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex >=
      JackalRam.Level6FinalBossEndingGraphics_PaletteUpdateIndex) {
    return 0;                                          /* :8515 */
  }
  Label152(JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex);
  JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex++;
  if (JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex >=
      JackalRam.Level6FinalBossEndingGraphics_PaletteUpdateIndex) {
    return 0;
  }
  return 1;
}

static void spawnBuildingExplosions(void) {  /* :8402-8474 RNG&$0F==0 → 3 连爆 */
  uint8_t x;
  if ((JackalRam.RNG_INCEveryFrame & 0x0Fu) != 0) {
    return;
  }
  JackalSpawnZp[0x00] = 3;                             /* 每帧至多 3 个 */
  for (x = 0x10u;;) {
    uint8_t y;
    JackalSpawnZp[0x01] = (uint8_t)(JackalSpawnZp[0x01] + 8u);
    if (JackalSpawnZp[0x01] >= 0x48u) {
      JackalSpawnZp[0x01] = 0;
    }
    x--;
    if ((int8_t)x < 0) {
      return;                                          /* :8415 槽扫尽 → 调用方落 Label1364 */
    }
    if (JackalRam.SpriteObjectID[x] != 0) {
      continue;
    }
    y = JackalRam.Level6FinalBossExplosionTableIndex;
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = tblLevel6FinalBossBuilding_Random_ExplosionSpawnLocations[y];
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = tblLevel6FinalBossBuilding_Random_ExplosionSpawnLocations[(uint8_t)(y + 1u)];
    if (JackalRam.Level6BossTankScroll_Next != 0) {    /* :8425 坦克横移跟随 */
      uint16_t lb = (uint16_t)JackalRam.Level6BossTankScroll_Next + 0x40u;
      JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)lb;
      if (lb > 0xFFu) {
        JackalRam.SpriteAbsoluteHorizPositionUB[x] = 1;
      }
      if ((JackalRam.RNG_INCEveryFrame & 0x10u) != 0) {
        uint16_t a = (uint16_t)JackalRam.SpriteAbsoluteHorizPositionLB[x] + JackalSpawnZp[0x01];
        JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)a;
        JackalRam.SpriteAbsoluteHorizPositionUB[x] =
            (uint8_t)(0u + JackalRam.SpriteAbsoluteHorizPositionUB[x] + (a >> 8));
      } else {
        uint8_t oldLB = JackalRam.SpriteAbsoluteHorizPositionLB[x];
        JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)(oldLB - JackalSpawnZp[0x01]);
        JackalRam.SpriteAbsoluteHorizPositionUB[x] =
            (uint8_t)(JackalRam.SpriteAbsoluteHorizPositionUB[x] -
                      (oldLB < JackalSpawnZp[0x01] ? 1u : 0u));
      }
    }
    y = (uint8_t)(y + 2u);
    JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
    JackalRam.SpriteVertScreenPosition[x] = tblLevel6FinalBossBuilding_Random_ExplosionSpawnLocations[y];
    y++;
    JackalRam.Level6FinalBossExplosionTableIndex = y;
    if ((int8_t)tblLevel6FinalBossBuilding_Random_ExplosionSpawnLocations[y] < 0) {
      JackalRam.Level6FinalBossExplosionTableIndex = 0;   /* :8459 表尾回绕 */
    }
    /* :8462-8464 注：$85 灰炮塔借尸爆炸——任意可爆对象皆可，状态直接置死 */
    JackalRam.SpriteObjectID[x] = 0x85;
    JackalRam.SpriteState[x] =
        (uint8_t)(BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU)[0x05] & 0x0Fu);
    JackalSpawnZp[0x00]--;
    if (JackalSpawnZp[0x00] == 0) {
      return;
    }
  }
}

uint8_t Label474(void) {  /* :8347-8366 坦克爆炸首帧布置；复帧落 :8402 爆炸+Label1364 */
  JackalRam.Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding = 1;
  if (JackalRam.Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated != 0) {
    spawnBuildingExplosions();                         /* :8352 BNE ++ → :8388 亦 !=0 → :8402 */
    return Label1364();
  }
  JackalRam.PPUGraphicsUpdateComplete = 0;
  JackalRam.PPUGraphicsUpdateTableIndex = 0x0F;        /* :8355 坦克击破图+mission 文本 */
  subInitiateSoundClip(FINAL_BOSS_BLOWUP_CLIP);        /* stub */
  JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilTankSpawns = 0xC0;
  JackalRam.Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated++;
  JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex = 0x55;
  JackalRam.Level6FinalBossEndingGraphics_PaletteUpdateIndex = 0x65;
  return 1;                                            /* :8366 RTS 时 A=$65（非零） */
}

uint8_t subFinalBossBuildingTransitionToFinalBossTank(void) {  /* :8370-8474 */
  if (JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilFinalBossMusicStarts != 0xC0u) {
    JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilFinalBossMusicStarts++;
    if (JackalRam.Level6FinalBossBuildingBlowingUpTimeUntilFinalBossMusicStarts == 0xC0u) {
      subInitiateSoundClip(FINAL_BOSS_MUSIC);          /* stub（:8376 注：二次 CMP 冗余） */
    }
  }
  JackalRam.Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding = 1;
  if (JackalRam.PPUGraphicsUpdateTableIndex == 0) {
    JackalRam.PPUGraphicsUpdateComplete = 0;
    JackalRam.PPUGraphicsUpdateTableIndex = 0x11;
  }
  if (JackalRam.Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated == 0) {
    JackalRam.ScreenVerticalScrollLockForBossFight = 0;   /* :8390 解锁（A=0） */
    subInitiateSoundClip(FINAL_BOSS_BLOWUP_CLIP);      /* stub */
    Label152(0x2F);
    JackalRam.Level6FinalBossCurrentGraphics_PaletteUpdateIndex = 0x3D;
    JackalRam.Level6FinalBossEndingGraphics_PaletteUpdateIndex = 0x55;
    JackalRam.Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated++;
    return 1;                                          /* :8401 RTS 时 A=$55（非零） */
  }
  spawnBuildingExplosions();
  return Label1364();
}
