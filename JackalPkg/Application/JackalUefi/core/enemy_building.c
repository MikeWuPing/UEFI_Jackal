/* core/enemy_building.c：建筑/POW/星/门 + BG 更新子系统逐行翻译。
   表地址（ROM 签名定位核实）：tblObjectControlledGraphicsUpdate=$E099（29 项 dw→tile 数据区）、
   tblAddressForMatrix=$E24A（3 项 dw {$E250,$E27A,$E271}）、偏移表 horiz=$F394/vert=$F3B1（29 项）。
   PPU 队列写入经 $0770（type1 段，ppu.c subInGamePPUUpdates 消费——Task 2.4 已有）。
   音效调用点全 stub（参数=Sound.ASM 常量）。 */
#include "ram.h"
#include "bank.h"
#include "enemy_building.h"
#include "enemy_ai.h"
#include "enemy_infantry.h"
#include "spawn.h"
#include "weapon.h"
#include "ppu.h"
#include "sound_stub.h"

#define TBL_OBJ_CONTROLLED_GFX_UPDATE_CPU 0xE099u  /* :4154，29 项 dw */
#define TBL_ADDRESS_FOR_MATRIX_CPU        0xE24Au  /* :4356，3 项 dw */
#define TBL_GRAPHICS_UPDATE_HORIZ_OFF_CPU 0xF394u  /* :6887 */
#define TBL_GRAPHICS_UPDATE_VERT_OFF_CPU  0xF3B1u  /* :6931 */

#define POW_BUILDING_OPENING_CLIP 0x1Bu
#define POW_ENTERING_HELI_CLIP    0x1Fu
#define HELI_BLADES_CLIP          0x0Cu
#define BOMB_HIT_GROUND_CLIP      0x15u
#define MAINWEAPON_EXPLOSION_CLIP 0x17u

/* ---------------------------------------------------------------- BG 更新子系统 */

/* 尺寸分派（:6867-6877）：$08-$0B→1x1（Label1281）、$0E/$1A→2x2（Label1282）、
   其余全部→4x4（Label1280——含 $0C/$0D/$0F-$19/$1B+，ASM 分支落入 :6875） */
static uint8_t bgUpdateSizeKind(uint8_t idx) {
  if (idx >= 0x08u && idx < 0x0Cu) { return 1; }
  if (idx == 0x0Eu || idx == 0x1Au) { return 2; }
  return 0;
}

/* Label1028（:4400-4460）：16x16 段碰撞写——读当前 1x4 tile 打包字节，
   table10 掩码（$3F/$CF/$F3/$FC）保留邻域，$16（新 2-bit 类型）按 (x&$1F)>>3 移位 ORA 写回。
   输入 $11=X LB、$14=Y、$12 页（0=$0300/!=0=$0400，fctTestForMovementCollision 同约定） */
static void Label1028(void) {
  static const uint8_t table10[4] = { 0x3F, 0xCF, 0xF3, 0xFC };
  uint16_t page = JackalRam.Raw[0x12] != 0 ? 0x0400u : 0x0300u;
  uint8_t a = (uint8_t)(JackalRam.Raw[0x14] -
                        (uint8_t)(JackalRam.CurrentLevelScreenSubPosition & 7u));
  uint8_t yIdx, shift, packed, keep;
  a = (uint8_t)(a & 0xF8u);
  a = (uint8_t)(a + JackalRam.Zp4F);
  a = (uint8_t)(a + 8u);
  yIdx = (uint8_t)(((JackalRam.Raw[0x11] & 0xE0u) >> 5) + a);
  shift = (uint8_t)((JackalRam.Raw[0x11] & 0x1Fu) >> 3);
  packed = JackalRam.Raw[page + yIdx];
  keep = (uint8_t)(packed & table10[shift]);
  JackalRam.Raw[page + yIdx] = (uint8_t)(keep | (JackalRam.Raw[0x16] << (shift * 2u)));
}

/* Label1037（:4056-4149）：PPU 地址合成 + tile 数据写 $0770 队列（type1 段，按行循环）。
   输入：Raw[0x00]=横向 UB（页选择 0→$20/!=0→$24）、Raw[0x01]=横向 LB（/8 列）、
        Raw[0x02]=首行 Y、gfx=tile 数据表、rows/perRow=table12[$D8]（{4,1,2}）、
        Raw[0x04]=数据游标（调用方清 0，连续跨行消费） */
