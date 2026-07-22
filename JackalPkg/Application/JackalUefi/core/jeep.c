/* core/jeep.c：吉普控制（Bank7.ASM:4440-4581 BG 碰撞、:5364-5780 吉普状态机移动段）。
   零页 scratch 归约：$0F（X 保存）/$11/$12/$14/$16/$17 经 JackalRam.Raw 镜像访问
   （真零页，Phase 4 敌人 AI 同此约定）；JeepMovementDirection=$00D8 即 Label1186 的
   tendency*4 暂存（RAM_Symbols.ASM:250 自注"Also used for displaying score/xp"——
   sprite.c 分数段以局部数组复用同址，不写字段）。
   常量（JeepAttributes.ASM）：速度 UB=±1/LB=0（1px/帧）、水中 LB=$80（0.5px/帧）、
   JeepIFrameValueAfterDeath=$A0、POWDropoffDelayInit=$20/Next=$10。 */
#include "ram.h"
#include "bank.h"
#include "jeep.h"
#include "spawn.h"
#include "weapon.h"
#include "sound_stub.h"


#define POW_LOADING_INTO_HELI_ID   0xA7u  /* :5426 */
#define WEAPON_UPGRADE_SOUND_CLIP  0x4Eu  /* WeaponUpgradeSoundClip（Sound.ASM，stub 仅调用点） */
#define WATER_SPLASH_SOUND_CLIP    0x47u  /* WaterSplashSoundClip（:5559 区域，stub 仅调用点） */
#define JEEP_DEATH_CLIP            0x25u  /* JeepDeathSoundClip（:5830，stub 仅调用点） */

/* tblJeepLevelSpawnLocation（:5247-5251）：P1X/P2X/P1Y/P2Y */
static const uint8_t tblJeepLevelSpawnLocation[4] = { 0x30, 0xD0, 0xE0, 0xE0 };

/* tblJeepPalette（:5391-5393） */
static const uint8_t tblJeepPalette[2] = { 0x00, 0x01 };
/* tblJeepTendencyDirectionValue_DPadPressed（:5395-5411）：1-8 顺时针（1=Up…7=Left），0=无效 */
static const uint8_t tblJeepTendencyDirectionValue_DPadPressed[16] = {
  0x01, 0x03, 0x07, 0x00, 0x05, 0x04, 0x06, 0x00,
  0x01, 0x02, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
};
/* table11（:5518-5526）：方向 1-8 每方向 2 个前缘探测点 (x1,y1,x2,y2) */
static const int8_t table11[32] = {
  -4, -8,  4, -8,   /* Up */
   0, -6,  6,  0,   /* Up-Right */
   8, -4,  8,  4,   /* Right */
   6,  0,  0,  6,   /* Down-Right */
  -4,  8,  4,  8,   /* Down */
  -6,  0,  0,  6,   /* Down-Left */
  -8, -4, -8,  4,   /* Left */
  -6,  0,  0, -6,   /* Up-Left */
};
/* tblSprite_BG_CollisionMask（:4577-4581）：(x&$1F)>>3 选域 */
static const uint8_t tblSprite_BG_CollisionMask[4] = { 0xC0, 0x30, 0x0C, 0x03 };
/* tblJeepSpriteDirectionalDrawing（:5878-5909）：方向 1-8 → {TypeIndex, Attributes} */
static const uint8_t tblJeepSpriteDirectionalDrawing[16] = {
  0x01, 0x00,   /* Up */
  0x03, 0x00,   /* Up-Right */
  0x05, 0x00,   /* Right */
  0x03, 0x80,   /* Down-Right */
  0x01, 0x80,   /* Down */
  0x03, 0xC0,   /* Down-Left */
  0x05, 0x40,   /* Left */
  0x03, 0x40,   /* Up-Left */
};

/* ---------------------------------------------------------------- BG 碰撞查询（:4440-4581） */

