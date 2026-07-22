/* core/enemy_ai.c：敌人 AI 底座逐行翻译。
   数据表：tblCosineValues（:7474）/table1（:7362）/tblSpriteMirroringAndIndex（:7545）/
   tblWalkingAnimationAttributes（:7584）硬编码 C 表；tblCollisionCheckBGOffsetNear/Far
   （Bank6 $A0CB/$A0DB、$A0AB/$A0BB，ROM 签名定位）经 BankPtr(6)。
   24 位坐标模型：{subpixel, LB, UB} 小端三元组（$10-$12 横、$13-$15 纵），C 侧以
   uint32_t 合成（subpixel 低 8 位），进位/借位语义与 6502 ADC/SBC 链一致。 */
#include "ram.h"
#include "bank.h"
#include "enemy_ai.h"
#include "enemy_infantry.h"
#include "enemy_tank.h"
#include "enemy_level1.h"
#include "enemy_building.h"
#include "enemy_level2.h"
#include "enemy_level3.h"
#include "enemy_level6.h"
#include "spawn.h"
#include "jeep.h"
#include "sound_stub.h"

#define TBL_COLLISION_OFFSET_FAR_UB  0xA0ABu  /* Bank6，16 项 */
#define TBL_COLLISION_OFFSET_FAR_LB  0xA0BBu
#define TBL_COLLISION_OFFSET_NEAR_UB 0xA0CBu
#define TBL_COLLISION_OFFSET_NEAR_LB 0xA0DBu

#define INFANTRY_SQUISH_CLIP   0x4Bu  /* InfantrySquishSoundClip（Sound.ASM，stub 仅调用点） */
#define MAINWEAPON_EXPLOSION_CLIP 0x17u

/* tblCosineValuesForDirectionalTargetting（:7474-7475） */
static const uint8_t tblCosineValues[16] = {
  0x80, 0x7D, 0x76, 0x6A, 0x5A, 0x47, 0x30, 0x18,
  0x00, 0x18, 0x30, 0x47, 0x5A, 0x6A, 0x76, 0x7D,
};
/* table1（:7362-7365）：方向量化角度表（bit0 选半字节） */
static const uint8_t table1[32] = {
  0x40, 0x00, 0x00, 0x00, 0x84, 0x22, 0x11, 0x11,
  0x86, 0x43, 0x22, 0x21, 0x86, 0x54, 0x33, 0x22,
  0x87, 0x65, 0x43, 0x33, 0x87, 0x65, 0x54, 0x43,
  0x87, 0x66, 0x54, 0x44, 0x87, 0x76, 0x55, 0x44,
};
/* tblSpriteMirroringAndIndexForDirection（:7545-7561）：attr 高半 + TypeIndex 增量低半 */
static const uint8_t tblSpriteMirroringAndIndexForDirection[8] = {
  0x00, 0x01, 0x02, 0x41, 0x40, 0xC1, 0x82, 0x81,
};
/* tblWalkingAnimationAttributes_SpriteIndex（:7584-7600） */
static const uint8_t tblWalkingAnimationAttributes[8] = {
  0x00, 0x0F, 0x0F, 0x00, 0x40, 0x4F, 0x00, 0x01,
};

/* ---------------------------------------------------------------- 取反（:7367-7410） */

uint8_t fctInvertA(uint8_t a) {  /* :7382：$100-A（A=0 → 0） */
  return (uint8_t)(0u - a);
}

/* fctInvert24Bit（:7367-7379）：$00-$02 三字节取反（C 侧 uint32 低 24 位） */
static uint32_t invert24(uint32_t v) {
  return (0x1000000u - v) & 0xFFFFFFu;
}

void subInvertSpriteVertSpeed(uint8_t x) {  /* :7402-7410 */
  uint16_t v = (uint16_t)(JackalRam.SpriteVertSpeedUB[x] << 8 | JackalRam.SpriteVertSpeedLB[x]);
  v = (uint16_t)(0u - v);
  JackalRam.SpriteVertSpeedLB[x] = (uint8_t)v;
  JackalRam.SpriteVertSpeedUB[x] = (uint8_t)(v >> 8);
}

