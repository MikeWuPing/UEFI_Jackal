/* core/sprite.c：Bank1.ASM:8-286 逐行翻译（subProcessSpriteUpdates + subDrawObjectSprites）。
   零页 scratch 归约为局部变量：$00=GraphicsAttributes、$01/$02=基 Y/X、$03=tile 计数、
   $04=OAM 字节偏移、$05=槽游标、$08/$09=header 地址/分数 scratch、$0A/$0B=计数、
   $0C=分数 Y 坐标、$10=SpriteAttributes（镜像位）、$17=OAM 余量。
   SpriteAttributes 是 $006A 起 0x20 项数组（RAM_Symbols.ASM:122 起；Jeep1Attributes=$007A 等
   为其下标别名），ram.h 按命名字段建模，本文件统一经 Raw[0x6A+slot] 访问。
   PLA PLA RTS 语义（:275-277）：第 64 tile 写满当轮整个 subProcessSpriteUpdates 立即返回——
   不画分数、不填 $F4；C 侧以 draw 返回 0 表达。 */
#include "ram.h"
#include "bank.h"
#include "sprite.h"
#include "jackal_assets.h"

/* tblScoreDisplayCoordinates（:208-212）：P1 X/Y、P2 X/Y */
static const uint8_t tblScoreDisplayCoordinates[4] = { 0x10, 0xC8, 0xA0, 0xC8 };

/* subDrawObjectSprites（:214-286）：单对象元精灵展开。
   返回新的 OAM 偏移（STX $04 语义）；OAM 写满 64 时返回 0 表示当轮中止（PLA PLA RTS）。 */
static uint8_t subDrawObjectSprites(uint8_t x, uint16_t hdr, uint8_t sprAttr10,
                                    uint8_t gfxAttr00, uint8_t baseY, uint8_t baseX,
                                    uint8_t *oamLeft) {
  const uint8_t *meta = BankPtr(1, hdr);   /* header 指向 Bank1 内元精灵 */
  uint8_t count = meta[0];                 /* ($08),Y：tile 数，0 → RTS */
  uint8_t i;
  if (count == 0) {
    return x;
  }
  for (i = 0; i < count; i++) {
    const uint8_t *tile = meta + 1u + (uint16_t)i * 4u;  /* {Y,tile,attr,X}×N */
    uint8_t off, adj;
    uint16_t sum;
    /* Y 偏移：$10 bit7=V 镜像 → -b-$10（:222-229 EOR#$FF+1-$10） */
    off = tile[0];
    adj = (sprAttr10 & 0x80u) != 0 ? (uint8_t)(0u - off - 0x10u) : off;
    /* :231-242 可见性：负偏移需进位（+base 不 wrap），正偏移需无进位（不溢底） */
    sum = (uint16_t)baseY + adj;
    if ((adj & 0x80u) != 0) {
      if (sum < 0x100u) { continue; }      /* BCC + ：越顶，整 tile 不写 */
    } else if (sum >= 0x100u) {
      continue;                            /* 越底，整 tile 不写 */
    }
    JackalRam.OamShadow[x] = (uint8_t)sum;                        /* OAMDMA_Y */
    JackalRam.OamShadow[(uint8_t)(x + 1u)] = tile[1];             /* tile index */
    JackalRam.OamShadow[(uint8_t)(x + 2u)] = (uint8_t)((tile[2] ^ sprAttr10) | gfxAttr00);
    /* X 偏移：$10 bit6=H 镜像 → -b-8（:254-262） */
    off = tile[3];
    adj = (sprAttr10 & 0x40u) != 0 ? (uint8_t)(0u - off - 8u) : off;
    sum = (uint16_t)baseX + adj;
    if ((adj & 0x80u) != 0) {
      if (sum < 0x100u) { continue; }      /* BCC ++++：越左；Y/tile/attr 已落槽，不耗槽 */
    } else if (sum >= 0x100u) {
      continue;                            /* BCS ++++：越右，同上 */
    }
    JackalRam.OamShadow[(uint8_t)(x + 3u)] = (uint8_t)sum;        /* :272 */
    (*oamLeft)--;                            /* :273 DEC $17 */
    if (*oamLeft == 0) {
      return 0;                            /* PLA PLA RTS：当轮中止 */
    }
    x = (uint8_t)(x + 0xC4u);              /* :278-281 旋转步进 */
  }
  return x;                                /* :285 STX $04 */
}