static void Label1037(const uint8_t *gfx, uint8_t rows, uint8_t perRow) {
  uint8_t row;
  for (row = 0; row < rows; row++) {
    uint8_t addrUB, addrLB;
    uint8_t i = JackalRam.PPUGraphicsUpdateByteLength;
    uint16_t wide;
    uint8_t t;
    /* :4059-4095：页基（$00==0→$20/!=0→$24）、Y 合成（sub==0→0 否则 (sub^$FF)-$0F、
       +Y、越 $F0 回 +$10、&$F8、ASL ROL ×2（/8 行）、X/8 列、页基加 UB） */
    addrUB = JackalRam.Raw[0x00] == 0 ? 0x20u : 0x24u;
    wide = (uint16_t)JackalRam.Raw[0x02];
    if (JackalRam.CurrentLevelScreenSubPosition != 0) {
      wide = (uint16_t)(wide + (uint8_t)((JackalRam.CurrentLevelScreenSubPosition ^ 0xFFu) - 0x0Fu));
    }
    if (wide >= 0xF0u) { wide = (uint16_t)(wide + 0x10u); }
    wide = (uint16_t)((wide & 0xF8u) * 4u);
    addrLB = (uint8_t)(wide + (uint16_t)(JackalRam.Raw[0x01] >> 3));
    addrUB = (uint8_t)(addrUB + (uint8_t)(wide >> 8));
    /* :4096-4110：首行带 type1 字节，后续行带 $FF,$01 段间隔 */
    if (row == 0) {
      JackalRam.PPUUpdateQueue[i++] = 1;
    } else {
      JackalRam.PPUUpdateQueue[i++] = 0xFF;
      JackalRam.PPUUpdateQueue[i++] = 1;
    }
    JackalRam.PPUUpdateQueue[i++] = addrUB;
    JackalRam.PPUUpdateQueue[i++] = addrLB;
    /* :4111-4128：tile 数据连续写 perRow 个 */
    for (t = 0; t < perRow; t++) {
      JackalRam.PPUUpdateQueue[i++] = gfx[JackalRam.Raw[0x04]++];
    }
    JackalRam.PPUGraphicsUpdateByteLength = i;
    /* :4130-4133 下一行（$02+=8，≥$E8 止） */
    JackalRam.Raw[0x02] = (uint8_t)(JackalRam.Raw[0x02] + 8u);
    if (JackalRam.Raw[0x02] >= 0xE8u) { break; }
  }
  /* :4144-4149 收尾：$FF,$00 段终结 */
  {
    uint8_t i = JackalRam.PPUGraphicsUpdateByteLength;
    JackalRam.PPUUpdateQueue[i++] = 0xFF;
    JackalRam.PPUUpdateQueue[i++] = 0;
    JackalRam.PPUGraphicsUpdateByteLength = i;
  }
}

/* Label1280/1281/1282 公共主体（:3985-4151）：$D8=尺寸（0=4x4/1=1x1/2=2x2） */
static void label1280Body(uint8_t x, uint8_t sizeIdx, uint8_t gfxIdx) {
  static const uint8_t tblPPUGraphicsUpdateBytesToLoad2[3] = { 0x10, 0x01, 0x04 };
  static const uint8_t table12[3] = { 0x04, 0x01, 0x02 };
  const uint8_t *matrix;
  const uint8_t *gfx;
  uint16_t mofs, gofs;
  uint8_t i;
  if ((int8_t)JackalRam.SpriteState[x] < 0) {          /* :3996-3997 屏外不更新 */
    return;
  }
  if (JackalRam.PPUGraphicsUpdateByteLength >= 0x30u) {  /* :3998-4000 队列余量 */
    return;
  }
  if (JackalRam.Raw[0x02] >= 0xE8u) {                  /* :4001-4003 */
    return;
  }
  /* :4004-4015：矩阵地址（tblAddressForMatrix[$D8*2]）+ 数据表（tblObjectControlledGraphicsUpdate[$03*2]）
     与 $16=数据表[tile 数]（碰撞类型，图形之后紧跟） */
  mofs = (uint16_t)(sizeIdx * 2u);
  matrix = BankPtr(7, (uint16_t)(BankPtr(7, TBL_ADDRESS_FOR_MATRIX_CPU)[mofs] |
                                 ((uint16_t)BankPtr(7, TBL_ADDRESS_FOR_MATRIX_CPU)[mofs + 1u] << 8)));
  gofs = (uint16_t)(gfxIdx * 2u);
  gfx = BankPtr(7, (uint16_t)(BankPtr(7, TBL_OBJ_CONTROLLED_GFX_UPDATE_CPU)[gofs] |
                              ((uint16_t)BankPtr(7, TBL_OBJ_CONTROLLED_GFX_UPDATE_CPU)[gofs + 1u] << 8)));
  JackalRam.Raw[0x16] = gfx[tblPPUGraphicsUpdateBytesToLoad2[sizeIdx]];
  JackalRam.Raw[0x04] = 0;                             /* :4051 数据游标 */
  /* :4027-4044 矩阵循环（$80 尾）：逐 tile Label1028 碰撞写（$11/$14=基础+矩阵偏移） */
  for (i = 0; ; i = (uint8_t)(i + 2u)) {
    int8_t mx = (int8_t)matrix[i];
    if (mx < 0) { break; }                             /* BMI +（$80 尾） */
    JackalRam.Raw[0x11] = (uint8_t)(JackalRam.Raw[0x01] + (uint8_t)mx);
    JackalRam.Raw[0x14] = (uint8_t)(JackalRam.Raw[0x02] + matrix[i + 1u]);
    if (JackalRam.Raw[0x14] >= 0xE8u) { break; }       /* :4038-4039 越界 */
    Label1028();
  }
  Label1037(gfx, table12[sizeIdx], table12[sizeIdx]);
}

void subUpdateBGGraphicsFromSprite(uint8_t x, uint8_t idx) {  /* :6833-6836（JMP :6848 尺寸分派） */
  /* :6850-6866：$02=Y+vert 偏移、$00/$01=X+horiz 偏移（16 位，$00=UB 页选择） */
  uint16_t wide = (uint16_t)(((uint16_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] << 8) |
                             JackalRam.SpriteAbsoluteHorizPositionLB[x]);
  wide = (uint16_t)(wide + (int16_t)(int8_t)BankPtr(7, TBL_GRAPHICS_UPDATE_HORIZ_OFF_CPU)[idx]);
  JackalRam.Raw[0x01] = (uint8_t)wide;
  JackalRam.Raw[0x00] = (uint8_t)(wide >> 8);
  JackalRam.Raw[0x02] = (uint8_t)(JackalRam.SpriteVertScreenPosition[x] +
                                  BankPtr(7, TBL_GRAPHICS_UPDATE_VERT_OFF_CPU)[idx]);
  label1280Body(x, bgUpdateSizeKind(idx), idx);
}