void subInvertSpriteHorizSpeed(uint8_t x) {  /* :7392-7400（16 位取反等价） */
  uint16_t h = (uint16_t)(JackalRam.SpriteHorizSpeedUB[x] << 8 | JackalRam.SpriteHorizSpeedLB[x]);
  h = (uint16_t)(0u - h);
  JackalRam.SpriteHorizSpeedLB[x] = (uint8_t)h;
  JackalRam.SpriteHorizSpeedUB[x] = (uint8_t)(h >> 8);
}

void subInvertSpriteVertAndHorizSpeed(uint8_t x) {  /* :7390-7400（vert 后落 horiz） */
  uint16_t h;
  subInvertSpriteVertSpeed(x);
  h = (uint16_t)(JackalRam.SpriteHorizSpeedUB[x] << 8 | JackalRam.SpriteHorizSpeedLB[x]);
  h = (uint16_t)(0u - h);
  JackalRam.SpriteHorizSpeedLB[x] = (uint8_t)h;
  JackalRam.SpriteHorizSpeedUB[x] = (uint8_t)(h >> 8);
}

/* ---------------------------------------------------------------- 位置积分/滚动同步/Label244 */

void subProcessObjectSpeed_UpdatePosition(uint8_t x) {  /* :7020-7066 */
  uint32_t acc;
  if (JackalRam.SpriteObjectID[x] != 0xBEu && JackalRam.SpriteObjectID[x] != 0xBDu &&
      JackalRam.ScreenScrollingForF0ToBoss != 0) {
    return;                                  /* ++：F0 滚动中冻结（heli 豁免） */
  }
  /* 横向：$00=UB 符号扩展；SubPixel+LB → AbsHorizLB+UB（ADC 链）→ AbsHorizUB+$00 */
  acc = ((uint32_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 16 |
         (uint32_t)JackalRam.SpriteAbsoluteHorizPositionLB[x] << 8 |
         JackalRam.SpriteHorizScreenPositionSubPixel[x]);
  acc = (acc + (uint32_t)(int16_t)(int8_t)JackalRam.SpriteHorizSpeedUB[x] * 0x100u +
         JackalRam.SpriteHorizSpeedLB[x]) & 0xFFFFFFu;
  JackalRam.SpriteHorizScreenPositionSubPixel[x] = (uint8_t)acc;
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)(acc >> 8);
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(acc >> 16);
  /* 纵向：VertScreenPos 为 LB（:7059-7061） */
  acc = ((uint32_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 16 |
         (uint32_t)JackalRam.SpriteVertScreenPosition[x] << 8 |
         JackalRam.SpriteVertScreenPositionSubPixel[x]);
  acc = (acc + (uint32_t)(int16_t)(int8_t)JackalRam.SpriteVertSpeedUB[x] * 0x100u +
         JackalRam.SpriteVertSpeedLB[x]) & 0xFFFFFFu;
  JackalRam.SpriteVertScreenPositionSubPixel[x] = (uint8_t)acc;
  JackalRam.SpriteVertScreenPosition[x] = (uint8_t)(acc >> 8);
  JackalRam.SpriteAbsoluteVertPositionUB[x] = (uint8_t)(acc >> 16);
}

void subUpdateSpritePositionForScrolling(uint8_t x) {  /* :1639-1687 */
  uint8_t step = 1;                          /* $00 */
  uint8_t diff;
  if (JackalRam.ScreenTransitionTimer != 0) {
    step = 8;
  }
  diff = (uint8_t)(JackalRam.CurrentLevelScreenSubPosition -
                   JackalRam.PreviousLevelScreenSubposition);
  if (diff != 0) {
    if (JackalRam.ScreenScrolledUp_Down == 0) {
      uint16_t acc = (uint16_t)JackalRam.SpriteVertScreenPosition[x] + step;
      JackalRam.SpriteVertScreenPosition[x] = (uint8_t)acc;
      JackalRam.SpriteAbsoluteVertPositionUB[x] =
          (uint8_t)(JackalRam.SpriteAbsoluteVertPositionUB[x] + (acc >> 8));
    } else {
      uint16_t acc = (uint16_t)JackalRam.SpriteVertScreenPosition[x] - 1u;
      JackalRam.SpriteVertScreenPosition[x] = (uint8_t)acc;
      JackalRam.SpriteAbsoluteVertPositionUB[x] =
          (uint8_t)(JackalRam.SpriteAbsoluteVertPositionUB[x] -
                    (acc > 0xFFu ? 1u : 0u));
    }
  }
  /* :1667-1686 屏外判定：AbsVertUB!=0 或 AbsHorizUB>=2 → bit7 置；
     否则 16 位减法 LB-Scroll 后 UB-0-borrow==0（即 0 ≤ absX-Scroll ≤ $FF）才在屏——
     初版误用 LB>=Scroll 单字节比较：UB=1 且 LB<Scroll（右半图近右缘对象，如屏3
     大门 absX=$160）被错标屏外（不可见不可击中，用户报告大门炸不开根因），
     UB=1 且 LB>=Scroll（越右缘）又被错标在屏 */
  {
    uint8_t lb = JackalRam.SpriteAbsoluteHorizPositionLB[x];
    uint8_t ub = JackalRam.SpriteAbsoluteHorizPositionUB[x];
    uint8_t scroll = JackalRam.ScreenLeftScrollPosition;
    uint8_t noBorrow = (lb >= scroll) ? 1u : 0u;   /* SEC SBC：无借位 C=1 */
    uint8_t a = (uint8_t)(ub - (1u - noBorrow));   /* :1681-1683 UB SBC #$00 */
    if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 0 && ub < 2u && a == 0u) {
      JackalRam.SpriteHorizScreenPosition[x] = (uint8_t)(lb - scroll);
      JackalRam.SpriteState[x] &= 0x7Fu;
    } else {
      JackalRam.SpriteState[x] |= 0x80u;
    }
  }
}

void subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(uint8_t x) {  /* :7068 */
  subProcessObjectSpeed_UpdatePosition(x);
  subUpdateSpritePositionForScrolling(x);
  subCheckSpriteDespawnIfOffscreen(x);
}

void Label244(uint8_t x) {  /* :7073-7115 */
  subProcessObjectSpeed_UpdatePosition(x);
  subUpdateSpritePositionForScrolling(x);
  /* :7076-7087：$10-$15 = 当前 24 位坐标（fctTestForMovementCollision 输入） */
  JackalRam.Raw[0x10] = JackalRam.SpriteHorizScreenPositionSubPixel[x];
  JackalRam.Raw[0x11] = JackalRam.SpriteAbsoluteHorizPositionLB[x];
  JackalRam.Raw[0x12] = JackalRam.SpriteAbsoluteHorizPositionUB[x];
  JackalRam.Raw[0x13] = JackalRam.SpriteVertScreenPositionSubPixel[x];
  JackalRam.Raw[0x14] = JackalRam.SpriteVertScreenPosition[x];
  JackalRam.Raw[0x15] = JackalRam.SpriteAbsoluteVertPositionUB[x];
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 0 &&
      fctTestForMovementCollision(x) != 0) {
    /* :7090-7096：屏内有碰撞且贴边（Y<$08 或 ≥$E8）→ 消亡 */
    if (JackalRam.SpriteVertScreenPosition[x] < 0x08u ||
        JackalRam.SpriteVertScreenPosition[x] >= 0xE8u) {
      subDespawnSprite(x);
      return;
    }
  }
  subCheckSpriteDespawnIfOffscreen(x);       /* :7098-7115 */
}

/* ---------------------------------------------------------------- 目标吉普/距离 */