/* 公共段（:4477-4550）：x=槽位（已含 $39 偏移语义），返回 2-bit 碰撞类型 */
static uint8_t collisionCore(uint8_t x) {
  uint16_t page = 0x0300u;              /* $08/$09 地址对：$03xx 起 */
  uint16_t acc;
  uint8_t wideX, lowY, a, yIdx, packed, shift, v;
  /* :4482-4493 横向：SubPixel+LB（进位）→ Pos ADC UB（带 LB 进位）→ CLC ADC $16 →
     CLC ADC Scroll——每次 CLC 隔断进位链，页选择只看末次 ADC 的进位！ */
  acc = (uint16_t)JackalRam.SpriteHorizScreenPositionSubPixel[x] +
        JackalRam.SpriteHorizSpeedLB[x];
  acc = (uint16_t)JackalRam.SpriteHorizScreenPosition[x] +
        JackalRam.SpriteHorizSpeedUB[x] + (acc >> 8);
  a = (uint8_t)acc;
  acc = (uint16_t)a + JackalRam.Raw[0x16];             /* CLC ADC $16 */
  a = (uint8_t)acc;
  acc = (uint16_t)a + JackalRam.ScreenLeftScrollPosition;  /* CLC ADC Scroll */
  wideX = (uint8_t)acc;
  if (acc >= 0x100u) { page = 0x0400u; }   /* 末次进位 → INC $09（:4493） */
  /* :4494-4501 纵向：SubPixel+LB（进位）→ Pos+UB */
  lowY = (uint8_t)(JackalRam.SpriteVertScreenPositionSubPixel[x] +
                   JackalRam.SpriteVertSpeedLB[x]);
  a = (uint8_t)(JackalRam.SpriteVertScreenPosition[x] +
                JackalRam.SpriteVertSpeedUB[x] +
                (lowY < JackalRam.SpriteVertSpeedLB[x] ? 1u : 0u));
  /* :4502-4511（- 公共段）：SEC SBC $0A；+$17；&$F8；+$4F；+8 */
  a = (uint8_t)(a - (uint8_t)(JackalRam.CurrentLevelScreenSubPosition & 7u));
  a = (uint8_t)(a + JackalRam.Raw[0x17]);
  a = (uint8_t)(a & 0xF8u);
  a = (uint8_t)(a + JackalRam.Zp4F);
  a = (uint8_t)(a + 8u);
  /* :4512-4523：Y=((x&$E0)>>5)+a 索引碰撞字节 */
  yIdx = (uint8_t)(((wideX & 0xE0u) >> 5) + a);
  packed = JackalRam.Raw[page + yIdx];
  /* :4524-4550：(x&$1F)>>3 选掩码域，再按域右移（Y=0→>>6、1→>>4、2→>>2、3→LSR ROL 不变） */
  shift = (uint8_t)((wideX & 0x1Fu) >> 3);
  v = (uint8_t)(packed & tblSprite_BG_CollisionMask[shift]);
  return (uint8_t)(v >> ((3u - shift) * 2u));
}

uint8_t fctGetCollisionTypeFromRAM300_4FF(uint8_t x) {  /* :4470：X' = X + $39 */
  JackalRam.Raw[0x0F] = x;
  return collisionCore((uint8_t)(x + JackalRam.Raw[0x39]));
}

uint8_t Label1210(uint8_t x) {  /* :4462：$16/$17=0、X' = $39（不叠加调用方 X） */
  JackalRam.Raw[0x16] = 0;
  JackalRam.Raw[0x17] = 0;
  JackalRam.Raw[0x0F] = x;
  return collisionCore(JackalRam.Raw[0x39]);
}

uint8_t fctTestForMovementCollision(uint8_t x) {  /* :4552-4574 */
  uint16_t page;
  uint8_t a, yIdx, packed, shift, v;
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] != 0) {
    return 0;                            /* :4573 ++：屏外无碰撞 */
  }
  JackalRam.Raw[0x0F] = x;
  JackalRam.Raw[0x16] = 0;               /* :4559-4560 */
  JackalRam.Raw[0x17] = 0;
  page = JackalRam.Raw[0x12] != 0 ? 0x0400u : 0x0300u;   /* :4561-4565：$12 选页 */
  a = JackalRam.Raw[0x14];               /* :4571 LDA $14，JMP -（$17=0，ADC 无效省略） */
  a = (uint8_t)(a - (uint8_t)(JackalRam.CurrentLevelScreenSubPosition & 7u));
  a = (uint8_t)(a & 0xF8u);
  a = (uint8_t)(a + JackalRam.Zp4F);
  a = (uint8_t)(a + 8u);
  yIdx = (uint8_t)(((JackalRam.Raw[0x11] & 0xE0u) >> 5) + a);
  packed = JackalRam.Raw[page + yIdx];
  shift = (uint8_t)((JackalRam.Raw[0x11] & 0x1Fu) >> 3);
  v = (uint8_t)(packed & tblSprite_BG_CollisionMask[shift]);
  return (uint8_t)(v >> ((3u - shift) * 2u));
}

/* ---------------------------------------------------------------- 吉普状态机（:5364-5780） */

static void subSetJeepSpeedToZero(uint8_t x) {  /* :5509 */
  uint8_t s = (uint8_t)(0x10 + x);
  JackalRam.SpriteVertSpeedUB[s] = 0;
  JackalRam.SpriteVertSpeedLB[s] = 0;
  JackalRam.SpriteHorizSpeedUB[s] = 0;
  JackalRam.SpriteHorizSpeedLB[s] = 0;
}
static void subSetJeepVerticalSpeedToZero_Escalator(uint8_t x) {  /* :5512：名不符实——清横向 */
  uint8_t s = (uint8_t)(0x10 + x);
  JackalRam.SpriteHorizSpeedUB[s] = 0;
  JackalRam.SpriteHorizSpeedLB[s] = 0;
}