void subUpdateBGGraphicsFromSprite_Every7thRNGFrame(uint8_t x, uint8_t idx) {  /* :6839-6878 */
  if ((uint8_t)(x + JackalRam.RNG_INCEveryFrame) & 7u) {
    return;                                            /* :6842-6847 每 8 帧才更新 */
  }
  if (JackalRam.SpriteObjectID[x] == 0) {
    return;                                            /* :6848-6849 */
  }
  subUpdateBGGraphicsFromSprite(x, idx);
}

/* ---------------------------------------------------------------- 公共例程（:7687-7739） */

void subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision(uint8_t x) {  /* :7703-7716 */
  subClearSpriteSpeed(x);
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData8[x] = 0x18;
  JackalRam.SpriteTypeIndex[x] = 0x19;
  JackalRam.SpriteGraphicsAttributes[x] = 3;
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  subUpdateBGGraphicsFromSprite(x, JackalRam.SpriteData1[x]);
  subMoveSpriteToNextState(x);
}

void subPlayBarracksOpeningSound_LoadNewBuildingBGGraphics(uint8_t x) {  /* :7687-7690 */
  subInitiateSoundClip(POW_BUILDING_OPENING_CLIP);     /* stub */
  subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision(x);
}

void subPlayMissileHittingGroundSound_LoadHoleInGroundBGGraphics(uint8_t x) {  /* :7692-7695 */
  subInitiateSoundClip(BOMB_HIT_GROUND_CLIP);          /* stub */
  subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision(x);
}

void subPlayExplosionSound_LoadNewBGGraphics(uint8_t x) {  /* :7697-7701 */
  if ((int8_t)JackalRam.SpriteState[x] >= 0) {
    subInitiateSoundClip(MAINWEAPON_EXPLOSION_CLIP);   /* stub */
  }
  subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision(x);
}

void subProcessExplosionAnimation(uint8_t x) {  /* :7718-7733 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  if (JackalRam.SpriteData8[x] == 0) {
    JackalRam.SpriteTypeIndex[x] = 0;
    subMoveSpriteToNextState(x);
  }
}

void subScrollSprite_CheckForDespawn_UpdateBG(uint8_t x) {  /* :7736-7739 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
}

/* ---------------------------------------------------------------- POW 建筑（:2910-3012、7145-7234） */

void subSpawnPOW(uint8_t x) {  /* :2998-3012（Y 返回语义经 SpriteData6 回写） */
  uint8_t slot;
  JackalSpawnZp[0x08] = 0x96;                        /* POW Walking（bit7 高优先） */
  JackalSpawnZp[0x0A] = JackalRam.SpriteData5[x];
  JackalSpawnZp[0x0B] = JackalRam.SpriteData6[x];
  JackalSpawnZp[0x0C] = JackalRam.SpriteWhatDirectionToShoot[x];
  JackalSpawnZp[0x0F] = 0x00;
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  slot = subSpawnObjectFromParent();
  JackalRam.SpriteData6[x] = slot;                   /* :3010-3011 TYA STA（$FF=无槽） */
}

/* tblLeftFacing/RightFacingPOWBuildingBGGraphicsUpdateIndex（:7182-7196） */
static const uint8_t tblLeftFacingPOWBuildingBGGraphicsUpdateIndex[6] = {
  0x01, 0x01, 0x0C, 0x01, 0x01, 0x19,
};
static const uint8_t tblRightFacingPOWBuildingBGGraphicsUpdateIndex[6] = {
  0x02, 0x02, 0x0D, 0x15, 0x0F, 0x17,
};

static void POWBuildingState0(uint8_t x) {  /* :7160-7176 */
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = tblLeftFacingPOWBuildingBGGraphicsUpdateIndex[JackalRam.CurrentLevel];
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x10;    /* 默认左出口 */
  if (JackalRam.SpriteObjectID[x] != 0x94u && JackalRam.SpriteObjectID[x] != 0x9Du) {
    JackalRam.SpriteData1[x] = tblRightFacingPOWBuildingBGGraphicsUpdateIndex[JackalRam.CurrentLevel];
    JackalRam.SpriteWhatDirectionToShoot[x] = 0x00;  /* 右出口 */
  }
  subMoveSpriteToNextState(x);
}

static void POWBuildingState4(uint8_t x) {  /* :2910-2935 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  JackalRam.SpriteData4[x] = 4;
  if (JackalRam.SpriteObjectID[x] != 0x9Du && JackalRam.SpriteObjectID[x] != 0x9Cu) {
    JackalRam.SpriteData4[x] >>= 1;                  /* 2 POW 建筑 */
  }
  JackalRam.SpriteData5[x] = 0;                      /* 站立挥手型 */
  JackalRam.SpriteData6[x] = 0xFF;                   /* 首个 POW（无前置槽） */
  JackalRam.SpriteData8[x] = 0x40;                   /* HELP 闪烁计时 */
  JackalRam.SpriteHitboxShapeIndex[x] = 0xF0;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  subMoveSpriteToNextState(x);
}

