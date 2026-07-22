/* core/weapon.c：吉普武器系统逐行翻译。
   tblPointValues=$E648（Bank7，ROM 签名 00 00 00 50 01 00... 唯一命中）；
   tblEnemyHitBoxDimension=$FA22 / tblEnemyPoints_DeathState=$FA96（Task 3.5 核实）经 BankPtr(7)；
   tblGrenadeSpeed/tblBazookaDirections/tblBazookaSpeed/tblGrenadeThrowMirroringAnimation 硬编码 C 表
   （:5990-6035、:6316-6347、:6349-6394、:6181-6182）。
   音效调用点全 stub（参数=Sound.ASM 常量，仅保留调用点）。 */
#include "ram.h"
#include "bank.h"
#include "weapon.h"
#include "spawn.h"
#include "jeep.h"
#include "trace.h"
#include "sound_stub.h"

#define TBL_ENEMY_HITBOX_DIMENSION_CPU  0xFA22u  /* :7939，16 项×2 */
#define TBL_ENEMY_POINTS_DEATHSTATE_CPU 0xFA96u  /* :8065，0x54 项 */
#define TBL_POINT_VALUES_CPU            0xE648u  /* :4940，16 项×2 BCD */

#define BULLET_FIRED_CLIP   0x09u   /* BulletBeingFiredSoundClip */
#define BULLET_TINK_CLIP    0x10u   /* BulletHitTinkSoundClip */
#define GRENADE_THROW_CLIP  0x11u   /* GrenadeThrow_BombSoundClip */
#define BAZOOKA_LAUNCH_CLIP 0x13u   /* BazookaLaunchSoundClip */
#define BOMB_HIT_GROUND_CLIP 0x15u  /* BlackAndWhiteMissile_Bomb_LaserBlastHitsGroundSoundClip */
#define BAZOOKA_HIT_GROUND_CLIP 0x16u
#define MAINWEAPON_EXPLOSION_CLIP 0x17u
#define EXTRA_LIFE_CLIP     0x26u

#define MAIN_WEAPON_X_MIN 0x10u     /* MainWeaponXMin（JeepAttributes.ASM:18） */
#define MAIN_WEAPON_X_MAX 0xF0u
#define MAIN_WEAPON_Y_MIN 0x10u
#define MAIN_WEAPON_Y_MAX 0xE8u
#define BULLET_HIT_GROUND_FRAMES 0x05u  /* JeepBulletHitGroundFrameCount（:1） */

/* tblGrenadeSpeed（:5990-6035）：方向 0-8 × {H UB, H LB, V UB, V LB} */
static const uint8_t tblGrenadeSpeed[36] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFE, 0x00,
  0x01, 0x80, 0xFE, 0x80,
  0x02, 0x00, 0x00, 0x00,
  0x01, 0x80, 0x01, 0x80,
  0x00, 0x00, 0x02, 0x00,
  0xFE, 0x80, 0x01, 0x80,
  0xFE, 0x00, 0x00, 0x00,
  0xFE, 0x80, 0xFE, 0x80,
};
/* tblGrenadeThrowMirroringAnimation（:6181-6182） */
static const uint8_t tblGrenadeThrowMirroringAnimation[4] = { 0x00, 0x80, 0x40, 0xC0 };
/* tblBazookaDirections（:6316-6347）：方向 1-8 × {TypeIndex, attr} */
static const uint8_t tblBazookaDirections[16] = {
  0x0D, 0x00, 0x0E, 0x00, 0x0F, 0x00, 0x0E, 0x80,
  0x0D, 0x80, 0x0E, 0xC0, 0x0F, 0x40, 0x0E, 0x40,
};
/* tblBazookaSpeed（:6349-6394）：方向 0-8 × {H UB, H LB, V UB, V LB} */
static const uint8_t tblBazookaSpeed[36] = {
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFC, 0x00,
  0x02, 0xC0, 0xFD, 0x40,
  0x04, 0x00, 0x00, 0x00,
  0x02, 0xC0, 0x02, 0xC0,
  0x00, 0x00, 0x04, 0x00,
  0xFD, 0x40, 0x02, 0xC0,
  0xFC, 0x00, 0x00, 0x00,
  0xFD, 0x40, 0xFD, 0x40,
};

/* ---------------------------------------------------------------- 位置助手（:1689-1757，Raw[0x39] 槽） */