static void JeepDeadState(uint8_t x) {  /* :5374-5389（重生初始化） */
  JackalRam.JeepIFrameTimer[x] = 0xA0;       /* JeepIFrameValueAfterDeath */
  JackalRam.JeepFacingDirection[x] = 1;      /* 朝向上 */
  JackalRam.JeepPOWDropoffDelay[x] = 0x10;   /* JeepPOWDropoffDelayNextValue */
  JackalRam.SpriteTypeIndex[0x10 + x] = 1;
  JackalRam.Raw[0x7A + x] = 0;                /* JeepAttributes,X（$007A 起） */
  JackalRam.SpriteGraphicsAttributes[0x10 + x] = tblJeepPalette[x];
  JackalRam.SpriteState[0x10 + x]++;         /* → Normal */
}

/* JeepNormalState 的 POW 卸载段（:5417-5459） */
static void jeepPowDropoff(uint8_t x) {
  uint8_t a;
  if (JackalRam.JeepAtHelipadDropoff[x] == 0 || JackalRam.JeepPOWCount[x] == 0) {
    return;                                  /* ++：非卸载中 */
  }
  JackalRam.JeepPOWDropoffDelay[x]--;
  if (JackalRam.JeepPOWDropoffDelay[x] != 0) {
    return;                                  /* ++ */
  }
  /* :5426-5432：spawn POW Loading Into Heli（NoOffset：$00-$03 清 0） */
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x08] = POW_LOADING_INTO_HELI_ID;
  JackalSpawnZp[0x09] = x;
  JackalSpawnZp[0x35] = x;
  if (subSpawnObjectFromParent() != 0xFFu) {
    JackalRam.JeepPOWCount[x]--;
    JackalRam.JeepPOWHeliDropOffCount[x]++;
    /* :5436-5450：BCD 调整（低半字节≥$0A → +6），命中 $04/$09/$15 → 武器升级 */
    a = JackalRam.JeepPOWHeliDropOffCount[x];
    if ((uint8_t)(a & 0x0Fu) >= 0x0Au) {
      a = (uint8_t)(a + 6u);
      JackalRam.JeepPOWHeliDropOffCount[x] = a;
    } else {
      a = (uint8_t)(a & 0x0Fu);
    }
    if ((a == 0x04u || a == 0x09u || a == 0x15u) && JackalRam.JeepMainWeapon[x] < 3u) {
      JackalRam.JeepMainWeapon[x]++;
      subInitiateSoundClip(WEAPON_UPGRADE_SOUND_CLIP);  /* stub：调用点保留 */
    }
  }
  JackalRam.JeepPOWDropoffDelay[x] = 0x20;   /* :5458 +++：JeepPOWDropoffDelayInitValue */
}

/* JeepNormalState 的 IFrame 闪烁段（:5460-5472） */
static void jeepIFrameFlash(uint8_t x) {
  if (JackalRam.JeepIFrameTimer[x] == 0) {
    return;
  }
  JackalRam.JeepIFrameTimer[x]--;
  if (JackalRam.JeepIFrameTimer[x] == 0) {
    JackalRam.SpriteGraphicsAttributes[0x10 + x] = tblJeepPalette[x];  /* 恢复默认 palette */
    return;
  }
  JackalRam.SpriteGraphicsAttributes[0x10 + x]++;   /* ++：闪烁 */
  if (JackalRam.SpriteGraphicsAttributes[0x10 + x] == 4u) {
    JackalRam.SpriteGraphicsAttributes[0x10 + x] = 0;  /* wrap 0-3 */
  }
}