void subProcessSpriteUpdates(void) {  /* :8-205 */
  uint8_t oamLeft = 0x40;            /* $17 */
  uint8_t x;                         /* $04 */
  int8_t slot;                       /* $05 */
  JackalRam.SpriteSlotRotation = (uint8_t)(JackalRam.SpriteSlotRotation + 0x44u);
  x = JackalRam.SpriteSlotRotation;
  for (slot = 0x1F; slot >= 0; slot--) {
    uint8_t type = JackalRam.SpriteTypeIndex[slot];
    uint8_t y2;
    uint16_t hdr;
    const uint8_t *tbl;
    if (type == 0) { continue; }                       /* :24 BEQ +++ */
    if ((JackalRam.SpriteState[slot] & 0x80u) != 0) { continue; }  /* :26 BMI +++ */
    y2 = (uint8_t)(type << 1);                         /* ASL：*2，进位=ID≥$80 */
    tbl = (type < 0x80u) ? jackal_tblSpriteConstructionHeaderAddress
                         : jackal_tblSpriteConstructionHeaderAddress2;
    hdr = (uint16_t)(tbl[y2] | ((uint16_t)tbl[(uint8_t)(y2 + 1u)] << 8));
    x = subDrawObjectSprites(x, hdr,
                             JackalRam.Raw[0x6Au + (uint8_t)slot],   /* $10：SpriteAttributes,X */
                             JackalRam.SpriteGraphicsAttributes[slot], /* $00 */
                             JackalRam.SpriteVertScreenPosition[slot], /* $01 */
                             JackalRam.SpriteHorizScreenPosition[slot],/* $02 */
                             &oamLeft);
    if (oamLeft == 0) { return; }          /* PLA PLA RTS 中止 */
  }
  /* 分数显示（:53-193）：仅 GCS==5 && GPM==3 */
  if (JackalRam.GameControlState == 5u && JackalRam.GamePlayMode == 3u) {
    uint8_t player;                        /* $08：0=P1/1=P2 */
    uint8_t xCoord, yCoord;                /* $09/$0C */
    uint8_t scoreTiles[8];                 /* $D8-$DF（与 JeepMovementDirection 复用同地址，本例程 scratch） */
    const uint8_t *score;
    uint8_t i, n;
    if (JackalRam.Player2Active == 0) {
      player = 0;
    } else {
      /* :67-71：RNG&$80 经 CLC ROL ROL → bit7 落到 bit0（每 $80 帧交替） */
      player = (JackalRam.RNG_INCEveryFrame & 0x80u) != 0 ? 1u : 0u;
    }
    xCoord = tblScoreDisplayCoordinates[player * 2u];
    yCoord = tblScoreDisplayCoordinates[player * 2u + 1u];
    score = player != 0 ? JackalRam.Jeep2Score : JackalRam.Jeep1Score;
    n = 0;
    for (i = 0; i < 3; i++) {              /* 24-bit BCD：低半字节先存，Y 倒序消费即 MSB 先显示 */
      scoreTiles[n++] = (uint8_t)((score[i] & 0x0Fu) * 2u + 2u);
      scoreTiles[n++] = (uint8_t)((score[i] >> 3) + 2u);   /* (b&$F0)>>3=hi*2 */
    }
    scoreTiles[6] = 0x20;                  /* 'P' */
    scoreTiles[7] = player == 0 ? 0x04u : 0x06u;  /* '1'/'2' */
    for (i = 0; i < 8; i++) {              /* :116-144（$0A=8..1，gap 于 $0A==7 即第二 tile 后） */
      JackalRam.OamShadow[x] = yCoord;
      JackalRam.OamShadow[(uint8_t)(x + 1u)] = scoreTiles[7u - i];
      JackalRam.OamShadow[(uint8_t)(x + 2u)] = 0;
      JackalRam.OamShadow[(uint8_t)(x + 3u)] = xCoord;
      xCoord = (uint8_t)(xCoord + 8u);
      if (i == 1) { xCoord = (uint8_t)(xCoord + 8u); }
      x = (uint8_t)(x + 0xC4u);
      oamLeft--;
      if (oamLeft == 0) { return; }        /* :205 RTS */
    }
    if ((player != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount) != 0) {
      uint8_t lives = player != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount;
      xCoord = (uint8_t)(xCoord - 0x2Cu);  /* :148-151 */
      yCoord = (uint8_t)(yCoord + 0x10u);  /* 两行下 */
      JackalRam.OamShadow[x] = yCoord;                 /* 生命 icon */
      JackalRam.OamShadow[(uint8_t)(x + 1u)] = 0x20;
      JackalRam.OamShadow[(uint8_t)(x + 2u)] = player; /* attr=player（调色板） */
      JackalRam.OamShadow[(uint8_t)(x + 3u)] = xCoord;
      x = (uint8_t)(x + 0xC4u);
      oamLeft--;
      if (oamLeft == 0) { return; }
      JackalRam.OamShadow[x] = yCoord;                 /* :172 命数（JeepLifeCount-1） */
      JackalRam.OamShadow[(uint8_t)(x + 1u)] = (uint8_t)(((lives - 1u) & 0x0Fu) * 2u + 2u);
      JackalRam.OamShadow[(uint8_t)(x + 2u)] = 0;
      JackalRam.OamShadow[(uint8_t)(x + 3u)] = (uint8_t)(xCoord + 0x0Au);
      x = (uint8_t)(x + 0xC4u);
      oamLeft--;
      if (oamLeft == 0) { return; }
    }
  }
  /* No_More_Sprites（:196-204）：余槽 Y=$F4（仅写 Y 字节） */
  while (oamLeft != 0) {
    JackalRam.OamShadow[x] = 0xF4;
    x = (uint8_t)(x + 0xC4u);
    oamLeft--;
  }
}