static void POWBuildingState5(uint8_t x) {  /* :2937-2954（HELP 文本闪烁） */
  JackalRam.SpriteTypeIndex[x] = 0x2D;
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) == 0) {
    JackalRam.SpriteTypeIndex[x] = 0;                /* 闪烁熄灭帧 */
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  if (JackalRam.SpriteData8[x] == 0) {
    subMoveSpriteToNextState(x);
  }
}

static void POWBuildingState6(uint8_t x) {  /* :2956-2963 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  subSpawnPOW(x);
  if (JackalRam.SpriteData6[x] != 0xFFu) {           /* BMI +（slot bit7=$FF 无槽） */
    subMoveSpriteToNextState(x);
  }
}

static void POWBuildingState7(uint8_t x) {  /* :2965-2985 */
  uint8_t prev;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  prev = JackalRam.SpriteData6[x];
  if ((int8_t)prev >= 0) {                           /* :2970-2978 前置 POW 走出（State>=2）再放下一个 */
    if (JackalRam.SpriteObjectID[prev] == 0x96u &&
        (JackalRam.SpriteState[prev] & 0x7Fu) < 2u) {
      return;                                        /* ++ :2983-2984 RTS（等待） */
    }
  }
  JackalRam.SpriteData4[x]--;
  if (JackalRam.SpriteData4[x] == 0) {               /* :2985 → subScrollSprite_CheckForDespawn_UpdateBG */
    subMoveSpriteToNextState(x);
    return;
  }
  subSpawnPOW(x);                                    /* :2982 JMP */
}

void POWBuildingSpriteLogic(uint8_t x) {  /* :7145-7158（dw 9 项） */
  static void (*const tbl[9])(uint8_t) = {
    POWBuildingState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    subPlayBarracksOpeningSound_LoadNewBuildingBGGraphics,
    subProcessExplosionAnimation,
    POWBuildingState4, POWBuildingState5, POWBuildingState6, POWBuildingState7,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* tblPOWPowerUpBuildingBGUpdateIndex（:7233-7234） */
static const uint8_t tblPOWPowerUpBuildingBGUpdateIndex[6] = {
  0x03, 0x04, 0x03, 0x16, 0x10, 0x03,
};

static void POWPowerUpBuildingState0(uint8_t x) {  /* :7223-7230 */
  JackalRam.SpriteData1[x] = tblPOWPowerUpBuildingBGUpdateIndex[JackalRam.CurrentLevel];
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x08;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subMoveSpriteToNextState(x);
}

static void POWPowerUpBuildingState4(uint8_t x) {  /* :2987-2996 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  JackalRam.SpriteData5[x] = 1;                      /* 走动型 power up POW */
  subSpawnPOW(x);
  if (JackalRam.SpriteData6[x] != 0xFFu) {
    subMoveSpriteToNextState(x);
  }
}

void POWPowerUpBuildingSpriteLogic(uint8_t x) {  /* :7211-7221（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    POWPowerUpBuildingState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    subPlayBarracksOpeningSound_LoadNewBuildingBGGraphics,
    subProcessExplosionAnimation,
    POWPowerUpBuildingState4,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

static void POWBuildingWithTankInsideState0(uint8_t x) {  /* :5557-5562 */
  JackalRam.SpriteData1[x] = 0x10;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subMoveSpriteToNextState(x);
}
static void POWBuildingWithTankInsideState4(uint8_t x) {  /* :5564-5575 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  JackalSpawnZp[0x08] = 0x23;                        /* L5 boss silver tank */
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x35] = x;
  if (subSpawnObjectFromParent() != 0xFFu) {
    subMoveSpriteToNextState(x);
  }
}
void POWBuildingWithTankInsideSpriteLogic(uint8_t x) {  /* :5545-5555（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    POWBuildingWithTankInsideState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    subPlayBarracksOpeningSound_LoadNewBuildingBGGraphics,
    subProcessExplosionAnimation,
    POWBuildingWithTankInsideState4,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- POWWalking（:3027-3205） */

static void POWWalkingState0(uint8_t x) {  /* :3040-3047 */
  JackalRam.SpriteTypeIndex[x] = 0x00;
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x] = (uint8_t)(JackalRam.RNG_INCEveryFrame & 1u);
  subMoveSpriteToNextState(x);
}

static void POWWalkingState1(uint8_t x) {  /* :3049-3089 */
  JackalRam.SpriteTypeIndex[x] = 0x2B;
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData5[x] == 0) {
    /* :3060-3066：Data6 槽的 POW 已存在（$96）→ RTS 等待（前置走出判定） */
    if ((int8_t)JackalRam.SpriteData6[x] >= 0 &&
        JackalRam.SpriteObjectID[JackalRam.SpriteData6[x]] == 0x96u) {
      return;                                        /* +++ :3083-3084 */
    }
    JackalRam.SpriteData8[x] = 0x48;
    JackalRam.Raw[0x08] = 0x10;
  } else {
    if ((JackalRam.SpriteData5[x] & 2u) == 0) {      /* :3072-3078 */
      JackalRam.SpriteData8[x] = 0x1C;
      JackalRam.Raw[0x08] = 0x20;
    } else {
      subMoveSpriteToNextState(x);                   /* :3085-3089（Data5>=2 → State2） */
      JackalRam.Raw[0x08] = 0x10;
    }
  }
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], JackalRam.Raw[0x08]);
  subMoveSpriteToNextState(x);
}