/* Label1186（:5528-5637）：碰撞查询 + 水域半速 + 定点积分 + 电扶梯/颠簸 */
static void Label1186(uint8_t x) {
  uint8_t s = (uint8_t)(0x10 + x);
  uint8_t blocked = 0;
  uint8_t collision;
  /* :5529-5537：JeepMovementDirection = (tendency-1)*4（table11 索引） */
  JackalRam.JeepMovementDirection = (uint8_t)((JackalRam.JeepDirectionTendency[x] - 1u) * 4u);
  JackalRam.JeepMovementState = 0;          /* Moving */
  if (JackalRam.CurrentLevel == 3u) {       /* Level 4 水域（:5539-5542） */
    JackalRam.JeepMovementState = 1;        /* Water */
    JackalRam.Raw[0x16] = 0;
    JackalRam.Raw[0x17] = 0;
    collision = fctGetCollisionTypeFromRAM300_4FF(x);
    if (collision == 3u) {                  /* 最后碰撞组=水：水花帧（:5549-5560） */
      if (JackalRam.SpriteState[s] < 2u) {
        if ((JackalRam.RNG_INCEveryFrame & 7u) == 0) {
          JackalRam.SpriteTypeIndex[s]++;   /* 水花帧（TypeIndex 奇偶切换由渲染表镜像） */
        }
        if ((JackalRam.RNG_INCEveryFrame & 0x0Fu) == 0) {
          subInitiateSoundClip(WATER_SPLASH_SOUND_CLIP);  /* stub */
        }
      }
    }
    /* :5561-5574 半速：UB 负 → LB=$80（保持 UB）；正 → LSR UB（原 bit0 进 C）ROR LB */
    if ((int8_t)JackalRam.SpriteHorizSpeedUB[s] < 0) {
      JackalRam.SpriteHorizSpeedLB[s] = 0x80;
    } else {
      uint8_t ub0 = JackalRam.SpriteHorizSpeedUB[s];
      JackalRam.SpriteHorizSpeedUB[s] = (uint8_t)(ub0 >> 1);
      JackalRam.SpriteHorizSpeedLB[s] = (uint8_t)((JackalRam.SpriteHorizSpeedLB[s] >> 1) |
                                                  ((ub0 & 1u) << 7));
    }
    if ((int8_t)JackalRam.SpriteVertSpeedUB[s] < 0) {
      JackalRam.SpriteVertSpeedLB[s] = 0x80;
    } else {
      uint8_t ub0 = JackalRam.SpriteVertSpeedUB[s];
      JackalRam.SpriteVertSpeedUB[s] >>= 1;
      JackalRam.SpriteVertSpeedLB[s] = (uint8_t)((JackalRam.SpriteVertSpeedLB[s] >> 1) |
                                                 ((ub0 & 1u) << 7));
    }
  }
  /* :5575-5596：前缘双探测点（table11）；可通行条件=碰撞==0 或（Water 态且==3） */
  JackalRam.Raw[0x16] = (uint8_t)table11[JackalRam.JeepMovementDirection];
  JackalRam.Raw[0x17] = (uint8_t)table11[JackalRam.JeepMovementDirection + 1u];
  collision = fctGetCollisionTypeFromRAM300_4FF(x);
  if (collision != 0u &&
      (JackalRam.JeepMovementState == 0u || collision != 3u)) {
    blocked = 1;
  } else {
    JackalRam.Raw[0x16] = (uint8_t)table11[JackalRam.JeepMovementDirection + 2u];
    JackalRam.Raw[0x17] = (uint8_t)table11[JackalRam.JeepMovementDirection + 3u];
    collision = fctGetCollisionTypeFromRAM300_4FF(x);
    if (collision != 0u &&
        (JackalRam.JeepMovementState == 0u || collision != 3u)) {
      blocked = 1;
    }
  }
  if (blocked) {                            /* :5597-5601 */
    if (JackalRam.JeepEscalatorEffectActive[x] != 0) {
      subSetJeepVerticalSpeedToZero_Escalator(x);
    } else {
      subSetJeepSpeedToZero(x);
    }
  }
  /* :5602-5637：移动/电扶梯分支 */
  if (JackalRam.JeepEscalatorEffectActive[x] != 0) {   /* ++ :5630 */
    if (JackalRam.SpriteVertSpeedUB[s] == 0 || (int8_t)JackalRam.SpriteVertSpeedUB[s] < 0) {
      JackalRam.SpriteVertSpeedLB[s] = 0x80;  /* :5633-5637：0.5px/帧 下沉（电扶梯） */
    }
  }
  /* -- :5604-5628：定点积分链（SubPixel+LB → Position+UB ADC 进位），双写屏幕/逻辑位置 */
  {
    uint16_t acc;
    acc = (uint16_t)JackalRam.SpriteHorizScreenPositionSubPixel[s] +
          JackalRam.SpriteHorizSpeedLB[s];
    JackalRam.SpriteHorizScreenPositionSubPixel[s] = (uint8_t)acc;
    acc = (uint16_t)JackalRam.JeepHorizPosition[x] +
          JackalRam.SpriteHorizSpeedUB[s] + (acc >> 8);
    JackalRam.SpriteHorizScreenPosition[s] = (uint8_t)acc;
    JackalRam.JeepHorizPosition[x] = (uint8_t)acc;
    acc = (uint16_t)JackalRam.SpriteVertScreenPositionSubPixel[s] +
          JackalRam.SpriteVertSpeedLB[s];
    JackalRam.SpriteVertScreenPositionSubPixel[s] = (uint8_t)acc;
    acc = (uint16_t)JackalRam.JeepVertPosition[x] +
          JackalRam.SpriteVertSpeedUB[s] + (acc >> 8);
    JackalRam.SpriteVertScreenPosition[s] = (uint8_t)acc;
    JackalRam.JeepVertPosition[x] = (uint8_t)acc;
  }
  /* :5622-5628：JeepState<2 且 RNG&4==0 → 显示 Y+1（行驶颠簸视觉，只写屏幕坐标） */
  if (JackalRam.SpriteState[s] < 2u && (JackalRam.RNG_INCEveryFrame & 4u) == 0) {
    JackalRam.SpriteVertScreenPosition[s]++;
  }
}