void subUpdateGrenade_Bazooka_BulletPositionForSpeed(void) {  /* :1712 */
  uint8_t x = JackalRam.Raw[0x39];
  uint16_t acc;
  acc = (uint16_t)JackalRam.SpriteHorizScreenPositionSubPixel[x] +
        JackalRam.SpriteHorizSpeedLB[x];
  JackalRam.SpriteHorizScreenPositionSubPixel[x] = (uint8_t)acc;
  acc = (uint16_t)JackalRam.SpriteHorizScreenPosition[x] +
        JackalRam.SpriteHorizSpeedUB[x] + (acc >> 8);
  JackalRam.SpriteHorizScreenPosition[x] = (uint8_t)acc;
  acc = (uint16_t)JackalRam.SpriteVertScreenPositionSubPixel[x] +
        JackalRam.SpriteVertSpeedLB[x];
  JackalRam.SpriteVertScreenPositionSubPixel[x] = (uint8_t)acc;
  acc = (uint16_t)JackalRam.SpriteVertScreenPosition[x] +
        JackalRam.SpriteVertSpeedUB[x] + (acc >> 8);
  JackalRam.SpriteVertScreenPosition[x] = (uint8_t)acc;
}

void subUpdateGrenade_Bazooka_BulletPositionForScroll(void) {  /* :1689 */
  uint8_t x = JackalRam.Raw[0x39];
  uint8_t dy;
  /* :1692-1697 横向：HScroll_PPU - LeftScroll + HorizPos */
  JackalRam.SpriteHorizScreenPosition[x] = (uint8_t)(
      JackalRam.ScreenHorizontalScrollPosition_PPU - JackalRam.ScreenLeftScrollPosition +
      JackalRam.SpriteHorizScreenPosition[x]);
  /* :1698-1708 纵向：滚动差值 <$10/≥$F0 直接用，否则 EOR $10 */
  dy = (uint8_t)(JackalRam.CurrentLevelScreenSubPosition -
                 JackalRam.PreviousLevelScreenSubposition);
  if (dy >= 0x10u && dy < 0xF0u) {
    dy = (uint8_t)(dy ^ 0x10u);
  }
  JackalRam.SpriteVertScreenPosition[x] = (uint8_t)(JackalRam.SpriteVertScreenPosition[x] + dy);
}

void subZeroOutSpriteSpeed(void) {  /* :1739 */
  uint8_t x = JackalRam.Raw[0x39];
  JackalRam.SpriteHorizSpeedUB[x] = 0;
  JackalRam.SpriteHorizSpeedLB[x] = 0;
  JackalRam.SpriteVertSpeedUB[x] = 0;
  JackalRam.SpriteVertSpeedLB[x] = 0;
}

void Label1055(void) {  /* :1750 */
  uint8_t x = JackalRam.Raw[0x39];
  JackalRam.SpriteState[x] = 0;
  JackalRam.SpriteTypeIndex[x] = 0;
}

/* ---------------------------------------------------------------- 分数（:1539-1637、:4930-4956） */

/* subAddToPlayerScore_1Byte（:1576-1603）：BCD 半字节调整；*carry 入=上一字节进位、出=本位进位 */
static uint8_t subAddToPlayerScore_1Byte(uint8_t scoreByte, uint8_t addByte, uint8_t *carry) {
  uint8_t hi = (uint8_t)(scoreByte & 0xF0u);
  uint8_t lo = (uint8_t)(scoreByte & 0x0Fu);
  uint16_t a = (uint16_t)(addByte & 0x0Fu) + lo + *carry;
  uint16_t b;
  if (a >= 0x0Au) {
    a += 6u;                     /* ADC #$05 在 CMP C=1 下=+6（:1587-1589） */
  }
  a += hi;                       /* :1591 ADC $06 */
  b = (uint16_t)(addByte & 0xF0u) + (uint8_t)a;   /* :1593-1595 */
  *carry = 0;
  if (b >= 0x100u || (uint8_t)b >= 0xA0u) {       /* :1596 BCS / :1599 CMP */
    b = (uint8_t)b - 0xA0u;      /* SBC #$A0（两路径 C 均=1）→ SEC */
    *carry = 1;
  }
  return (uint8_t)b;
}

/* Label865（:1605-1637）：score 高字节 ≥ JeepNext1Up → 阈值 BCD+5（溢出 $FF 封顶）、命+1（<9） */
static void Label865(uint8_t player) {
  uint8_t *score = player != 0 ? JackalRam.Jeep2Score : JackalRam.Jeep1Score;
  uint8_t threshold = JackalRam.JeepNext1Up[player];
  uint8_t carry = 0;
  if (score[2] < threshold) {
    return;                      /* ++ :1609 BCC */
  }
  /* Label871（:1573-1575）：阈值 += 5（BCD；$00[0]=5 经 subAddToPlayerScore_1Byte） */
  threshold = subAddToPlayerScore_1Byte(threshold, 5, &carry);
  if (carry != 0) {
    threshold = 0xFF;            /* :1616 溢出封顶 */
  }
  JackalRam.JeepNext1Up[player] = threshold;
  if ((player != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount) < 9u) {
    if (player != 0) { JackalRam.Jeep2LifeCount++; } else { JackalRam.Jeep1LifeCount++; }
    subInitiateSoundClip(EXTRA_LIFE_CLIP);   /* stub */
  }
  /* :1625-1637 HighScore 逐字节比较更新（unused，ASM 原样） */
  {
    int8_t i;
    for (i = 2; i >= 0; i--) {
      if (JackalRam.HighScore[i] < score[i]) {
        uint8_t j;
        for (j = 0; j < 3; j++) { JackalRam.HighScore[j] = score[j]; }
        return;
      }
      if (JackalRam.HighScore[i] != score[i]) {
        return;
      }
    }
  }
}