static void POWWalkingState2(uint8_t x) {  /* :3091-3129 */
  if (JackalRam.SpriteData5[x] != 0) {               /* 闪烁（power up） */
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  }
  JackalRam.SpriteTypeIndex[x] = 0x25;
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) == 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.Raw[0x08] = 0x10;
  JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)(JackalRam.RNG_INCEveryFrame & 0x1Fu);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x10);
  JackalRam.SpriteData8[x] = 0x80;
  JackalRam.SpriteHitboxShapeIndex[x] = 0x73;          /* :3122-3123 可拾取态 */
  if (JackalRam.SpriteData5[x] == 0) {
    subClearSpriteSpeed(x);
    JackalRam.SpriteData8[x] = 1;
  }
  subMoveSpriteToNextState(x);
}

static void POWWalkingState3(uint8_t x) {  /* :3131-3164 */
  if ((JackalRam.SpriteData5[x] & 1u) != 0) {
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  }
  JackalRam.SpriteTypeIndex[x] = 0x25;
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  if (fctGetCollision_WithSpeed_NearLookAhead_BG(x) != 0) {
    subClearSpriteSpeed(x);
  }
  Label244(x);
  if (JackalRam.Raw[0xD7] != 0 || JackalRam.SpriteData8[x] == 0) {
    JackalRam.SpriteData8[x] = 0x63;
    subClearSpriteSpeed(x);
    subMoveSpriteToNextState(x);
  }
}

static void POWWalkingState4(uint8_t x) {  /* :3166-3205 */
  if ((JackalRam.SpriteData5[x] & 1u) != 0) {
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  }
  JackalRam.SpriteTypeIndex[x] = 0x2B;
  JackalRam.SpriteData8[x]--;
  if ((JackalRam.SpriteData8[x] & 8u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  Label244(x);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteData8[x] = 0x80;
  JackalRam.Raw[0x08] = 0x10;
  /* :3190-3199：Data2 方向随机（0=+RNG、1=-RNG） */
  {
    uint8_t d = JackalRam.SpriteData2[x] == 0
        ? (uint8_t)(x + JackalRam.RNG_INCEveryFrame)
        : (uint8_t)(x - JackalRam.RNG_INCEveryFrame);
    JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)(d & 0x1Fu);
  }
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x10);
  if (JackalRam.SpriteData5[x] == 0) {
    subClearSpriteSpeed(x);                          /* :3204 普通 POW 停下 */
    return;
  }
  subMoveSpriteToPreviousState(x);                   /* :3205（power up 来回走） */
}

void POWWalkingSpriteLogic(uint8_t x) {  /* :3027-3038（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    POWWalkingState0, POWWalkingState1, POWWalkingState2,
    POWWalkingState3, POWWalkingState4, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- POWLoadingIntoHeli（:3220-3293） */

static void POWLoadingIntoHeliState0(uint8_t x) {  /* :3229-3263 */
  uint8_t y = JackalRam.SpriteData4[x];              /* Player 1/2 */
  JackalRam.SpriteTypeIndex[x] = 0x25;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  JackalRam.SpriteVertScreenPosition[x] = JackalRam.SpriteVertScreenPosition[0x10 + y];
  {
    uint16_t wx = (uint16_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] +
                  JackalRam.ScreenLeftScrollPosition;
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wx;
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wx >> 8);
  }
  JackalRam.SpriteData3[x] = JackalRam.JeepPOWCount[y];
  JackalRam.SpriteWhatDirectionToShoot[x] =
      (int8_t)JackalRam.POWDropOffWalkDirection < 0 ? 0x10u : 0x00u;  /* :3249-3252（负=左走 $10、正=右走 $00） */
  subProcessWalkingAnimation(x, JackalRam.SpriteWhatDirectionToShoot[x]);
  JackalRam.SpriteData2[x] = JackalRam.SpriteTypeIndex[x];
  JackalRam.SpriteData1[x] = 0x60;                   /* JeepPOWWalkToHeliValue */
  subUpdateSpritePositionForScrolling(x);
  subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x10);  /* JeepPOWDropoffSpeedMultiplier */
  subMoveSpriteToNextState(x);
}

static void POWLoadingIntoHeliState1(uint8_t x) {  /* :3265-3293 */
  if (JackalRam.SpriteData3[x] == 0) {               /* 最后一个 POW 闪烁 */
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  }
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData2[x];
  JackalRam.SpriteData1[x]--;
  if ((JackalRam.SpriteData1[x] & 8u) != 0) {
    JackalRam.SpriteTypeIndex[x] += 3;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteData1[x] != 0) {
    return;
  }
  subInitiateSoundClip(POW_ENTERING_HELI_CLIP);      /* stub */
  subGetObjectPointsValue_AddToPlayerScore(JackalRam.SpriteData4[x], x);
  subDespawnSprite(x);
}