/* subRotateJeepTowardsDirectionOfMotion（:5735-5780）：RNG&3 门控 ±1 环形最短趋近 */
static void subRotateJeepTowardsDirectionOfMotion(uint8_t x) {
  int8_t diff;
  uint8_t y;
  if ((JackalRam.RNG_INCEveryFrame & 3u) != 0) {
    return;                                 /* ++++++ RTS */
  }
  diff = (int8_t)(JackalRam.JeepFacingDirection[x] - JackalRam.JeepDirectionTendency[x]);
  if (diff != 0) {
    uint8_t absDiff = diff < 0 ? (uint8_t)(0 - diff) : (uint8_t)diff;
    if (diff > 0) {                         /* facing > tendency */
      if (absDiff >= 4u) {
        JackalRam.JeepFacingDirection[x]++; /* +++++ INC（:5760） */
      } else {
        JackalRam.JeepFacingDirection[x]--; /* ++++ DEC（:5766） */
      }
    } else {                                /* facing < tendency */
      if (absDiff >= 4u) {
        JackalRam.JeepFacingDirection[x]--; /* ++++ DEC */
      } else {
        JackalRam.JeepFacingDirection[x]++; /* +++++ INC */
      }
    }
    if (JackalRam.JeepFacingDirection[x] >= 9u) {
      JackalRam.JeepFacingDirection[x] = 1; /* wrap 9→1 */
    } else if (JackalRam.JeepFacingDirection[x] == 0) {
      JackalRam.JeepFacingDirection[x] = 8; /* wrap 0→8 */
    }
  }
  y = (uint8_t)((JackalRam.JeepFacingDirection[x] - 1u) * 2u);   /* :5771-5775 */
  JackalRam.SpriteTypeIndex[0x10 + x] = tblJeepSpriteDirectionalDrawing[y];
  JackalRam.Raw[0x7A + x] = tblJeepSpriteDirectionalDrawing[y + 1u];  /* JeepAttributes,X */
}

/* 8 方向处理器（:5638-5730）：subRotate + VisibleFrameTimer + 速度 UB → Label1186 */
static void jeepDirectionHandler(uint8_t x, uint8_t visTimer, int8_t hSpeed, int8_t vSpeed) {
  uint8_t s = (uint8_t)(0x10 + x);
  subRotateJeepTowardsDirectionOfMotion(x);
  JackalRam.JeepVisibleFrameTimer[x] = visTimer;   /* Label995 双人协同的方向象限编码 */
  JackalRam.SpriteHorizSpeedUB[s] = (uint8_t)hSpeed;
  JackalRam.SpriteVertSpeedUB[s] = (uint8_t)vSpeed;
  Label1186(x);
}

/* NoDPad_Invalid（:5638-5668）：tendency=facing；电扶梯下沉；静止水花（Level 4） */
static void NoDPad_Invalid(uint8_t x) {
  uint8_t s = (uint8_t)(0x10 + x);
  JackalRam.JeepDirectionTendency[x] = JackalRam.JeepFacingDirection[x];
  if (JackalRam.JeepEscalatorEffectActive[x] != 0) {
    if ((JackalRam.RNG_INCEveryFrame & 1u) != 0) {   /* LSR; BCS + ：bit0=1 */
      JackalRam.SpriteVertScreenPosition[s]++;
      JackalRam.JeepVertPosition[x]++;
    }
    return;
  }
  /* :5649-5668：Level 4 静止水花（碰撞==3 时 RNG&7==0 且 TypeIndex 偶 → ++） */
  if (JackalRam.CurrentLevel != 3u) {
    return;
  }
  JackalRam.Raw[0x16] = 0;
  JackalRam.Raw[0x17] = 0;
  if (fctGetCollisionTypeFromRAM300_4FF(x) != 3u) {
    return;
  }
  if ((JackalRam.RNG_INCEveryFrame & 7u) != 0) {
    return;
  }
  /* :5662-5667：LSR 判断的是 INC 前的原值——原值奇（已是水花帧）保持 INC（切回正常），
     原值偶 DEC DEC（=原值-1……实为偶↔奇帧切换；吉普帧对 1/2、3/4、5/6 奇=水花） */
  {
    uint8_t before = JackalRam.SpriteTypeIndex[s];
    JackalRam.SpriteTypeIndex[s]++;
    if ((before & 1u) == 0) {
      JackalRam.SpriteTypeIndex[s]--;
      JackalRam.SpriteTypeIndex[s]--;
    }
  }
}