void subAddToPlayerScore(uint8_t player, const uint8_t add3[3]) {  /* :1539 */
  uint8_t *score = player != 0 ? JackalRam.Jeep2Score : JackalRam.Jeep1Score;
  uint8_t carry = 0;
  uint8_t i;
  if (JackalRam.TitleScreenMode != 0) {
    return;                      /* :1543-1545 */
  }
  for (i = 0; i < 3; i++) {
    score[i] = subAddToPlayerScore_1Byte(score[i], add3[i], &carry);
  }
  if (carry == 0) {
    Label865(player);            /* :1565 BCC */
  } else {
    JackalRam.HighScore[0] = 0x99;   /* :1566-1571 溢出封顶（unused 字段，ASM 原样） */
    JackalRam.HighScore[1] = 0x99;
    JackalRam.HighScore[2] = 0x99;
  }
}

void subGetObjectPointsValue_AddToPlayerScore(uint8_t player, uint8_t enemySlot) {  /* :4930 */
  uint8_t idx = (uint8_t)(JackalRam.EnemyPoints[enemySlot] * 2u);
  const uint8_t *tbl = BankPtr(7, TBL_POINT_VALUES_CPU);
  uint8_t add[3];
  add[0] = tbl[(uint8_t)(idx + 1u)];   /* $01=tbl[X+1]（低） */
  add[1] = tbl[idx];                   /* $02=tbl[X]（中） */
  add[2] = 0;                          /* $03=0（高，:1542） */
  subAddToPlayerScore(player, add);
}

/* Label1059（:4747-4757）：击杀结算——加分 + 死亡动画状态 */
static void Label1059(uint8_t player, uint8_t enemySlot) {
  uint8_t id = (uint8_t)(JackalRam.SpriteObjectID[enemySlot] & 0x7Fu);
  subGetObjectPointsValue_AddToPlayerScore(player, enemySlot);
  JackalRam.SpriteState[enemySlot] =
      (uint8_t)(BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU)[id] & 0x0Fu);
}

/* ---------------------------------------------------------------- Label1001（:4583-4646 子弹碰撞） */

void Label1001(uint8_t x) {
  const uint8_t *dim = BankPtr(7, TBL_ENEMY_HITBOX_DIMENSION_CPU);
  uint8_t slot = (uint8_t)(0x12 + x);        /* 子弹槽（$39 同步槽） */
  int8_t y;
  if (JackalRam.SpriteState[slot] == 0) {
    return;                    /* :4586-4587 BEQ ++++（无弹不查） */
  }
  for (y = 0x0F; y >= 0; y--) {
    uint8_t shape, w, h, dx, dy;
    if ((JackalRam.SpriteObjectID[y] & 0x7Fu) == 0) { continue; }       /* ++ */
    if ((int8_t)JackalRam.SpriteState[y] < 0) { continue; }             /* 屏外 */
    if ((JackalRam.SpriteState[y] & 0x7Fu) == 0) { continue; }          /* state 0 */
    shape = (uint8_t)(JackalRam.SpriteHitboxShapeIndex[y] & 0x5Fu);
    if (shape >= 0x40u) { continue; }                                   /* 无子弹碰撞 */
    shape = (uint8_t)((JackalRam.SpriteHitboxShapeIndex[y] & 0x0Fu) * 2u);
    w = dim[shape];
    h = dim[(uint8_t)(shape + 1u)];
    dx = (uint8_t)(JackalRam.SpriteHorizScreenPosition[slot] -
                   JackalRam.SpriteHorizScreenPosition[y]);
    if ((int8_t)dx < 0) { dx = (uint8_t)(0u - dx); }        /* BCS +/EOR 绝对值 */
    if (dx >= w) { continue; }
    dy = (uint8_t)(JackalRam.SpriteVertScreenPosition[slot] -
                   JackalRam.SpriteVertScreenPosition[y]);
    if ((int8_t)dy < 0) { dy = (uint8_t)(0u - dy); }
    if (dy >= h) { continue; }
    Label1055();                                                          /* 消弹（$39 槽） */
    JackalRam.SpriteHealthHP[y]--;
    if ((JackalRam.SpriteHealthHP[y] & 0x7Fu) == 0) {                     /* 击杀 → Label1059 */
      Label1059(x < 3 ? 0u : 1u, (uint8_t)y);                             /* :4641-4644 子弹槽 0-2=P1 */
      return;                                                             /* +++ JMP Label1059 后 RTS */
    }
    subInitiateSoundClip(BULLET_TINK_CLIP);                               /* stub */
  }
}

