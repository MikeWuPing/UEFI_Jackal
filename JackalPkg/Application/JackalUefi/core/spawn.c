/* core/spawn.c：Bank7.ASM 生成/消亡机器逐行翻译（:6553-6831、7098-7147）。
   三表（tblEnemyHitBoxIndex $F9CE/tblEnemyHealth $FA42/tblEnemyPoints_DeathState $FA96）
   在 Bank7 固定窗口，经 BankPtr(7,cpu) 读（同 scroll.c TBL_LEVEL_OBJECT_SPAWN_CPU 模式；
   地址经 ROM 字节签名定位核实：hitbox 尾 C4 C4 C4 85、points 尾 B2 B2 B2 12）。 */
#include "ram.h"
#include "bank.h"
#include "spawn.h"
#include "trace.h"

#define TBL_ENEMY_HITBOX_INDEX_CPU      0xF9CEu  /* :7849，0x54 项 */
#define TBL_ENEMY_HEALTH_CPU            0xFA42u  /* :7960，0x54 项 */
#define TBL_ENEMY_POINTS_DEATHSTATE_CPU 0xFA96u  /* :8065，0x54 项 */

uint8_t JackalSpawnZp[0x36];

/* SPAWN 串口追踪（调试设施，非 ASM 语义）：subSpawnObjectFromParent 成功生成后
   输出对象 ID——scroll.c traceSpawn 只覆盖 Label1257 块生成，生成器类子对象
   （$0A/$46/$4A/$4F 等）由本通道补齐，供 verify_phase4 对照。 */
static void traceSpawnFromParent(uint8_t id) {
  char buf[20];
  char *p = buf;
  const char *s;
  static const char hexd[] = "0123456789ABCDEF";
  if (JackalTraceHook == 0) {
    return;
  }
  s = "SPAWN id=";
  while (*s != 0) { *p++ = *s++; }
  *p++ = hexd[(id >> 4) & 0xF];
  *p++ = hexd[id & 0xF];
  *p = 0;
  JackalTraceHook(buf);
}

/* 三表查值（:6604-6616 / :6755-6767 公共段）：hitbox/HP/EnemyPoints（>>4），
   双吉普存活且分数非 0 时 EnemyPoints+1。 */
static void lookupEnemyTables(uint8_t x, uint8_t id) {
  uint8_t y = (uint8_t)(id & 0x7Fu);
  const uint8_t *hitbox = BankPtr(7, TBL_ENEMY_HITBOX_INDEX_CPU);
  const uint8_t *health = BankPtr(7, TBL_ENEMY_HEALTH_CPU);
  const uint8_t *points = BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU);
  JackalRam.SpriteHitboxShapeIndex[x] = hitbox[y];
  JackalRam.SpriteHealthHP[x] = health[y];
  JackalRam.EnemyPoints[x] = (uint8_t)(points[y] >> 4);
  if (JackalRam.EnemyPoints[x] != 0 &&
      JackalRam.Jeep1State != 0 && JackalRam.Jeep2State != 0) {
    JackalRam.EnemyPoints[x]++;
  }
}

void subClearSpriteVertSpeed(uint8_t x) {  /* :6701-6705 */
  JackalRam.SpriteVertSpeedLB[x] = 0;
  JackalRam.SpriteVertSpeedUB[x] = 0;
}

void subClearSpriteHorizSpeed(uint8_t x) {  /* :6695-6699 */
  JackalRam.SpriteHorizSpeedLB[x] = 0;
  JackalRam.SpriteHorizSpeedUB[x] = 0;
}

void subClearSpriteSpeed(uint8_t x) {  /* :6692（JSR vert 后落 horiz） */
  subClearSpriteVertSpeed(x);
  subClearSpriteHorizSpeed(x);
}

void subInitSpriteDataZERO(uint8_t x) {  /* :6669，无 RTS 落入 subClearSpriteSpeed */
  JackalRam.SpriteState[x] = 0;
  JackalRam.SpriteData1[x] = 0;
  JackalRam.SpriteData2[x] = 0;
  JackalRam.SpriteData3[x] = 0;
  JackalRam.SpriteData4[x] = 0;
  JackalRam.SpriteData5[x] = 0;
  JackalRam.SpriteData6[x] = 0;
  JackalRam.SpriteWhatDirectionToShoot[x] = 0;
  JackalRam.SpriteData8[x] = 0;
  JackalRam.SpriteWhichJeeptoAttack[x] = 0;
  JackalRam.SpriteTypeIndex[x] = 0;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
  JackalRam.Raw[0x6Au + x] = 0;                 /* SpriteAttributes,X（$006A 数组） */
  JackalRam.SpriteHorizScreenPositionSubPixel[x] = 0;
  JackalRam.SpriteVertScreenPositionSubPixel[x] = 0;
  subClearSpriteSpeed(x);
}