uint8_t fctGetDistanceBetweenEnemyAndJeep(uint8_t x) {  /* :7156-7197 */
  uint8_t y = (int8_t)JackalRam.SpriteWhichJeeptoAttack[x] < 0 ? 1u : 0u;
  uint16_t jeepX = (uint16_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] +
                   JackalRam.ScreenLeftScrollPosition;
  int32_t dx = (int32_t)jeepX -
               ((int32_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 8 |
                JackalRam.SpriteAbsoluteHorizPositionLB[x]);
  int32_t dy = (int32_t)JackalRam.SpriteVertScreenPosition[0x10 + y] -
               (((int32_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 8) |
                JackalRam.SpriteVertScreenPosition[x]);
  uint32_t adx = dx < 0 ? (uint32_t)(-dx) : (uint32_t)dx;
  uint32_t ady = dy < 0 ? (uint32_t)(-dy) : (uint32_t)dy;
  if ((adx >> 8) != 0) {                       /* :7178-7179 UB!=0 → $FF */
    return 0xFF;
  }
  if ((ady >> 8) != 0) {                       /* :7189-7190 */
    return 0xFF;
  }
  if (adx + ady > 0xFFu) {                     /* :7194 BCS */
    return 0xFF;
  }
  return (uint8_t)(adx + ady);
}

void subCountDownForJeepTargetBy1(uint8_t x) {  /* :7199-7210 */
  if ((JackalRam.SpriteWhichJeeptoAttack[x] & 0x7Fu) == 0) {
    JackalRam.SpriteWhichJeeptoAttack[x] += 0xFFu;   /* 低 7=0 → 回绕 $FF（bit7 保留） */
  }
  /* SEC RTS（调用方 By1 后不走 C 分支——见 Label228） */
}
void subCountDownForJeepTarget(uint8_t x) {     /* :7212-7215 */
  JackalRam.SpriteWhichJeeptoAttack[x]--;            /* CLC RTS */
}

void subCheckWhichJeepToAttack(uint8_t x) {     /* :7217-7227：槽奇偶定 P1/P2 */
  if ((x & 1u) != 0) {
    JackalRam.SpriteWhichJeeptoAttack[x] |= 0x80u;
  } else {
    JackalRam.SpriteWhichJeeptoAttack[x] &= 0x7Fu;
  }
  (void)subCheckEnemyTarget_AttackOtherJeepIfDead(x);   /* :7224 JMP 落入 */
}

uint8_t subCheckEnemyTarget_AttackOtherJeepIfDead(uint8_t x) {  /* :7229-7241 */
  uint8_t y = (int8_t)JackalRam.SpriteWhichJeeptoAttack[x] < 0 ? 1u : 0u;
  if (JackalRam.SpriteState[0x10 + y] != 0) {
    return 0;                                /* CLC RTS：目标存活 */
  }
  JackalRam.SpriteWhichJeeptoAttack[x] += 0x80u;       /* 翻转目标 */
  return 1;                                  /* SEC RTS */
}

/* ---------------------------------------------------------------- 方向/速度计算 */

uint8_t subCalculateDirectionWithPresets(uint8_t x) {  /* :7269-7359 */
  uint32_t dy = ((uint32_t)JackalRam.Raw[0x15] << 16 |
                 (uint32_t)JackalRam.Raw[0x14] << 8 | JackalRam.Raw[0x13]);
  uint32_t dx = ((uint32_t)JackalRam.Raw[0x12] << 16 |
                 (uint32_t)JackalRam.Raw[0x11] << 8 | JackalRam.Raw[0x10]);
  uint32_t enemyY = ((uint32_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 16 |
                     (uint32_t)JackalRam.SpriteVertScreenPosition[x] << 8 |
                     JackalRam.SpriteVertScreenPositionSubPixel[x]);
  uint32_t enemyX = ((uint32_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 16 |
                     (uint32_t)JackalRam.SpriteAbsoluteHorizPositionLB[x] << 8 |
                     JackalRam.SpriteHorizScreenPositionSubPixel[x]);
  uint8_t z06 = 0, z01, z04, z07, y, a;
  dy = (dy - enemyY) & 0xFFFFFFu;
  if ((int32_t)(dy << 8) < 0) {              /* 24 位符号（bit23） */
    z06 |= 1u;
    dy = invert24(dy);
  }
  dx = (dx - enemyX) & 0xFFFFFFu;
  if ((int32_t)(dx << 8) < 0) {
    z06 |= 2u;
    dx = invert24(dx);
  }
  /* Label1286（:7307-7325）：双量同步 LSR 直到高字节均 <7 且第 17-24 位为 0 */
  while ((dx >> 16) != 0 || ((dx >> 8) & 0xFFu) >= 7u ||
         (dy >> 16) != 0 || ((dy >> 8) & 0xFFu) >= 7u) {
    dx >>= 1;
    dy >>= 1;
  }
  z01 = (uint8_t)(dx >> 8);
  z04 = (uint8_t)(dy >> 8);
  /* :7326-7331 舍入（LB bit7 → 高字节 +1） */
  if ((dx & 0x80u) != 0) { z01++; }
  if ((dy & 0x80u) != 0) { z04++; }
  z07 = (uint8_t)((z04 << 3) + z01);
  y = (uint8_t)(z07 >> 1);
  a = table1[y];
  a = (z07 & 1u) != 0 ? (uint8_t)(a & 0x0Fu) : (uint8_t)(a >> 4);
  if ((z06 & 1u) != 0) { a = fctInvertA(a); }        /* :7350-7352 */
  if ((z06 & 2u) != 0) { a = (uint8_t)(fctInvertA(a) + 0x10u); }  /* :7353-7357 */
  return (uint8_t)(a & 0x1Fu);
}

uint8_t subCalculateDirectionTowardJeep(uint8_t x) {  /* :7249-7268 */
  uint8_t y = (int8_t)JackalRam.SpriteWhichJeeptoAttack[x] < 0 ? 1u : 0u;
  uint16_t jeepX = (uint16_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] +
                   JackalRam.ScreenLeftScrollPosition;
  JackalRam.Raw[0x10] = JackalRam.SpriteHorizScreenPositionSubPixel[0x10 + y];
  JackalRam.Raw[0x11] = (uint8_t)jeepX;
  JackalRam.Raw[0x12] = (uint8_t)(jeepX >> 8);
  JackalRam.Raw[0x13] = JackalRam.SpriteVertScreenPositionSubPixel[0x10 + y];
  JackalRam.Raw[0x14] = JackalRam.SpriteVertScreenPosition[0x10 + y];
  JackalRam.Raw[0x15] = 0;
  return subCalculateDirectionWithPresets(x);
}

/* Label1321（:7478-7525）：$00/$01（16 位有符号基值）× $08（UB 整数倍 + LB LSR 微调） */
static uint16_t label1321(uint16_t base, uint8_t mult) {
  int32_t v = (int16_t)base;
  int32_t acc = v * (mult >> 4);             /* UB 整数倍累加 */
  uint8_t lb = (uint8_t)(mult & 0x0Fu);
  if (lb != 0) {
    acc += v >> lb;                          /* LSR 微调（算术右移保符号） */
  } else {
    /* :7515-7517 ++：LB==0 时微调段清零（$00/$01=0 再加）——与 v>>0=v 不同！ */
  }
  return (uint16_t)acc;
}

void subCalculateObjectSpeed(uint8_t x, uint8_t dir, uint8_t mult) {  /* :7425-7439 */
  uint16_t hv, vv;
  int16_t hBase, vBase;
  /* subGetHorizontalSpeed_DirectionOnCircle（:7441-7456） */
  hBase = tblCosineValues[dir & 0x0Fu];
  if (dir >= 0x08u && dir < 0x18u) {
    hBase = (int16_t)-hBase;
  }
  /* subGetVerticalSpeed_DirectionOnCircle（:7458-7471） */
  vBase = tblCosineValues[(uint8_t)(dir + 0x08u) & 0x0Fu];
  if (dir >= 0x10u) {
    vBase = (int16_t)-vBase;
  }
  hv = label1321((uint16_t)hBase, mult);
  vv = label1321((uint16_t)vBase, mult);
  JackalRam.SpriteHorizSpeedLB[x] = (uint8_t)hv;
  JackalRam.SpriteHorizSpeedUB[x] = (uint8_t)(hv >> 8);
  JackalRam.SpriteVertSpeedLB[x] = (uint8_t)vv;
  JackalRam.SpriteVertSpeedUB[x] = (uint8_t)(vv >> 8);
}

/* ---------------------------------------------------------------- 动画表 */

uint8_t fctGetSpriteOrientationIndex(uint8_t dir) {  /* :7008-7018 */
  uint8_t a = (uint8_t)(dir + 1u) & 0x1Fu;
  uint8_t y = 0;
  while ((int8_t)(a - 4u) >= 0) {            /* SEC SBC #$04 BCC + */
    a = (uint8_t)(a - 4u);
    y++;
  }
  return y;
}

void subUpdateSpriteForDirectionChange(uint8_t x, uint8_t dir) {  /* :7530-7541 */
  uint8_t y = fctGetSpriteOrientationIndex(dir);
  JackalRam.Raw[0x6A + x] = (uint8_t)(tblSpriteMirroringAndIndexForDirection[y] & 0xF0u);
  JackalRam.SpriteTypeIndex[x] =
      (uint8_t)(JackalRam.SpriteTypeIndex[x] +
                (tblSpriteMirroringAndIndexForDirection[y] & 0x0Fu));
}

void subProcessWalkingAnimation(uint8_t x, uint8_t dir) {  /* :7567-7581 */
  uint8_t y, lo;
  /* :7568 JSR subUpdateSpriteForDirectionChange（朝向组 attr + TypeIndex 朝向偏移）——
     初版漏调：TypeIndex 缺朝向偏移，多数方向帧错（用户报告"倒着走"根因） */
  subUpdateSpriteForDirectionChange(x, dir);
  y = fctGetSpriteOrientationIndex(dir);
  JackalRam.Raw[0x6A + x] = (uint8_t)(tblWalkingAnimationAttributes[y] & 0xF0u);
  lo = (uint8_t)(tblWalkingAnimationAttributes[y] & 0x0Fu);
  if (lo == 1u) {
    JackalRam.SpriteTypeIndex[x]++;
  } else if (lo == 0x0Fu) {
    JackalRam.SpriteTypeIndex[x]--;
  }
}

/* ---------------------------------------------------------------- 前眺碰撞（Bank6 :3968-4066） */

static uint8_t lookaheadCollision(uint8_t x, uint16_t ubCpu, uint16_t lbCpu) {
  const uint8_t *ubTbl = BankPtr(6, ubCpu);
  const uint8_t *lbTbl = BankPtr(6, lbCpu);
  uint8_t dir = JackalRam.SpriteWhatDirectionToShoot[x];
  uint8_t d = (uint8_t)(dir + 0x08u) & 0x1Fu;
  uint8_t y = (uint8_t)(d & 0x0Fu);
  int32_t off = ((int32_t)ubTbl[y] << 8) | lbTbl[y];
  uint32_t acc;
  if ((d & 0x10u) != 0) {
    off = -off;
  }
  /* fctAddToSpriteHorizPosition_24bit（:4066-…）：$10-$12 = 横向 24 位 + off */
  acc = ((uint32_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 16 |
         (uint32_t)JackalRam.SpriteAbsoluteHorizPositionLB[x] << 8 |
         JackalRam.SpriteHorizScreenPositionSubPixel[x]);
  acc = (uint32_t)((int32_t)acc + off) & 0xFFFFFFu;
  JackalRam.Raw[0x10] = (uint8_t)acc;
  JackalRam.Raw[0x11] = (uint8_t)(acc >> 8);
  JackalRam.Raw[0x12] = (uint8_t)(acc >> 16);
  y = (uint8_t)(dir & 0x0Fu);
  off = ((int32_t)ubTbl[y] << 8) | lbTbl[y];
  if ((dir & 0x10u) != 0) {
    off = -off;
  }
  /* fctAddToSpriteVertPosition_24bit_TestCollision（:4049-4062）：$13-$15 + off → 碰撞 */
  acc = ((uint32_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 16 |
         (uint32_t)JackalRam.SpriteVertScreenPosition[x] << 8 |
         JackalRam.SpriteVertScreenPositionSubPixel[x]);
  acc = (uint32_t)((int32_t)acc + off) & 0xFFFFFFu;
  JackalRam.Raw[0x13] = (uint8_t)acc;
  JackalRam.Raw[0x14] = (uint8_t)(acc >> 8);
  JackalRam.Raw[0x15] = (uint8_t)(acc >> 16);
  JackalRam.Raw[0xD7] = fctTestForMovementCollision(x);
  return JackalRam.Raw[0xD7];
}

uint8_t fctGetCollision_WithSpeed_NearLookAhead_BG(uint8_t x) {  /* :3968 */
  return lookaheadCollision(x, TBL_COLLISION_OFFSET_NEAR_UB, TBL_COLLISION_OFFSET_NEAR_LB);
}
uint8_t fctGetCollision_WithSpeed_FarLookAhead_BG(uint8_t x) {   /* :4012 */
  return lookaheadCollision(x, TBL_COLLISION_OFFSET_FAR_UB, TBL_COLLISION_OFFSET_FAR_LB);
}
uint8_t fctGetCollisionType_SwampInfantry(uint8_t x) {           /* :4004 */
  /* fctAddToSpriteHorizPosition_24bit/VertPosition 零偏移段：$10-$15 = 当前 24 位坐标 */
  JackalRam.Raw[0x10] = JackalRam.SpriteHorizScreenPositionSubPixel[x];
  JackalRam.Raw[0x11] = JackalRam.SpriteAbsoluteHorizPositionLB[x];
  JackalRam.Raw[0x12] = JackalRam.SpriteAbsoluteHorizPositionUB[x];
  JackalRam.Raw[0x13] = JackalRam.SpriteVertScreenPositionSubPixel[x];
  JackalRam.Raw[0x14] = JackalRam.SpriteVertScreenPosition[x];
  JackalRam.Raw[0x15] = JackalRam.SpriteAbsoluteVertPositionUB[x];
  JackalRam.Raw[0xD7] = fctTestForMovementCollision(x);
  return JackalRam.Raw[0xD7];
}

/* ---------------------------------------------------------------- 公共死亡/爆炸 */

void subInfantryDeath(uint8_t x) {  /* :7602-7615 */
  JackalRam.SpriteGraphicsAttributes[x] = 1;
  subClearSpriteSpeed(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x] = 0x20;
  JackalRam.SpriteTypeIndex[x] = 0x18;
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  subInitiateSoundClip(INFANTRY_SQUISH_CLIP);   /* stub */
  subMoveSpriteToNextState(x);
}

void subInfantryDeathAnimation(uint8_t x) {     /* :7617-7622 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] == 0) {
    subDespawnSprite(x);
  }
}

void subCheckForBossDeath_MultipleBossEnemies(uint8_t x) {  /* :7646-7655 */
  subClearSpriteSpeed(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  if (JackalRam.LevelBossEntitiesRemaining == 1u) {
    subStopMusic();                            /* stub：残 1 */
  }
  subMoveSpriteToNextState(x);
}

void subSpriteDeath(uint8_t x) {  /* :7657-7672 */
  subClearSpriteSpeed(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x] = 0x18;
  JackalRam.SpriteTypeIndex[x] = 0x19;
  JackalRam.SpriteGraphicsAttributes[x] = 3;
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {   /* 屏内才音效 stub */
    subInitiateSoundClip(MAINWEAPON_EXPLOSION_CLIP);
  }
  subMoveSpriteToNextState(x);
}

void subSpriteExplosion(uint8_t x) {  /* :7674-7685 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData8[x] == 0) {
    subDespawnSprite(x);
  }
}

/* ---------------------------------------------------------------- 分派（:7744） */

static void EMPTYSprite_RTS(uint8_t x) { (void)x; }

#define EL EMPTYSprite_RTS
/* dw 表顺序（:7757-7840）；Task 4.3+ 逐组替换 EL 为实 Logic */
static void (*const tblSpriteLogic[0x54])(uint8_t) = {
  EL,
  MobileInfantrySpriteLogic,                 /* 01 */
  StationaryInfantrySpriteLogic,             /* 02 */
  StationaryInfantrySpriteLogic,             /* 03 - Flame Thrower */
  MobileSwampInfantrySpriteLogic,            /* 04 */
  LargeGrayTurretWhiteBulletsSpriteLogic,    /* 05 */
  LargeGrayTurretYellowBulletSpriteLogic,    /* 06 */
  RedMediumTankSpriteLogic,                  /* 07 */
  Level1AttackBoatSpriteLogic,               /* 08 */
  Level1BossSpriteLogic,                     /* 09 */
  Level1BossTankSpriteLogic,                 /* 0A */
  SilverLargeTankSpriteLogic,                /* 0B */
  EL,                                        /* 0C */
  Level3LaserChargingFlashesSpriteLogic,     /* 0D */
  FlameTankSpriteLogic,                      /* 0E */
  EnemyJeepSpriteLogic,                      /* 0F */
  Level2PillarSpriteLogic,                   /* 10 - 左倒 */
  InfantryTruckSpriteLogic,                  /* 11 */
  SpreadTurretSpriteLogic,                   /* 12 */
  POWBuildingSpriteLogic,                    /* 13 - 2 POW 右出口 */
  POWBuildingSpriteLogic,                    /* 14 - 2 POW 左出口 */
  POWPowerUpBuildingSpriteLogic,             /* 15 */
  POWWalkingSpriteLogic,                     /* 16 */
  EL,                                        /* 17 */
  Level2BossStatueHeadSpriteLogic,           /* 18 */
  POWBuildingWithTankInsideSpriteLogic,      /* 19 */
  Level3LargeAttackBoatSpriteLogic,          /* 1A */
  GateSpriteLogic,                           /* 1B */
  POWBuildingSpriteLogic,                    /* 1C - 4 POW 右出口 */
  POWBuildingSpriteLogic,                    /* 1D - 4 POW 左出口 */
  Level2PillarTopSpriteLogic,                /* 1E */
  Level2PillarSpriteLogic,                   /* 1F - 右倒 */
  Level2BossSpriteLogic,                     /* 20 */
  Level2StatueHeadSpriteLogic,               /* 21 - 射击 */
  Level2StatueHeadSpriteLogic,               /* 22 - idle */
  EL, EL,                                    /* 23-24 */
  Level3BossSpriteLogic,                     /* 25 */
  EL,                                        /* 26 */
  POWLoadingIntoHeliSpriteLogic,             /* 27 */
  POWSpawnOnJeepDeathSpriteLogic,            /* 28 */
  SubmarineSpriteLogic,                      /* 29 */
  EL, EL, EL, EL,                            /* 2A-2D */
  Level6MissileLauncherSpriteLogic,          /* 2E */
  EL,                                        /* 2F */
  EL, EL, EL, EL,                            /* 30-33 */
  FlameThrowerSpriteLogic,                   /* 34 */
  EL,                                        /* 35 */
  EnemyBulletSpriteLogic,                    /* 36（:7749 特判直跳，表项占位同 ASM） */
  FallingBombSpriteLogic,                    /* 37 */
  Level3LaserSpriteLogic,                    /* 38 */
  BlackAndWhite_SmallMissileSpriteLogic,     /* 39 */
  AttackPlaneSpriteLogic,                    /* 3A - 定点 */
  AttackPlaneSpriteLogic,                    /* 3B - 随机 */
  FlyingOverheadHeliSpriteLogic,             /* 3C */
  LandedHeliSpriteLogic,                     /* 3D - 右装 */
  LandedHeliSpriteLogic,                     /* 3E - 左装 */
  POWDropOffSpriteLogic,                     /* 3F */
  EL, EL, EL,                                /* 40-42 */
  Level6AttackHelicopterSpriteLogic,         /* 43 */
  EscalatorSpriteLogic,                      /* 44 */
  Level6BossLoadSpriteLogic,                 /* 45 */
  EndofLevelCheckSpriteLogic,                /* 46 */
  Level6BossLaserTurretSpriteLogic,          /* 47 */
  Level6BossLaserTurretBlastSpriteLogic,     /* 48 */
  Level6BossLaserTurretGraphicsLoadSpriteLogic, /* 49 */
  Level6FinalBossSpriteLogic,                /* 4A */
  Level6FinalBossTankSpriteLogic,            /* 4B */
  Level6FinalBossTankFlameShotSpriteLogic,   /* 4C */
  Level6FinalBossTankFlameShotTipSpriteLogic,/* 4D */
  ParkedJeepTankSpriteLogic,                 /* 4E */
  Level6BossTankTurretSpriteLogic,           /* 4F */
  PowerUpStarSpriteLogic,                    /* 50 - 杀星 */
  PowerUpStarSpriteLogic,                    /* 51 - 满武器星 */
  PowerUpStarSpriteLogic,                    /* 52 - 命星 */
  ParkedJeepTankSpriteLogic,                 /* 53 - 星下藏车 */
};

void subProcessObjectLogic(uint8_t x) {  /* :7744-7840 */
  uint8_t id = (uint8_t)(JackalRam.SpriteObjectID[x] & 0x7Fu);
  if (id == 0) {
    return;
  }
  if (id == 0x36u) {                          /* :7749-7751 敌弹特判（省时直跳） */
    EnemyBulletSpriteLogic(x);
    return;
  }
  tblSpriteLogic[id](x);               /* subExecuteCodeViaIndirectJump 语义（函数指针表） */
}