/* ---------------------------------------------------------------- Label996（:4648-4759 主武器碰撞） */

void Label996(uint8_t x) {
  const uint8_t *dim = BankPtr(7, TBL_ENEMY_HITBOX_DIMENSION_CPU);
  uint8_t slot = (uint8_t)(0x18 + x);        /* 武器槽 */
  int8_t y;
  uint8_t state = JackalRam.SpriteState[slot];
  if (state == 0) {
    return;                    /* :4653 BEQ - */
  }
  for (y = 0x0F; y >= 0; y--) {
    uint8_t shape, w, h, dx, dy;
    if (state < 9u) {          /* :4654-4664：非 splash 段——HP bit7 敌 + 武器已触发爆炸 → RTS */
      if ((int8_t)JackalRam.SpriteHealthHP[y] < 0 &&
          (state >= 7u || state == 3u || state == 4u)) {
        return;
      }
    }
    if ((JackalRam.SpriteObjectID[y] & 0x7Fu) == 0) { goto next; }        /* -- */
    if ((int8_t)JackalRam.SpriteState[y] < 0) { goto next; }
    if ((JackalRam.SpriteState[y] & 0x7Fu) == 0) { goto next; }
    shape = (uint8_t)(JackalRam.SpriteHitboxShapeIndex[y] & 0x2Fu);
    if (shape >= 0x20u) { goto next; }                                    /* 无主武器碰撞 */
    shape = (uint8_t)((JackalRam.SpriteHitboxShapeIndex[y] & 0x0Fu) * 2u);
    w = (uint8_t)(dim[shape] + 4u);                                       /* :4681 +4 边 */
    h = (uint8_t)(dim[(uint8_t)(shape + 1u)] + 4u);
    dx = (uint8_t)(JackalRam.SpriteHorizScreenPosition[slot] -
                   JackalRam.SpriteHorizScreenPosition[y]);
    if ((int8_t)dx < 0) { dx = (uint8_t)(0u - dx); }
    if (dx >= w) { goto next; }
    dy = (uint8_t)(JackalRam.SpriteVertScreenPosition[slot] -
                   JackalRam.SpriteVertScreenPosition[y]);
    if ((int8_t)dy < 0) { dy = (uint8_t)(0u - dy); }
    if (dy >= h) { goto next; }
    if (x >= 2u && (int8_t)JackalRam.SpriteHealthHP[y] < 0) { goto next; }  /* :4706-4709 splash vs 耗血敌 */
    if ((JackalRam.SpriteHitboxShapeIndex[y] & 0x10u) == 0 && x < 2u) {     /* :4710-4724 主武器状态变更 */
      JackalRam.SpriteState[slot] = state >= 5u ? 7u : 3u;                /* bazooka→7 / grenade→3 */
      subInitiateSoundClip(MAINWEAPON_EXPLOSION_CLIP);                    /* stub */
    }
    /* :4725-4737 伤害：HP bit7 清 → 瞬杀；bit7 置 → & $7F -5，≤0 死、>0 续 */
    if ((int8_t)JackalRam.SpriteHealthHP[y] >= 0) {
      Label1059((uint8_t)(x & 1u), (uint8_t)y);                           /* + :4742（player=X&1） */
      return;
    }
    {
      uint8_t rest = (uint8_t)((JackalRam.SpriteHealthHP[y] & 0x7Fu) - 5u);
      if (rest == 0u || (int8_t)rest < 0) {
        Label1059((uint8_t)(x & 1u), (uint8_t)y);
        return;
      }
      JackalRam.SpriteHealthHP[y] = (uint8_t)(0x80u | rest);
    }
  next:;
  }
}

/* ---------------------------------------------------------------- 发射（:5313-5362 射击段） */

/* FIRE 串口追踪（调试设施，非 ASM 语义）：发射发生时输出，供 verify_phase3 核对。 */
static void traceFire(const char *kind, uint8_t x) {
  char buf[20];
  char *p = buf;
  const char *s;
  if (JackalTraceHook == 0) {
    return;
  }
  s = "FIRE ";
  while (*s != 0) { *p++ = *s++; }
  s = kind;
  while (*s != 0) { *p++ = *s++; }
  *p++ = (char)('0' + x);
  *p = 0;
  JackalTraceHook(buf);
}