uint8_t fctGetNextOpenSpriteSlot(void) {  /* :6709 */
  int8_t x;
  for (x = 0x0F; x >= 0; x--) {
    if (JackalRam.SpriteObjectID[x] == 0) {
      subInitSpriteDataZERO((uint8_t)x);  /* BEQ 语义（A=0）→ 找到 */
      return (uint8_t)x;
    }
  }
  return 0xFF;
}

void subDespawnSprite(uint8_t x) {  /* :7120 */
  JackalRam.SpriteObjectID[x] = 0;
  JackalRam.SpriteState[x] = 0;
  JackalRam.SpriteTypeIndex[x] = 0;
  JackalRam.SpriteHitboxShapeIndex[x] = 0;
  JackalRam.SpriteHealthHP[x] = 0;
  JackalRam.SpriteGraphicsAttributes[x] = 0;
}

uint8_t subDespawnLesserObjects_Offscreen_ForHighPriorityObjects(void) {  /* :6645 */
  int8_t x;
  for (x = 0x0F; x >= 0; x--) {                      /* 第一级：子弹/火焰/导弹 或 屏外 */
    uint8_t id = JackalRam.SpriteObjectID[x];
    if ((int8_t)id < 0) { continue; }                /* BMI ++：MSB 防消亡 */
    if (id == 0x36u || id == 0x34u || id == 0x39u ||
        (int8_t)JackalRam.SpriteState[x] < 0) {
      subDespawnSprite((uint8_t)x);                  /* :6667 JSR 后顺序落入 init */
      subInitSpriteDataZERO((uint8_t)x);
      return (uint8_t)x;
    }
  }
  for (x = 0x0F; x >= 0; x--) {                      /* 第二级：任意低优先（ID bit7 清） */
    if ((int8_t)JackalRam.SpriteObjectID[x] >= 0) {
      subDespawnSprite((uint8_t)x);
      subInitSpriteDataZERO((uint8_t)x);
      return (uint8_t)x;
    }
  }
  subDespawnSprite(0);                               /* 槽 0 兜底（恒成功） */
  subInitSpriteDataZERO(0);
  return 0;
}

void subMoveSpriteToNextState(uint8_t x) {     /* :7130：ADC +1（不保 bit7） */
  JackalRam.SpriteState[x] = (uint8_t)(JackalRam.SpriteState[x] + 1u);
  if (JackalRam.SpriteObjectID[x] == 0) {
    subDespawnSprite(x);
  }
}
void subMoveSpriteToPreviousState(uint8_t x) { /* :7133：ADC -1 */
  JackalRam.SpriteState[x] = (uint8_t)(JackalRam.SpriteState[x] - 1u);
  if (JackalRam.SpriteObjectID[x] == 0) {
    subDespawnSprite(x);
  }
}
void subSetSpriteState(uint8_t x, uint8_t a) { /* :7138：保 bit7 */
  JackalRam.SpriteState[x] = (uint8_t)((JackalRam.SpriteState[x] & 0x80u) | a);
  if (JackalRam.SpriteObjectID[x] == 0) {
    subDespawnSprite(x);
  }
}

void subCheckSpriteDespawnIfOffscreen(uint8_t x) {  /* Label244 尾段 :7098-7115 */
  if ((int8_t)JackalRam.SpriteAbsoluteHorizPositionUB[x] < 0) {  /* 左出界 */
    subDespawnSprite(x);
    return;
  }
  if (JackalRam.SpriteAbsoluteHorizPositionUB[x] >= 2u) {        /* 右出界（地图 2 屏宽） */
    subDespawnSprite(x);
    return;
  }
  if (JackalRam.SpriteAbsoluteVertPositionUB[x] == 0) {
    return;                                          /* 在屏 */
  }
  if ((int8_t)JackalRam.SpriteAbsoluteVertPositionUB[x] < 0) {   /* below 分支 :7105 */
    if (JackalRam.SpriteVertScreenPosition[x] >= 0x40u) {
      subDespawnSprite(x);
    }
    return;
  }
  if (JackalRam.SpriteVertScreenPosition[x] < 0xC0u) {           /* above 分支 :7110 */
    subDespawnSprite(x);
  }
}