void POWLoadingIntoHeliSpriteLogic(uint8_t x) {  /* :3220-3227（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    POWLoadingIntoHeliState0, POWLoadingIntoHeliState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- POWSpawnOnJeepDeath（:3307-3367） */

static void POWSpawnOnJeepDeathState0(uint8_t x) {  /* :3316-3346 */
  uint8_t y = JackalRam.SpriteData4[x];
  uint8_t count;
  JackalRam.SpriteTypeIndex[x] = 0;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  JackalRam.SpriteVertScreenPosition[x] = JackalRam.SpriteVertScreenPosition[0x10 + y];
  {
    uint16_t wx = (uint16_t)JackalRam.SpriteHorizScreenPosition[0x10 + y] +
                  JackalRam.ScreenLeftScrollPosition;
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wx;
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wx >> 8);
  }
  count = JackalRam.SpriteData6[x];                  /* 吉普携带 POW 数 */
  if (count == 0 || (uint8_t)(count - 1u) == 0) {    /* :3333-3346 无 POW/仅 1 → 清零消亡 */
    JackalRam.JeepPOWCount[y] = 0;
    subDespawnSprite(x);
    return;
  }
  count--;
  if (count >= 5u) { count = 4; }                    /* :3339-3340 cap 4 */
  JackalRam.SpriteData1[x] = count;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void POWSpawnOnJeepDeathState1(uint8_t x) {  /* :3348-3367 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]--;
  if ((int8_t)JackalRam.SpriteData1[x] < 0) {
    subMoveSpriteToNextState(x);                     /* ++ :3367 → despawn */
    return;
  }
  JackalRam.SpriteData5[x] = 2;                      /* 走动型 */
  JackalRam.SpriteWhatDirectionToShoot[x] = (uint8_t)(
      (uint8_t)(JackalRam.SpriteData1[x] * 8u + JackalRam.RNG_INCEveryFrame) & 0x1Fu);
  if (JackalRam.SpriteWhatDirectionToShoot[x] == 0) {
    JackalRam.SpriteData5[x]++;                      /* 1/32 → power up 型 */
  }
  subSpawnPOW(x);                                    /* :3366 JMP */
}

