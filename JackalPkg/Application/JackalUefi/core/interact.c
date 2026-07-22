/* core/interact.c：Label1005（:4762-4928）逐行翻译。
   tblEnemyHitBoxDimension=$FA22 / tblEnemyPoints_DeathState=$FA96（Bank7，Task 3.5 核实）。
   音效调用点全 stub（参数=Sound.ASM 常量）。 */
#include "ram.h"
#include "bank.h"
#include "interact.h"
#include "spawn.h"
#include "weapon.h"
#include "sound_stub.h"

#define TBL_ENEMY_HITBOX_DIMENSION_CPU  0xFA22u
#define TBL_ENEMY_POINTS_DEATHSTATE_CPU 0xFA96u

#define POW_PICKED_UP_CLIP     0x1Du   /* POWPickedUpSoundClip */
#define WEAPON_UPGRADE_CLIP    0x21u   /* WeaponUpgradeSoundClip */
#define EXTRA_LIFE_CLIP        0x26u   /* ExtraLifeSoundClip */

/* Label1074（:4873-4884）：敌死亡动画状态 + 得分（碾压/双杀共用尾段） */
static void Label1074(uint8_t x, uint8_t y) {
  uint8_t id = (uint8_t)(JackalRam.SpriteObjectID[x] & 0x7Fu);
  JackalRam.SpriteState[x] =
      (uint8_t)(BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU)[id] & 0x0Fu);
  subGetObjectPointsValue_AddToPlayerScore(y, x);
}

/* POWPickedUp（:4831-4864） */
static void powPickedUp(uint8_t x, uint8_t y) {
  JackalRam.JeepPOWCount[y]++;
  subInitiateSoundClip(POW_PICKED_UP_CLIP);   /* stub */
  if ((JackalRam.SpriteData5[x] & 1u) != 0) {  /* 闪烁 POW=power up（:4845-4856） */
    uint8_t wsum;
    subInitiateSoundClip(WEAPON_UPGRADE_CLIP);   /* stub */
    if (JackalRam.JeepMainWeapon[y] < 3u) {
      JackalRam.JeepMainWeapon[y]++;
    }
    wsum = (uint8_t)(JackalRam.JeepMainWeapon[0] + JackalRam.JeepMainWeapon[1]);
    JackalRam.DifficultyBasedOnWeapon = wsum >= 3u ? 3u : wsum;   /* :4857-4863 */
  }
  subDespawnSprite(x);                           /* :4864 JMP */
}

void Label1005(uint8_t x) {
  const uint8_t *dim = BankPtr(7, TBL_ENEMY_HITBOX_DIMENSION_CPU);
  uint8_t id;
  int8_t y;
  if ((JackalRam.SpriteObjectID[x] & 0x7Fu) == 0) { return; }      /* :4763-4765 */
  if ((int8_t)JackalRam.SpriteState[x] < 0) { return; }            /* 屏外 */
  if ((JackalRam.SpriteState[x] & 0x7Fu) == 0) { return; }         /* state 0 */
  if ((int8_t)JackalRam.SpriteHitboxShapeIndex[x] < 0) { return; } /* :4775-4776 MSB 无吉普碰撞 */
  for (y = 1; y >= 0; y--) {
    uint8_t shape, w, h, dx, dy;
    if (JackalRam.SpriteState[0x10 + y] == 0 ||
        JackalRam.SpriteState[0x10 + y] >= 2u) {                   /* :4771-4774 */
      continue;
    }
    shape = (uint8_t)((JackalRam.SpriteHitboxShapeIndex[x] & 0x0Fu) * 2u);
    w = dim[shape];
    h = dim[(uint8_t)(shape + 1u)];
    dx = (uint8_t)(JackalRam.SpriteHorizScreenPosition[0x10 + y] -
                   JackalRam.SpriteHorizScreenPosition[x]);
    if ((int8_t)dx < 0) { dx = (uint8_t)(0u - dx); }
    if (dx >= w) { continue; }
    dy = (uint8_t)(JackalRam.SpriteVertScreenPosition[0x10 + y] -
                   JackalRam.SpriteVertScreenPosition[x]);
    if ((int8_t)dy < 0) { dy = (uint8_t)(0u - dy); }
    if (dy >= h) { continue; }
    id = (uint8_t)(JackalRam.SpriteObjectID[x] & 0x7Fu);
    if (id < 5u) {                            /* :4805-4806 步兵碾压 */
      Label1074(x, (uint8_t)y);
      return;
    }
    if (id == 0x16u) { powPickedUp(x, (uint8_t)y); return; }      /* POW 拾取 */
    if (id == 0x3Fu) {                        /* helipad（:4824-4825） */
      JackalRam.JeepAtHelipadDropoff[y] = 1;
      continue;
    }
    if (id == 0x52u) {                        /* 命星（:4896-4907；≥9 → :4894 JMP -- 循环） */
      subDespawnSprite(x);
      if ((y != 0 ? JackalRam.Jeep2LifeCount : JackalRam.Jeep1LifeCount) < 9u) {
        if (y != 0) { JackalRam.Jeep2LifeCount++; } else { JackalRam.Jeep1LifeCount++; }
        subInitiateSoundClip(EXTRA_LIFE_CLIP);   /* stub */
      }
      continue;                               /* JMP --（对象已消亡，下吉普碰撞不上） */
    }
    if (id == 0x50u) {                        /* 杀星（:4909-4928） */
      int8_t i;
      subDespawnSprite(x);
      for (i = 0x0F; i >= 0; i--) {
        uint8_t eid;
        if ((int8_t)JackalRam.SpriteObjectID[i] < 0) { continue; }  /* BMI + */
        eid = JackalRam.SpriteObjectID[i];
        JackalRam.SpriteState[i] = (uint8_t)(
            (JackalRam.SpriteState[i] & 0x80u) |
            (BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU)[eid & 0x7Fu] & 0x0Fu));
      }
      continue;                               /* :4928 JMP -- */
    }
    if (id == 0x51u) {                        /* 满武器星（:4886-4894） */
      subDespawnSprite(x);
      JackalRam.JeepMainWeapon[y] = 3;
      JackalRam.DifficultyBasedOnWeapon = 3;
      subInitiateSoundClip(WEAPON_UPGRADE_CLIP);   /* stub */
      continue;                               /* :4894 JMP -- */
    }
    if (id == 0x44u) {                        /* 电扶梯（:4821-4823） */
      JackalRam.JeepEscalatorEffectActive[y] = 1;
      continue;
    }
    /* lblJeepDeathByCollision（:4866-4872）：IFrame!=0 → --（下吉普）；
       HP bit7 置 → 吉普死但敌不死 → --（下吉普，耗血敌可双杀）；HP bit7 清 → Label1074 双杀 */
    if (JackalRam.JeepIFrameTimer[y] == 0) {
      JackalRam.SpriteState[0x10 + y] = 2;    /* JeepState=Collision */
      if ((int8_t)JackalRam.SpriteHealthHP[x] >= 0) {
        Label1074(x, (uint8_t)y);
        return;
      }
    }
  }
}