uint8_t subSpawnObjectFromParent(void) {  /* :6718-6805（NoOffset 变体由调用方清 $00-$03） */
  uint8_t parent = JackalSpawnZp[0x35];               /* $35：调用方 X */
  uint8_t lb, ub, vert, vertUb, slot;
  /* $04/$05 = AbsHoriz + $00/$01（16 位；UB ADC 不 CLC=用 LB 进位） */
  lb = (uint8_t)(JackalRam.SpriteAbsoluteHorizPositionLB[parent] + JackalSpawnZp[0x00]);
  ub = (uint8_t)(JackalRam.SpriteAbsoluteHorizPositionUB[parent] + JackalSpawnZp[0x01] +
                 (lb < JackalSpawnZp[0x00] ? 1u : 0u));
  vert = (uint8_t)(JackalRam.SpriteVertScreenPosition[parent] + JackalSpawnZp[0x02]);
  vertUb = (uint8_t)(JackalRam.SpriteAbsoluteVertPositionUB[parent] + JackalSpawnZp[0x03] +
                     (vert < JackalSpawnZp[0x02] ? 1u : 0u));
  slot = fctGetNextOpenSpriteSlot();
  if (slot == 0xFFu) {
    return 0xFF;                        /* lblNoAvailableSpriteSlotsFound：Y=$FF */
  }
  JackalRam.SpriteAbsoluteHorizPositionLB[slot] = lb;
  JackalRam.SpriteAbsoluteHorizPositionUB[slot] = ub;
  JackalRam.SpriteVertScreenPosition[slot] = vert;
  JackalRam.SpriteAbsoluteVertPositionUB[slot] = vertUb;
  JackalRam.SpriteObjectID[slot] = JackalSpawnZp[0x08];
  lookupEnemyTables(slot, JackalSpawnZp[0x08]);
  JackalRam.SpriteData4[slot] = JackalSpawnZp[0x09];
  JackalRam.SpriteData5[slot] = JackalSpawnZp[0x0A];
  JackalRam.SpriteData6[slot] = JackalSpawnZp[0x0B];
  JackalRam.SpriteWhatDirectionToShoot[slot] = JackalSpawnZp[0x0C];
  JackalRam.SpriteData8[slot] = JackalSpawnZp[0x0D];
  JackalRam.SpriteWhichJeeptoAttack[slot] = JackalSpawnZp[0x0E];
  JackalRam.SpriteGraphicsAttributes[slot] = (uint8_t)(JackalSpawnZp[0x0F] & 0x0Fu);
  JackalRam.Raw[0x6Au + slot] = (uint8_t)(JackalSpawnZp[0x0F] & 0xF0u);
  JackalRam.SpriteData1[slot] = JackalSpawnZp[0x10];
  JackalRam.SpriteData2[slot] = JackalSpawnZp[0x11];
  JackalRam.SpriteData3[slot] = JackalSpawnZp[0x12];
  traceSpawnFromParent(JackalSpawnZp[0x08]);
  return slot;
}

void Label1257(uint8_t x, const uint8_t *blk, uint8_t cursor) {  /* :6567-6625 */
  uint8_t xPos, id;
  JackalRam.LastSpawnedEnemyY_LB = JackalRam.CurrentLevelScreenSubPosition;  /* :6568 */
  JackalRam.LastSpawnedEnemyY_HB = JackalRam.CurrentLevelScreen;             /* :6570 */
  xPos = blk[(uint8_t)(cursor + 1u)];          /* :6574（$08 存参） */
  JackalRam.SpriteAbsoluteVertPositionUB[x] = 0;
  JackalRam.SpriteVertScreenPosition[x] = (int8_t)xPos < 0 ? 0xEFu : 0x00u;  /* bit7=底部 */
  /* :6585-6591：AbsHorizLB = X*4（两次 ASL），第二次进位入 UB */
  {
    uint16_t wide = (uint16_t)((uint16_t)xPos << 2);
    JackalRam.SpriteAbsoluteHorizPositionLB[x] = (uint8_t)wide;
    JackalRam.SpriteAbsoluteHorizPositionUB[x] = (uint8_t)(wide >> 8);
  }
  id = blk[(uint8_t)(cursor + 2u)];            /* :6593 对象 ID */
  JackalRam.SpriteObjectID[x] = id;
  /* :6595-6603：奇 SubPosition 且难度<3 → 抹 ID（偶位难度减负语义） */
  if ((JackalRam.CurrentLevelScreenSubPosition & 1u) != 0 &&
      JackalRam.DifficultyBasedOnWeapon < 3u) {
    JackalRam.SpriteObjectID[x] = 0;
  }
  lookupEnemyTables(x, JackalRam.SpriteObjectID[x]);   /* :6604-6624（ID=0 查 [0]，ASM 原样） */
  /* ASM 末尾 JMP Label978 回环：C 侧 scroll.c 调用点 continue */
}