static void JeepNormalState(uint8_t x) {  /* :5413-5508 */
  uint8_t s = (uint8_t)(0x10 + x);
  uint8_t dpad, tend;
  jeepPowDropoff(x);
  jeepIFrameFlash(x);
  /* :5473-5477 */
  subSetJeepSpeedToZero(x);
  JackalRam.SpriteHorizScreenPosition[s] = JackalRam.JeepHorizPosition[x];
  JackalRam.SpriteVertScreenPosition[s] = JackalRam.JeepVertPosition[x];
  dpad = (uint8_t)(JackalRam.JeepControlsInput[x] & 0x0Fu);   /* :5478-5480 */
  if (dpad != 0) {
    tend = tblJeepTendencyDirectionValue_DPadPressed[dpad];
    JackalRam.JeepDirectionTendency[x] = tend;
    if (tend == JackalRam.JeepFacingDirection[x]) {
      JackalRam.JeepFacingDirection[x] = tend;   /* ++：同向（:5487 两 STA 等价） */
    }
  }
  /* :5491 分派（函数指针表=ASM dw 顺序；对角=同帧先后设两轴速度） */
  switch (dpad) {
    case 0x01: jeepDirectionHandler(x, 0,  1,  0); break;   /* Right */
    case 0x02: jeepDirectionHandler(x, 2, -1,  0); break;   /* Left */
    case 0x04: jeepDirectionHandler(x, 1,  0,  1); break;   /* Down（timer=JeepSpeedDown=1） */
    case 0x05: jeepDirectionHandler(x, 1,  1,  1); break;   /* DownRight */
    case 0x06: jeepDirectionHandler(x, 3, -1,  1); break;   /* DownLeft */
    case 0x08: jeepDirectionHandler(x, 0,  0, -1); break;   /* Up */
    case 0x09: jeepDirectionHandler(x, 0,  1, -1); break;   /* UpRight */
    case 0x0A: jeepDirectionHandler(x, 2, -1, -1); break;   /* UpLeft */
    default:   NoDPad_Invalid(x); break;                    /* 0 与同帧对撞（:5492-5507） */
  }
}

/* 死亡三态（:5809-5876）+ Label1202（:5806-5807：INC state、JMP Label1186） */
static void Label1202(uint8_t x) {  /* :5806 */
  JackalRam.SpriteState[0x10 + x]++;
  Label1186(x);
}

static void JeepCollisionState(uint8_t x) {  /* :5809-5849 */
  uint8_t wsum;
  JackalRam.JeepMainWeapon[x] = 0;                     /* 武器归零 */
  wsum = (uint8_t)(JackalRam.JeepMainWeapon[0] + JackalRam.JeepMainWeapon[1]);
  JackalRam.DifficultyBasedOnWeapon = wsum >= 3u ? 3u : wsum;   /* :5813-5819 难度重算 */
  if ((uint8_t)(JackalRam.Jeep1LifeCount + JackalRam.Jeep2LifeCount) == 1u) {
    subStopMusic();   /* :5829 stub：仅 1 吉普 1 命——本死即 Game Over（原版此处
                          有手雷得分 1UP 救回的 glitch，ASM 注释在案） */
  }
  subInitiateSoundClip(JEEP_DEATH_CLIP);               /* stub */
  subSetJeepSpeedToZero(x);
  /* :5833-5840：携带 POW 落地（Walking POW，数量=POWCount） */
  JackalSpawnZp[0x00] = 0;
  JackalSpawnZp[0x01] = 0;
  JackalSpawnZp[0x02] = 0;
  JackalSpawnZp[0x03] = 0;
  JackalSpawnZp[0x08] = 0xA8;                          /* Walking POW object ID */
  JackalSpawnZp[0x09] = x;
  JackalSpawnZp[0x0B] = JackalRam.JeepPOWCount[x];
  JackalSpawnZp[0x35] = x;
  subSpawnObjectFromParent();
  JackalRam.JeepPOWCount[x] = 0;
  JackalRam.SpriteTypeIndex[0x10 + x] = 0x19;          /* 小爆炸 */
  JackalRam.SpriteGraphicsAttributes[0x10 + x] = 3;    /* 爆炸 palette */
  JackalRam.JeepVisibleFrameTimer[x] = 6;
  Label1202(x);                                        /* :5849 BNE → state 3 */
}

static void JeepExplodingState(uint8_t x) {  /* :5851-5866 */
  JackalRam.JeepVisibleFrameTimer[x]--;
  if (JackalRam.JeepVisibleFrameTimer[x] != 0) {
    Label1186(x);                                      /* -: JMP --（:5787） */
    return;
  }
  JackalRam.JeepVisibleFrameTimer[x] = 6;
  JackalRam.SpriteTypeIndex[0x10 + x]++;
  if (JackalRam.SpriteTypeIndex[0x10 + x] < 0x1Cu) {   /* $19/$1A/$1B 三步 */
    Label1186(x);
    return;
  }
  JackalRam.SpriteTypeIndex[0x10 + x] = 0;             /* 爆炸完隐藏 */
  JackalRam.JeepVisibleFrameTimer[x] = 0x60;           /* 空帧等待 */
  Label1202(x);                                        /* → state 4 */
}