void subProcessJeepFireButtons(uint8_t x) {
  if (JackalRam.SpriteState[0x10 + x] >= 2u) {   /* :5313-5315 */
    subProcessJeepState(x);
    return;
  }
  /* B 键（:5317-5345）：边沿 或（按住 且 RNG&$1F==0）→ 首个空子弹槽发射 */
  if ((JackalRam.JeepControlsInput1Frame[x] & 0x40u) != 0 ||
      ((JackalRam.JeepControlsInput[x] & 0x40u) != 0 &&
       (JackalRam.RNG_INCEveryFrame & 0x1Fu) == 0)) {
    int8_t y = x == 0 ? 2 : 5;
    int8_t stop = x == 0 ? -1 : 2;         /* P1 扫 2,1,0；P2 扫 5,4,3（:5336 CPY#2 止） */
    for (; y > stop; y--) {
      if (JackalRam.SpriteState[0x12 + y] == 0) {
        JackalRam.SpriteState[0x12 + y] = 1;
        JackalRam.SpriteHorizScreenPosition[0x12 + y] = JackalRam.SpriteHorizScreenPosition[0x10 + x];
        JackalRam.SpriteVertScreenPosition[0x12 + y] = JackalRam.SpriteVertScreenPosition[0x10 + x];
        traceFire("B", x);
        break;
      }
    }
  }
  /* A 键（:5346-5361）：仅边沿、MainWeaponState==0 → 手雷 1/火箭筒 5 */
  if ((JackalRam.JeepControlsInput1Frame[x] & 0x80u) == 0) {
    subProcessJeepState(x);
    return;
  }
  if (JackalRam.SpriteState[0x18 + x] != 0) {
    subProcessJeepState(x);
    return;
  }
  JackalRam.SpriteState[0x18 + x] = JackalRam.JeepMainWeapon[x] != 0 ? 5u : 1u;
  JackalRam.SpriteHorizScreenPosition[0x18 + x] = JackalRam.SpriteHorizScreenPosition[0x10 + x];
  JackalRam.SpriteVertScreenPosition[0x18 + x] = JackalRam.SpriteVertScreenPosition[0x10 + x];
  traceFire("A", x);
  subProcessJeepState(x);
}

/* ---------------------------------------------------------------- 子弹（:5911-5967） */

void subProcessJeepBullet(uint8_t x) {
  uint8_t slot = (uint8_t)(0x12 + x);
  uint8_t state = JackalRam.SpriteState[slot];
  uint8_t collision;
  if (state == 0) {
    return;
  }
  if (state == 3) {          /* :5964-5967：击地动画到 0 → Label1055 */
    JackalRam.JeepBulletDisplayFrames[x]--;
    if (JackalRam.JeepBulletDisplayFrames[x] != 0) {
      return;
    }
    Label1055();
    return;
  }
  if (state == 1) {          /* :5919-5931 发射初始化 */
    subInitiateSoundClip(BULLET_FIRED_CLIP);   /* stub */
    JackalRam.SpriteTypeIndex[slot] = 0x10;
    JackalRam.SpriteGraphicsAttributes[slot] = 1;
    subZeroOutSpriteSpeed();
    JackalRam.SpriteVertSpeedUB[slot] = 0xFA;  /* -6 上行 */
    JackalRam.JeepBulletDisplayFrames[x] = 0x10;
    JackalRam.SpriteState[slot]++;
    return;
  }
  /* state 2（:5933-5962 飞行） */
  if (!(JackalRam.CurrentLevel == 5u && JackalRam.CurrentLevelScreen >= 0x0Du)) {
    collision = Label1210(x);                  /* :5940（L6 末屏不查） */
    if (JackalRam.CurrentLevel == 3u && collision == 3u) {
      goto dec;                                /* :5943-5945 L4 水不击地 */
    }
    if ((collision & 1u) != 0) {
      goto ground;                             /* :5946-5947 type 1/3 击地 */
    }
  }
dec:
  JackalRam.JeepBulletDisplayFrames[x]--;
  if (JackalRam.JeepBulletDisplayFrames[x] != 0) {
    goto fly;
  }
ground:                   /* :5950-5954 击地 */
  JackalRam.JeepBulletDisplayFrames[x] = BULLET_HIT_GROUND_FRAMES;
  JackalRam.SpriteTypeIndex[slot]++;
  JackalRam.SpriteState[slot]++;
  subZeroOutSpriteSpeed();
fly:                      /* :5955-5962 */
  subUpdateGrenade_Bazooka_BulletPositionForSpeed();
  if (JackalRam.SpriteHorizScreenPosition[slot] >= 0xFAu ||
      JackalRam.SpriteVertScreenPosition[slot] >= 0xFAu) {
    Label1055();
    return;
  }
  subUpdateGrenade_Bazooka_BulletPositionForScroll();
}

/* ---------------------------------------------------------------- 主武器（:5969-6425） */

/* 飞行公共段（:6200-6214 Label1235→Label1233）：位置更新 + 边界/timer → ++（INC state） */
static void mainWeaponFlightCommon(uint8_t x) {
  uint8_t slot = (uint8_t)(0x18 + x);
  subUpdateGrenade_Bazooka_BulletPositionForSpeed();
  subUpdateGrenade_Bazooka_BulletPositionForScroll();
  if (JackalRam.SpriteHorizScreenPosition[slot] < MAIN_WEAPON_X_MIN ||
      JackalRam.SpriteHorizScreenPosition[slot] >= MAIN_WEAPON_X_MAX ||
      JackalRam.SpriteVertScreenPosition[slot] < MAIN_WEAPON_Y_MIN ||
      JackalRam.SpriteVertScreenPosition[slot] >= MAIN_WEAPON_Y_MAX) {
    JackalRam.SpriteState[slot]++;           /* :6214 BEQ ++ → :6262 INC（越界爆炸） */
    return;
  }
  JackalRam.Raw[0x75C + x]--;                /* JeepMainWeaponTimer,X（$075C 段） */
  if (JackalRam.Raw[0x75C + x] == 0) {
    JackalRam.SpriteState[slot]++;
  }
}