void POWSpawnOnJeepDeathSpriteLogic(uint8_t x) {  /* :3307-3314（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    POWSpawnOnJeepDeathState0, POWSpawnOnJeepDeathState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- POWDropOff（:7512-7574） */

/* tblHelipadSpawnHarassmentEnemies/Palette（:7565-7574） */
static const uint8_t tblHelipadSpawnHarassmentEnemies[6] = {
  0x00, 0x3B, 0x00, 0x00, 0x3B, 0x43,
};
static const uint8_t tblHelipadSpawnHarassmentEnemiesPalette[6] = {
  0x00, 0x01, 0x00, 0x00, 0x01, 0x02,
};

static void POWDropOffState0(uint8_t x) {  /* :7521-7523 */
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void POWDropOffState1(uint8_t x) {  /* :7525-7561 */
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData1[x]++;
  if (JackalRam.SpriteData1[x] == 0) {               /* :7527-7558 每 $200 帧检查 */
    JackalRam.SpriteData2[x]++;
    if ((JackalRam.SpriteData2[x] & 1u) == 0 &&
        JackalRam.SpriteData3[x] != JackalRam.CurrentLevel &&
        tblHelipadSpawnHarassmentEnemies[JackalRam.CurrentLevel] != 0) {
      uint16_t vy;
      JackalRam.SpriteData3[x]++;
      JackalSpawnZp[0x08] = tblHelipadSpawnHarassmentEnemies[JackalRam.CurrentLevel];
      JackalSpawnZp[0x0F] = tblHelipadSpawnHarassmentEnemiesPalette[JackalRam.CurrentLevel];
      /* :7549-7557：(X,-Y) 偏移——fctInvert24Bit 后加（对象 X,0） */
      JackalSpawnZp[0x00] = 0;
      JackalSpawnZp[0x01] = 0;
      vy = (uint16_t)((uint16_t)JackalRam.SpriteAbsoluteVertPositionUB[x] << 8 |
                      JackalRam.SpriteVertScreenPosition[x]);
      vy = (uint16_t)(0u - vy);
      JackalSpawnZp[0x02] = (uint8_t)vy;
      JackalSpawnZp[0x03] = (uint8_t)(vy >> 8);
      JackalSpawnZp[0x35] = x;
      (void)subSpawnObjectFromParent();
    }
  }
  if (JackalRam.POWDropOffWalkDirection == 0) {
    subMoveSpriteToNextState(x);                     /* :7559-7560 BEQ - → despawn */
  }
}

void POWDropOffSpriteLogic(uint8_t x) {  /* :7512-7519（dw 3 项） */
  static void (*const tbl[3])(uint8_t) = {
    POWDropOffState0, POWDropOffState1, subDespawnSprite,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- Gate（:7588-7604） */

static void GateState0(uint8_t x) {  /* :7600-7604 */
  JackalRam.SpriteData1[x] = 0;
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}
void GateSpriteLogic(uint8_t x) {  /* :7588-7598（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    GateState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    subInitiateExplosionAnimation_LoadReplacementBackgroundTiles_Collision,
    subProcessExplosionAnimation,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- PowerUpStar（:7825-7885） */

static void PowerUpStarState0(uint8_t x) {  /* :7835-7857 */
  if ((int8_t)JackalRam.ContinuesUsed < 0) {         /* :7836-7839 通关后不再出星 */
    subDespawnSprite(x);
    return;
  }
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x40;
  /* :7849-7856：Level 3 且 ID=$D1（满武器星）→ 直接 State2 可见 */
  if (JackalRam.CurrentLevel == 2u && JackalRam.SpriteObjectID[x] == 0xD1u) {
    subSetSpriteState(x, 2);
    return;
  }
  subMoveSpriteToNextState(x);
}

static void PowerUpStarState2(uint8_t x) {  /* :7859-7874 */
  JackalRam.SpriteTypeIndex[x] = 0x2E;
  JackalRam.SpriteHitboxShapeIndex[x] = 0x74;        /* :7863 可拾取态 */
  JackalRam.SpriteHealthHP[x] = 1;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  JackalRam.EnemyPoints[x] = 0;
  if (JackalRam.SpriteObjectID[x] != 0xD2u) {
    JackalRam.SpriteGraphicsAttributes[x] = 1;       /* 杀星棕 palette */
  }
  subMoveSpriteToNextState(x);
}

static void PowerUpStarState3(uint8_t x) {  /* :7876-7885 */
  if (JackalRam.SpriteObjectID[x] == 0xD1u) {        /* 满武器星闪烁 */
    JackalRam.SpriteGraphicsAttributes[x]++;
    JackalRam.SpriteGraphicsAttributes[x] &= 3u;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
}

void PowerUpStarSpriteLogic(uint8_t x) {  /* :7825-7833（dw 4 项） */
  static void (*const tbl[4])(uint8_t) = {
    PowerUpStarState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    PowerUpStarState2, PowerUpStarState3,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- FlyingOverheadHeli（:7250-7292） */

static void FlyingOverheadHeliState0(uint8_t x) {  /* :7259-7277 */
  uint16_t wx;
  JackalRam.SpriteTypeIndex[x] = 0x31;
  wx = (uint16_t)((uint8_t)((JackalRam.RNG_INCEveryFrame & 0x7Fu) + 0x40u) +
                  JackalRam.ScreenLeftScrollPosition);
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wx;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wx >> 8);
  subCalculateObjectSpeed(x, 0x18, 0x50);            /* :7272-7275 向上 mult=$50 */
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}
static void FlyingOverheadHeliState1(uint8_t x) {  /* :7279-7292 */
  if ((JackalRam.SpriteData8[x] & 3u) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);          /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = 0x31;
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 2u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
}
void FlyingOverheadHeliSpriteLogic(uint8_t x) {  /* :7250-7257（dw 2 项） */
  static void (*const tbl[2])(uint8_t) = { FlyingOverheadHeliState0, FlyingOverheadHeliState1 };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- LandedHeli（:7307-7494） */

static void LandedHeliState0(uint8_t x) {  /* :7319-7340 */
  JackalRam.SpriteTypeIndex[x] = 0x2F;
  JackalRam.SpriteData3[x] = 0x2F;
  JackalRam.SpriteData1[x] = 0x40;
  JackalRam.SpriteData2[x] = 0x80;
  JackalRam.SpriteData5[x] = 0xFF;
  JackalRam.SpriteWhatDirectionToShoot[x] = 0x18;
  JackalRam.SpriteWhichJeeptoAttack[x] = 0x80;
  JackalRam.POWDropOffWalkDirection = JackalRam.SpriteObjectID[x] == 0xBDu ? 1u : 0xFFu;  /* :7334-7338 */
  subUpdateSpritePositionForScrolling(x);
  subMoveSpriteToNextState(x);
}

static void LandedHeliState1(uint8_t x) {  /* :7342-7369 */
  JackalRam.SpriteTypeIndex[x] = 0x2F;
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 4u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  JackalRam.SpriteData2[x]--;
  if ((int8_t)JackalRam.SpriteData2[x] >= 0) {
    return;                                          /* +++ :7369 RTS */
  }
  JackalRam.SpriteData2[x] = 0;                      /* :7353 INC 回 0（持续运行） */
  if ((uint8_t)(JackalRam.JeepPOWCount[0] + JackalRam.JeepPOWCount[1]) == 0) {
    subMoveSpriteToNextState(x);                     /* ++ :7367（无 POW → 离场序列） */
    return;
  }
  /* :7359-7365：吉普远离（heli 在屏下 $30）→ 同样离场 */
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 1u &&
      JackalRam.SpriteVertScreenPosition[x] >= 0x30u) {
    subMoveSpriteToNextState(x);
  }
}

static void LandedHeliState2(uint8_t x) {  /* :7371-7394 */
  JackalRam.SpriteTypeIndex[x] = 0x2F;
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 4u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 1u &&
      JackalRam.SpriteVertScreenPosition[x] < 0x30u) {
    subMoveSpriteToNextState(x);                     /* :7386-7387 → State3（起飞） */
    return;
  }
  JackalRam.SpriteData4[x]--;
  if (JackalRam.SpriteData4[x] != 0) {
    return;
  }
  if ((uint8_t)(JackalRam.JeepPOWCount[0] + JackalRam.JeepPOWCount[1]) == 0) {
    subMoveSpriteToNextState(x);                     /* -- :7393（无 POW → State3） */
    return;
  }
  subMoveSpriteToPreviousState(x);                   /* :7394（拾到 POW → 回 State1 等待） */
}

static void LandedHeliState3(uint8_t x) {  /* :7396-7439 */
  JackalRam.POWDropOffWalkDirection = 0;
  if ((JackalRam.SpriteData8[x] & JackalRam.SpriteData5[x]) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);          /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = JackalRam.SpriteData3[x];
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 2u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((JackalRam.SpriteData8[x] & 0x0Fu) == 0) {
    JackalRam.SpriteData5[x] >>= 1;                  /* :7413-7415 加速 ramp */
  }
  JackalRam.SpriteData1[x]--;
  if (JackalRam.SpriteData1[x] == 1u) {              /* :7418-7419 BEQ ++（升空） */
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x41);
    JackalRam.POWDropOffWalkDirection = 0;
    subMoveSpriteToNextState(x);
    return;
  }
  if (JackalRam.SpriteData1[x] == 0x20u) {
    JackalRam.SpriteData3[x] = 0x31;                 /* :7422-7423 离地换空中帧 */
  }
  /* :7424-7429：heli 已在屏下（UB==1）且 VertPos>=$30（玩家远离）→ 也升空 */
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 1u &&
      JackalRam.SpriteVertScreenPosition[x] >= 0x30u) {
    subCalculateObjectSpeed(x, JackalRam.SpriteWhatDirectionToShoot[x], 0x41);
    JackalRam.POWDropOffWalkDirection = 0;
    subMoveSpriteToNextState(x);
  }
  /* +: RTS */
}

static void LandedHeliState4(uint8_t x) {  /* :7441-7464 */
  if ((JackalRam.SpriteData8[x] & 3u) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);          /* stub */
  }
  JackalRam.SpriteTypeIndex[x] = 0x31;
  JackalRam.SpriteData8[x]++;
  if ((JackalRam.SpriteData8[x] & 2u) != 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  if ((int8_t)JackalRam.SpriteAbsoluteVertPositionUB[x] < 0) {
    return;                                          /* :7457 BPL -（屏上边界以上才隐藏） */
  }
  if (JackalRam.SpriteVertScreenPosition[x] < 0xF0u) {
    return;                                          /* :7459-7460 */
  }
  subClearSpriteSpeed(x);
  JackalRam.SpriteTypeIndex[x] = 0;
  subMoveSpriteToNextState(x);
}

static void LandedHeliState5(uint8_t x) {  /* :7466-7494 */
  uint16_t wx;
  if ((JackalRam.SpriteData8[x] & 3u) == 0) {
    subInitiateSoundClip(HELI_BLADES_CLIP);          /* stub */
  }
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData8[x]++;
  JackalRam.SpriteWhichJeeptoAttack[x]--;            /* :7474 $06E0,X 延迟计时 */
  if (JackalRam.SpriteWhichJeeptoAttack[x] != 0) {
    return;
  }
  /* :7476-7493 回场：顶部随机横向、向下 mult=$50、V 镜像 */
  JackalRam.SpriteVertScreenPosition[x] = 0;
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  wx = (uint16_t)((uint8_t)((JackalRam.RNG_INCEveryFrame & 0x7Fu) + 0x40u) +
                  JackalRam.ScreenLeftScrollPosition);
  JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wx;
  JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wx >> 8);
  subCalculateObjectSpeed(x, 0x08, 0x50);            /* 向下 */
  JackalRam.Raw[0x6A + x] = 0x80;                    /* :7492-7493 V 镜像 */
  subMoveSpriteToPreviousState(x);
}

void LandedHeliSpriteLogic(uint8_t x) {  /* :7307-7317（dw 6 项） */
  static void (*const tbl[6])(uint8_t) = {
    LandedHeliState0, LandedHeliState1, LandedHeliState2,
    LandedHeliState3, LandedHeliState4, LandedHeliState5,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}

/* ---------------------------------------------------------------- ParkedJeepTank（:4104-4157） */

static void ParkedJeepTankState0(uint8_t x) {  /* :4116-4126 */
  subUpdateSpritePositionForScrolling(x);
  JackalRam.SpriteData1[x] = 0x1C;                   /* 吉普 BG（默认） */
  if (JackalRam.CurrentLevel != 2u) {
    JackalRam.SpriteData1[x] = 0x18;                 /* 坦克 BG（非 L3） */
  }
  subMoveSpriteToNextState(x);
}

static void ParkedJeepTankState3(uint8_t x) {  /* :4131-4157 */
  JackalRam.SpriteData8[x]--;
  if (JackalRam.SpriteData8[x] != 0 && (JackalRam.SpriteData8[x] & 7u) == 0) {
    JackalRam.SpriteTypeIndex[x]++;
  }
  subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(x);
  subUpdateBGGraphicsFromSprite_Every7thRNGFrame(x, JackalRam.SpriteData1[x]);
  if (JackalRam.SpriteData8[x] != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[x] = 0;
  if (JackalRam.SpriteObjectID[x] == 0xD3u) {        /* :4147-4156 星下藏车 → 出满武器星 */
    JackalSpawnZp[0x0F] = 0;
    JackalSpawnZp[0x08] = 0xD1;
    JackalSpawnZp[0x09] = 0xD1;
    JackalSpawnZp[0x00] = 0;
    JackalSpawnZp[0x01] = 0;
    JackalSpawnZp[0x02] = 0;
    JackalSpawnZp[0x03] = 0;
    JackalSpawnZp[0x35] = x;
    if (subSpawnObjectFromParent() == 0xFFu) {
      JackalRam.SpriteData8[x]++;
      return;
    }
  }
  subMoveSpriteToNextState(x);
}

void ParkedJeepTankSpriteLogic(uint8_t x) {  /* :4104-4114（dw 5 项） */
  static void (*const tbl[5])(uint8_t) = {
    ParkedJeepTankState0,
    subUpdateSpritePositionForScrolling_Speed_CheckForDespawn,
    subPlayExplosionSound_LoadNewBGGraphics,
    ParkedJeepTankState3,
    subScrollSprite_CheckForDespawn_UpdateBG,
  };
  tbl[JackalRam.SpriteState[x] & 0x7Fu](x);
}