static void JeepWaitingToRespawn(uint8_t x) {  /* :5868-5876 */
  JackalRam.JeepVisibleFrameTimer[x]--;
  if (JackalRam.JeepVisibleFrameTimer[x] != 0) {
    Label1186(x);                                      /* BNE -（:5866 → Label1186） */
    return;
  }
  JackalRam.SpriteState[0x10 + x] = 0;                 /* → JeepDeadState（下帧重生初始化） */
  if (x != 0) { JackalRam.Jeep2LifeCount--; } else { JackalRam.Jeep1LifeCount--; }
  JackalRam.Raw[0x1F] = 1;                             /* :5875（RAM_Symbols:32 written but not used） */
}

void subProcessJeepState(uint8_t x) {  /* :5364-5372（dw 顺序） */
  static void (*const stateTable[5])(uint8_t) = {
    JeepDeadState, JeepNormalState, JeepCollisionState,
    JeepExplodingState, JeepWaitingToRespawn,
  };
  stateTable[JackalRam.SpriteState[0x10 + x] % 5u](x);   /* %5：越界不防御（同 ASM），仅防 MSVC 告警路径 */
}

/* subProcessDeadJeep_StealTeammateLives（:5253-5362）：命=0 → 死亡段（属性清、
   逻辑位置归中 $80/$80、屏位回 spawn 点、GAME OVER 精灵 RNG&$80 双吉普交替、
   双人 B+A 按住借命——对方命≥2 → 对方-1 己方+1、己方位置=对方、State=0）；
   命!=0 → ++++ 活着分支（:5313 射击段，weapon.c subProcessJeepFireButtons）。 */
static void subProcessDeadJeep_StealTeammateLives(uint8_t x) {
  if ((x != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount) != 0) {
    subProcessJeepFireButtons(x);   /* ++++ :5313 */
    return;
  }
  /* :5256-5264 */
  JackalRam.Raw[0x7A + x] = 0;                          /* JeepAttributes,X */
  JackalRam.JeepHorizPosition[x] = 0x80;
  JackalRam.JeepVertPosition[x] = 0x80;
  JackalRam.SpriteHorizScreenPosition[0x10 + x] = tblJeepLevelSpawnLocation[x];
  JackalRam.SpriteVertScreenPosition[0x10 + x] = tblJeepLevelSpawnLocation[x + 2u];
  /* :5265-5276 GAME OVER 精灵（$0C）：RNG bit7 交替——bit7=1 时 P2 显示、bit7=0 时 P1 显示 */
  {
    uint8_t show = ((JackalRam.RNG_INCEveryFrame & 0x80u) != 0) ? 1u : 0u;
    JackalRam.SpriteTypeIndex[0x10 + x] = (x == show) ? 0x0Cu : 0x00u;
  }
  /* :5277-5311 双人借命（B+A 按住） */
  if (JackalRam.Player2Active != 0 &&
      (JackalRam.JeepControlsInput[x] & 0xC0u) == 0xC0u) {
    uint8_t other = (uint8_t)(x ^ 1u);
    if ((other != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount) >= 2u) {
      if (other != 0) { JackalRam.Jeep2LifeCount--; } else { JackalRam.Jeep1LifeCount--; }
      JackalRam.SpriteHorizScreenPosition[0x10 + x] = JackalRam.SpriteHorizScreenPosition[0x10 + other];
      JackalRam.JeepHorizPosition[x] = JackalRam.SpriteHorizScreenPosition[0x10 + other];
      JackalRam.SpriteVertScreenPosition[0x10 + x] = JackalRam.SpriteVertScreenPosition[0x10 + other];
      JackalRam.JeepVertPosition[x] = JackalRam.SpriteVertScreenPosition[0x10 + other];
      if (x != 0) { JackalRam.Jeep2LifeCount++; } else { JackalRam.Jeep1LifeCount++; }
      JackalRam.SpriteState[0x10 + x] = 0;              /* → JeepDeadState 重生 */
    }
  }
}

/* ---------------------------------------------------------------- Label1096 单人滚动协同 */