/* 爆炸初始化公共段（:6254-6262 尾）：速度清、TypeIndex=$19、palette 3、timer=8、state++ */
static void mainWeaponExplosionInit(uint8_t x) {
  uint8_t slot = (uint8_t)(0x18 + x);
  subUpdateGrenade_Bazooka_BulletPositionForScroll();
  subZeroOutSpriteSpeed();
  JackalRam.SpriteTypeIndex[slot] = 0x19;
  JackalRam.SpriteGraphicsAttributes[slot] = 3;
  JackalRam.Raw[0x75C + x] = 0x08;
  JackalRam.SpriteState[slot]++;
}

/* 爆炸动画公共段（:6266-6276 State3）：每 8 帧 TypeIndex++；$1C → Label1055 */
static void mainWeaponExplosionAnim(uint8_t x) {
  uint8_t slot = (uint8_t)(0x18 + x);
  subUpdateGrenade_Bazooka_BulletPositionForScroll();
  JackalRam.Raw[0x75C + x]--;
  if (JackalRam.Raw[0x75C + x] != 0) {
    return;
  }
  JackalRam.Raw[0x75C + x] = 0x08;
  JackalRam.SpriteTypeIndex[slot]++;
  if (JackalRam.SpriteTypeIndex[slot] != 0x1Cu) {
    return;
  }
  Label1055();
}

void subProcessJeepMainWeapon(uint8_t x) {
  uint8_t slot = (uint8_t)(0x18 + x);
  uint8_t state = JackalRam.SpriteState[slot];
  uint8_t tend4;
  switch (state) {
  case 0:
    return;
  case 1:                                    /* JeepGrenadeState0（:6037-6065） */
    subInitiateSoundClip(GRENADE_THROW_CLIP);   /* stub */
    JackalRam.SpriteTypeIndex[slot] = 7;
    JackalRam.SpriteGraphicsAttributes[slot] = 0;
    tend4 = (uint8_t)(JackalRam.JeepDirectionTendency[x] * 4u);
    JackalRam.SpriteHorizSpeedUB[slot] = tblGrenadeSpeed[tend4];
    JackalRam.SpriteHorizSpeedLB[slot] = tblGrenadeSpeed[tend4 + 1u];
    JackalRam.SpriteVertSpeedUB[slot] = tblGrenadeSpeed[tend4 + 2u];
    JackalRam.SpriteVertSpeedLB[slot] = tblGrenadeSpeed[tend4 + 3u];
    JackalRam.Raw[0x75C + x] =
        (JackalRam.ScreenVerticalScrollLockForBossFight != 0 && JackalRam.CurrentLevel == 5u)
            ? 0x32u : 0x28u;                 /* :6056-6062 */
    JackalRam.SpriteState[slot]++;
    return;
  case 2:                                    /* JeepGrenadeState1（:6184-6199） */
    JackalRam.JeepWeaponAttributes[6 + x] =
        tblGrenadeThrowMirroringAnimation[(JackalRam.RNG_INCEveryFrame & 0x0Fu) >> 2];
    JackalRam.SpriteTypeIndex[slot] =
        (JackalRam.Raw[0x75C + x] < 0x1Au && JackalRam.Raw[0x75C + x] >= 0x0Eu) ? 8u : 7u;
    mainWeaponFlightCommon(x);
    return;
  case 3:                                    /* JeepGrenadeState2（:6251-6262） */
    subInitiateSoundClip(BOMB_HIT_GROUND_CLIP);   /* stub */
    mainWeaponExplosionInit(x);
    return;
  case 4:                                    /* JeepGrenadeState3（:6266） */
    mainWeaponExplosionAnim(x);
    return;
  case 5:                                    /* JeepBazookaState0（:6281-6314） */
    JackalRam.SpriteTypeIndex[slot] =
        tblBazookaDirections[(uint8_t)((JackalRam.JeepDirectionTendency[x] - 1u) * 2u)];
    JackalRam.JeepWeaponAttributes[6 + x] =
        tblBazookaDirections[(uint8_t)((JackalRam.JeepDirectionTendency[x] - 1u) * 2u + 1u)];
    subInitiateSoundClip(BAZOOKA_LAUNCH_CLIP);   /* stub */
    JackalRam.SpriteGraphicsAttributes[slot] = 1;
    tend4 = (uint8_t)(JackalRam.JeepDirectionTendency[x] * 4u);
    JackalRam.SpriteHorizSpeedUB[slot] = tblBazookaSpeed[tend4];
    JackalRam.SpriteHorizSpeedLB[slot] = tblBazookaSpeed[tend4 + 1u];
    JackalRam.SpriteVertSpeedUB[slot] = tblBazookaSpeed[tend4 + 2u];
    JackalRam.SpriteVertSpeedLB[slot] = tblBazookaSpeed[tend4 + 3u];
    JackalRam.Raw[0x75C + x] =
        (JackalRam.ScreenVerticalScrollLockForBossFight != 0 && JackalRam.CurrentLevel == 5u)
            ? 0x20u : 0x16u;
    JackalRam.SpriteState[slot]++;
    return;
  case 6: {                                  /* JeepBazookaState1（:6153-6177） */
    uint8_t collision;
    if (JackalRam.CurrentLevel == 4u) {      /* L5 boss 锁不查（:6154-6159） */
      if (JackalRam.ScreenVerticalScrollLockForBossFight != 0) {
        mainWeaponFlightCommon(x);
        return;
      }
    } else if (JackalRam.CurrentLevel == 5u &&
               JackalRam.CurrentLevelScreen >= 0x0Du) {   /* L6 末屏不查（:6160-6165） */
      mainWeaponFlightCommon(x);
      return;
    }
    collision = Label1210(x);
    if ((JackalRam.CurrentLevel == 3u || JackalRam.CurrentLevel == 5u) && collision == 3u) {
      mainWeaponFlightCommon(x);             /* :6167-6173 L4/L6 type3 不关（Label1235） */
      return;
    }
    if ((collision & 1u) == 0) {
      mainWeaponFlightCommon(x);             /* :6174-6175 type 0/2 继续飞 */
      return;
    }
    JackalRam.SpriteState[slot]++;           /* :6176 碰撞 → state 7 */
    return;
  }
  case 7:                                    /* JeepBazookaState2（:6216-6249） */
    subInitiateSoundClip(BAZOOKA_HIT_GROUND_CLIP);   /* stub */
    if (JackalRam.JeepMainWeapon[x] >= 2u) { /* splash 生成（:6222-6249） */
      JackalRam.SpriteState[0x1A + x] = 9;   /* H splash State0 */
      JackalRam.SpriteHorizScreenPosition[0x1A + x] = JackalRam.SpriteHorizScreenPosition[slot];
      JackalRam.SpriteVertScreenPosition[0x1A + x] = JackalRam.SpriteVertScreenPosition[slot];
      JackalRam.Raw[0x766 + x] = 0;          /* H 方向标志 */
      if (JackalRam.JeepMainWeapon[x] >= 3u) {
        JackalRam.SpriteState[0x1C + x] = 9; /* V splash State0 */
        JackalRam.SpriteHorizScreenPosition[0x1C + x] = JackalRam.SpriteHorizScreenPosition[slot];
        JackalRam.SpriteVertScreenPosition[0x1C + x] = JackalRam.SpriteVertScreenPosition[slot];
        JackalRam.Raw[0x768 + x] = 1;        /* V 方向标志 */
      }
    }
    mainWeaponExplosionInit(x);              /* :6254-6262 公共尾段 */
    return;
  case 8:                                    /* JeepBazookaState3（:6266） */
    mainWeaponExplosionAnim(x);
    return;
  case 9:                                    /* JeepBazookaSplashState0（:6396-6425） */
    JackalRam.SpriteGraphicsAttributes[slot] = 3;
    JackalRam.SpriteTypeIndex[slot] = 9;
    JackalRam.Raw[0x75C + x] = 0x28;
    JackalRam.SpriteHorizSpeedUB[slot] = 0;
    JackalRam.SpriteHorizSpeedLB[slot] = 0;
    JackalRam.SpriteVertSpeedUB[slot] = 0;
    JackalRam.SpriteVertSpeedLB[slot] = 0;
    JackalRam.BazookaSplashHorizOffset[x - 2u] = JackalRam.SpriteHorizScreenPosition[slot];   /* :6408-6411 */
    JackalRam.BazookaSplashVertOffset[x - 2u] = JackalRam.SpriteVertScreenPosition[slot];
    if (JackalRam.Raw[0x764 + x] == 0) {     /* :6412-6418 方向 0=横飞 */
      JackalRam.SpriteHorizSpeedUB[slot] = 2;
    } else {                                 /* :6420-6424 方向 1=纵飞 */
      JackalRam.SpriteVertSpeedUB[slot] = 2;
    }
    JackalRam.SpriteState[slot]++;
    return;
  case 10: {                                 /* JeepBazookaSplashState1（:6069-6151） */
    uint8_t yDir;
    /* :6070-6078 老化外形：timer≥$18→$0A、≥$08→$0A、<$08→$0B */
    if (JackalRam.Raw[0x75C + x] >= 0x08u) {
      JackalRam.SpriteTypeIndex[slot] = 0x0A;
    } else {
      JackalRam.SpriteTypeIndex[slot] = 0x0B;
    }
    if ((JackalRam.RNG_INCEveryFrame & 3u) == 0) {   /* :6079-6084 闪烁 */
      JackalRam.JeepWeaponAttributes[6 + x] ^= 0x80u;
    }
    /* :6085-6112 scroll 同步（$10=纵向增量：无滚动 0/正 1/负 $FF；$11=横向增量：
       无滚动 0/正（LeftScroll>PPU）$FF/负 1——两轴 BCS/BCC 方向相反，ASM 原样） */
    {
      uint8_t dV = (uint8_t)(JackalRam.CurrentLevelScreenSubPosition -
                             JackalRam.PreviousLevelScreenSubposition);
      uint8_t dH = (uint8_t)(JackalRam.ScreenLeftScrollPosition -
                             JackalRam.ScreenHorizontalScrollPosition_PPU);
      uint8_t s10 = dV == 0 ? 0u : (dV < 0x80u ? 1u : 0xFFu);
      uint8_t s11 = dH == 0 ? 0u : (dH < 0x80u ? 0xFFu : 1u);
      JackalRam.BazookaSplashHorizOffset[x - 2u] =
          (uint8_t)(JackalRam.BazookaSplashHorizOffset[x - 2u] + s11);
      JackalRam.SpriteHorizScreenPosition[slot] = JackalRam.BazookaSplashHorizOffset[x - 2u];
      JackalRam.BazookaSplashVertOffset[x - 2u] =
          (uint8_t)(JackalRam.BazookaSplashVertOffset[x - 2u] + s10);
      JackalRam.SpriteVertScreenPosition[slot] = JackalRam.BazookaSplashVertOffset[x - 2u];
    }
    /* :6113-6150 速度翻转老化（$9A/$A2 方向记忆）：本帧移动 ±(UB+2)，
       但移动后 UB 恢复为未取反的 $10/$11（:6143-6146 LDA $10 STA）——振荡幅度
       逐帧递增（+4,-6,+8,-10…），火焰范围越来越大。初版误存取反值 → UB 在 ±6
       小循环，火焰窝在爆点不展开（用户报告"不是开花弹"根因） */
    {
      uint8_t s10 = 0, s11 = 0;
      if (JackalRam.SpriteHorizSpeedUB[slot] != 0) {
        const uint8_t raw10 = (uint8_t)(JackalRam.SpriteHorizSpeedUB[slot] + 2u);  /* $10 */
        s10 = raw10;
        if (JackalRam.Raw[0x9A + x] != 0) {
          yDir = 0;
          s10 = (uint8_t)(0u - s10);
        } else {
          yDir = 1;
        }
        JackalRam.Raw[0x9A + x] = yDir;
        JackalRam.SpriteHorizSpeedUB[slot] = s10;   /* 本帧移动用 ±(UB+2) */
        s10 = raw10;                                 /* 恢复用未取反值 */
      }
      if (JackalRam.SpriteVertSpeedUB[slot] != 0) {
        const uint8_t raw11 = (uint8_t)(JackalRam.SpriteVertSpeedUB[slot] + 2u);  /* $11 */
        s11 = raw11;
        if (JackalRam.Raw[0xA2 + x] != 0) {
          yDir = 0;
          s11 = (uint8_t)(0u - s11);
        } else {
          yDir = 1;
        }
        JackalRam.Raw[0xA2 + x] = yDir;
        JackalRam.SpriteVertSpeedUB[slot] = s11;
        s11 = raw11;
      }
      subUpdateGrenade_Bazooka_BulletPositionForSpeed();
      JackalRam.SpriteHorizSpeedUB[slot] = s10;     /* :6143-6146 恢复 $10/$11 */
      JackalRam.SpriteVertSpeedUB[slot] = s11;
    }
    /* JMP Label1233（:6151）：边界/timer 检查（不含 ForSpeed，已内联上段） */
    subUpdateGrenade_Bazooka_BulletPositionForScroll();
    if (JackalRam.SpriteHorizScreenPosition[slot] < MAIN_WEAPON_X_MIN ||
        JackalRam.SpriteHorizScreenPosition[slot] >= MAIN_WEAPON_X_MAX ||
        JackalRam.SpriteVertScreenPosition[slot] < MAIN_WEAPON_Y_MIN ||
        JackalRam.SpriteVertScreenPosition[slot] >= MAIN_WEAPON_Y_MAX) {
      JackalRam.SpriteState[slot]++;
      return;
    }
    JackalRam.Raw[0x75C + x]--;
    if (JackalRam.Raw[0x75C + x] == 0) {
      JackalRam.SpriteState[slot]++;
    }
    return;
  }
  case 11:                                   /* JeepBazookaSplashState2（:6278） */
    Label1055();
    return;
  default:
    return;
  }
}