/* Label1096（:5156-5245）：吉普近屏边滚动协同（仅单人）。
   边界常量（JeepAttributes.ASM:31-36）：HMin=$40/HMax=$BF、VMin=$60/VMax=$BF。
   横向：X<HMin → Label1132（左界：ScrollLeft DEC（LDA #$40 立即数 quirk，BNE 恒跳）+
   位置归 $F0/$10）；X>HMax → 右界（ScrollLeft<$FF → INC+归 $BF；=$FF 且 Y>=$F1 → 归 $F0）。
   纵向：Y<VMin → Label1134（上界推进：锁!=0 且 Y>=$30 归位不滚；否则 $3D 追踪 <$40 才
   INC $3D、位置归 $60 贴顶、$4E=0 向上滚、SubPosition++、$F0 → screen++）；
   Y>VMax → Label1135（下界回退：锁!=0 或 $3D==0 → Y>=$E0 归位；否则 DEC $3D、归 $BF、
   $4E=1、SubPosition--、回绕 $EF → screen--）。 */
void Label1096(void) {
  uint8_t y = JackalRam.JeepHorizPosition[0];
  uint8_t v = JackalRam.JeepVertPosition[0];
  /* 横向段（:5157-5171；BCC/BCS 无符号借位语义） */
  if (y < 0x40u) {                                     /* X < HMin → Label1132 */
    if (JackalRam.ScreenLeftScrollPosition == 0) {     /* :5175-5181 */
      if (y < 0x10u) {
        JackalRam.JeepHorizPosition[0] = 0x10;
      }
    } else {
      /* :5182-5192（Label1136）：DEC 视口左滚 + 钳吉普到 $40——:5183 LDA #$40
         立即数 quirk（BNE + 恒跳 → :5192 STA 的是常量 $40 而非计算值）。
         初版误钳 $F0（瞬移到右缘，用户报告 bug #5） */
      JackalRam.ScreenLeftScrollPosition--;
      JackalRam.JeepHorizPosition[0] = 0x40;
    }
  } else if (y >= 0xC0u) {                             /* X > HMax（y-$40≥$80）→ :5185 右界 */
    if (JackalRam.ScreenLeftScrollPosition != 0xFFu) {
      JackalRam.ScreenLeftScrollPosition++;            /* :5194-5196 INC + 归 $BF */
      JackalRam.JeepHorizPosition[0] = 0xBF;
    } else if (y >= 0xF1u) {
      JackalRam.JeepHorizPosition[0] = 0xF0;           /* :5190（锁右端归位） */
    }
  }
  /* 纵向段（:5165-5171） */
  if (v < 0x60u) {                                     /* Y < VMin → Label1134 上界推进 */
    if (JackalRam.ScreenVerticalScrollLockForBossFight != 0) {   /* :5197-5204 锁时不滚只归位 */
      if (JackalRam.Jeep1VertPosition < 0x30u) {
        JackalRam.Jeep1VertPosition = 0x30;
      }
      return;
    }
    /* Label1140（:5205-5219）：$3D<$40 → INC $3D（回退额度恢复）；贴顶 $60+向上滚+推进 */
    if (JackalRam.PreviousScreenDataScrollTracking < 0x40u) {
      JackalRam.PreviousScreenDataScrollTracking++;
    }
    JackalRam.JeepVertPosition[0] = 0x60;
    JackalRam.ScreenScrolledUp_Down = 0;
    JackalRam.CurrentLevelScreenSubPosition++;
    if (JackalRam.CurrentLevelScreenSubPosition >= 0xF0u) {
      JackalRam.CurrentLevelScreenSubPosition = 0;
      JackalRam.CurrentLevelScreen++;                  /* :5218-5219 screen 推进 */
    }
    return;
  }
  if (v >= 0xC0u) {                                    /* Y > VMax（v-$60≥$60）→ Label1135 下界回退 */
    if (JackalRam.ScreenVerticalScrollLockForBossFight != 0 ||
        JackalRam.PreviousScreenDataScrollTracking == 0) {
      if (JackalRam.Jeep1VertPosition >= 0xE0u) {      /* :5227-5231 归位不滚 */
        JackalRam.Jeep1VertPosition = 0xE0;
      }
      return;
    }
    /* :5233-5244 回退滚屏：DEC $3D、归 $BF、$4E=1、SubPosition--、回绕 $EF → screen-- */
    JackalRam.PreviousScreenDataScrollTracking--;
    JackalRam.JeepVertPosition[0] = 0xBF;
    JackalRam.ScreenScrolledUp_Down = 1;
    JackalRam.CurrentLevelScreenSubPosition--;
    if (JackalRam.CurrentLevelScreenSubPosition >= 0xF0u) {   /* 回绕（$FF→$EF） */
      JackalRam.CurrentLevelScreenSubPosition = 0xEF;
      JackalRam.CurrentLevelScreen--;
    }
  }
  /* 中间区 RTS（:5171） */
}

void Label995(void) {  /* :4964-4971 */
  subProcessDeadJeep_StealTeammateLives(0);
  if (JackalRam.Player2Active != 0) {
    subProcessDeadJeep_StealTeammateLives(1);
    /* 双人滚动协同（:4972-5252 Label1092-1126）：Phase 6 实装 */
    return;
  }
  Label1096();                                 /* :4969 JMP（单人滚动协同） */
}
