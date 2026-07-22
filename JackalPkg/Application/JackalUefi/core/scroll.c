/* 滚屏与列装载 + 生成表游走（Bank7.ASM 逐行翻译）。
   覆盖：subProcessScreenScrolling（:204-238）、Label975（:1764-1897）、
   Label888（:1899-1920）、Label893（:1922-2103）、Label911（:2104-2188）、
   Label978（:6427-6551）骨架、Label984（:3791-3810）。

   装载几何（Task 2.8 经机械仿真 + 渲染核对查实）：
   - ROM 垂直镜像（iNES flags6 bit0=1）= $2000/$2400 左右排列两张物理 nametable；
     关卡条带 16 个大块宽（512px）：左 8 块进 $2000 行、右 8 块进 $2400 行
     （Label893 中途 UB+4 的第二个 entry，:2093-2099）。
   - tblLevelLayoutScreenLoadIndex 倒序：$4B=0 命中表尾页（Level 1 = page 11），
     即关卡底部；滚屏向上时 $4B 递增、页码递减。
   - Label911 的邻页读（±$80）越表时命中 bank 窗口内后续 ROM 字节（Level 1 底部
     边界读进 tblLevel1Layout32x32Definition）——必须按 BankPtr 窗口读，
     不能只读提取后的 C 数组（golden 生成器 gen_golden.py 同语义，双侧同步）。
   - 每帧 GPM2 加 8（:3479-3484），(sub&7)==0 触发一次装载；首屏 30 行需 31 帧
     （第 30 次装载发生在 screen=1 转换帧，InitializeLevel 同帧）。
   - 行地址 $48 每装载 -$20（行 29→0），回绕 $23A0；属性地址 $4A 每 4 次 -8。
   - 属性仅奇数 sub-row（$4D&1）写（Label911 入口 :2104），16 字节两 entry（UB+4）；
     同一属性行会被相邻两个奇数 sub-row 写两次，后写覆盖（ASM 原样）。 */
#include "ram.h"
#include "bank.h"
#include "ppu.h"
#include "sound_stub.h"
#include "trace.h"
#include "scroll.h"
#include "spawn.h"
#include "jackal_assets.h"

#define WEEP_WEEP_BOSS_MUSIC_F0_CLIP 0x23u  /* WeepWeepBossMusic_F0SoundClip（Sound.ASM:47） */
#define PAUSE_GAME_SOUND_CLIP        0x58u  /* PauseGameSoundClip（Sound.ASM:105） */
#define START_BUTTON                 0x10u  /* StartButton（Global.ASM:20） */

/* tblLevelBGTileOffset（:2246，Bank7 $CFEE，.PAD 未登记 → BankPtr(7) 直读） */
#define TBL_LEVEL_BG_TILE_OFFSET_CPU 0xCFEEu
/* LevelCollision_* 六表（:2205-2220，Bank7 $CFBE 起，每关 8 字节：前 3 公共、后 3 关特定） */
#define LEVEL_COLLISION_BASE_CPU     0xCFBEu
/* tblLevelObjectSpawnAddress（:6630，Bank7 $F1E0，六关 dw → Bank6 生成头表；
   地址经 Label978 序言机器码（$F081: B9 E0 F1 = LDA $F1E0,Y）核实——
   ASM :6553 的 ;$F15D 注释标的是其后 tblF2LevelIndex，非本表） */
#define TBL_LEVEL_OBJECT_SPAWN_CPU   0xF1E0u

/* Bank7.ASM:2192-2195 */
static const uint8_t tblLoadLevelBGRow1_2[4] = {0xF0, 0xF0, 0x0F, 0x0F};
static const uint8_t tblLoadLevelBGRow3_4[4] = {0x0F, 0x0F, 0xF0, 0xF0};
/* Bank7.ASM:6557-6565（tblF2LevelIndex $F15D、tblLevelIndexForPPUGraphics_
   PalleteUpdateAddress $F163，均已对照 ROM 字节核实） */
static const uint8_t tblF2LevelIndex[6] = {0x00, 0x00, 0x01, 0x07, 0x02, 0x06};
static const uint8_t tblLevelIndexForPPUGraphics_PalleteUpdateAddress[6] = {0x2D, 0x00, 0x2B, 0x00, 0x2C, 0x2E};

/* 六关 dw 表（Bank7）第 i 项的 16 位值 */
static uint16_t dwAt(const uint8_t *tbl12, uint8_t i) {
  return (uint16_t)(tbl12[i * 2u] | ((uint16_t)tbl12[i * 2u + 1u] << 8));
}

void subProcessScreenScrolling(void) {  /* :204-238 */
  if (JackalRam.Level6BossTankScroll_Next != 0) {
    /* Level 6 boss 裂屏：PPUADDR/PPUSTATUS/PPUSCROLL 硬件写与长延时循环在镜像模型
       无对应物（滚屏值经 $FC/$FD 由 render 消费），仅 Current 跟进有状态 */
    JackalRam.Level6BossTankScroll_Current = JackalRam.Level6BossTankScroll_Next;
  }
  /* :231-237 PPUSTATUS 读、PPUSCROLL/PPUCTRL 写：镜像模型无寄存器副作用 */
}

/* ---------------------------------------------------------------- Label888/893/911 */

/* Label893（含 Label888 快照与 Label911 属性段）：一次上行/下行装载。
   调用前 $47-$4D/$4F/$05 已按方向更新；本函数读 zero page 快照（ASM 的 Label888）。 */
static void label893(void) {
  const uint8_t level = JackalRam.CurrentLevel;
  const uint8_t bank = jackal_tblLevelLayoutBank[level];
  /* Label888（:1899-1920）：$08-$0E 与 $DA-$DC 快照 */
  uint8_t z08 = JackalRam.Zp47;
  uint8_t z09 = JackalRam.Zp48;
  const uint8_t z0A = JackalRam.Zp4B;
  uint8_t z0B = (uint8_t)(JackalRam.Zp4C << 4);
  const uint8_t z0C = JackalRam.Zp4D;
  uint8_t z0D = JackalRam.Zp49;
  const uint8_t z0E = JackalRam.Zp4A;
  uint8_t zDB = z0B;
  const uint8_t zDC = z0C;
  /* Label893（:1922-1937）：($C8/$C9)=ScreenLoadIndex、($CA/$CB)=ScreenLayout、
     ($CE/$CF)=LargeTilePaletteData 三表基址（Bank7 dw 表解码，bank 窗口读） */
  const uint16_t idxCpu = dwAt(jackal_tblLevelLayoutScreenLoadIndex, level);
  const uint16_t layCpu = dwAt(jackal_tblLevelLayoutData, level);
  const uint16_t palCpu = dwAt(jackal_tblLevelLayoutLargeTilePaletteData, level);
  /* :1951-1966：($00/$01) = layBase + index[$4B]*128（7 位移入高字节） */
  const uint16_t pageCpu = (uint16_t)(layCpu + BankPtr(bank, idxCpu)[z0A] * 128u);
  uint8_t z0F = 0x10;
  uint8_t z11 = 0x03;   /* ($10/$11) 碰撞写入指针：$0300，半程切 $0400（:2086） */
  const uint8_t z10 = 0x00;

  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 1;   /* :1941 */
  JackalRam.PPUGraphicsUpdateByteLength++;
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = z08; /* :1944 UB */
  JackalRam.PPUGraphicsUpdateByteLength++;
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = z09; /* :1947 LB */
  JackalRam.PPUGraphicsUpdateByteLength++;

  for (;;) {   /* Label913（:1967） */
    uint8_t tileId = BankPtr(bank, pageCpu)[z0B];   /* :1971 LDA ($00),Y */
    uint16_t defCpu;
    uint8_t zY;
    uint8_t z04;
    /* :1975-1988：tileId >= tblLevelBGTileOffset[level] → 减偏移并切关卡专属 32x32 表
       （Y=level*2 字节偏移=dw 表项 [level]；否则 Y=0 用公共表（Level 1 数据）。
       修正：原误传 level*2 给 dwAt（其内部已 *2）——level!=0 时错读成 [level*2]，
       L2 曾被错用 L1/L3 的 def 表（Task 6.5 L2 首屏垃圾根因） */
    if (tileId >= BankPtr(7, TBL_LEVEL_BG_TILE_OFFSET_CPU)[level]) {
      tileId = (uint8_t)(tileId - BankPtr(7, TBL_LEVEL_BG_TILE_OFFSET_CPU)[level]);
      defCpu = dwAt(jackal_tblLevelLayout32x32Definition, level);
    } else {
      defCpu = dwAt(jackal_tblLevelLayout32x32Definition, 0);
    }
    zY = (uint8_t)(z0C * 4u);   /* :2005-2008 */
    for (z04 = 4; z04 != 0; z04--) {   /* Label910（:2012） */
      const uint8_t tile = BankPtr(bank, defCpu)[(uint16_t)tileId * 16u + zY];
      uint8_t cb;
      uint8_t shift;
      uint16_t caddr;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = tile;  /* :2013 */
      JackalRam.PPUGraphicsUpdateByteLength++;
      /* :2018-2047：碰撞分级（LevelCollision_* 六阈值，$CFBE+level*8） */
      {
        const uint8_t *ct = BankPtr(7, (uint16_t)(LEVEL_COLLISION_BASE_CPU + level * 8u));
        if (tile < ct[0]) { cb = 0; }
        else if (tile < ct[1]) { cb = 1; }
        else if (tile < ct[2]) { cb = 2; }
        else if (tile < ct[3]) { cb = 0; }
        else if (tile < ct[4]) { cb = 1; }
        else if (tile < ct[5]) { cb = 2; }
        else { cb = 3; }
      }
      /* :2049-2069：按 $04 余数定移位（4→6、3→4、2→2、1→0），首写先清零再 ORA */
      shift = (uint8_t)((z04 - 1u) * 2u);
      caddr = (uint16_t)(((uint16_t)z11 << 8) + z10 + JackalRam.Raw[0x05]);
      if (z04 == 4u) {
        JackalRam.Raw[caddr] = 0;
      }
      JackalRam.Raw[caddr] = (uint8_t)(JackalRam.Raw[caddr] | (uint8_t)(cb << shift));
      zY++;
    }
    JackalRam.Raw[0x05]++;                            /* :2075 */
    z0F--;                                            /* :2076 */
    if (z0F == 0) {
      break;                                          /* → Label911 */
    }
    if (z0F == 8) {   /* :2078-2100：半程换 entry（碰撞回退 8、$0400、UB+4=另一 nametable） */
      JackalRam.Raw[0x05] = (uint8_t)(JackalRam.Raw[0x05] - 8u);
      z11 = 4;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 0xFF;
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 1;
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = (uint8_t)(z08 + 4u);
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = z09;
      JackalRam.PPUGraphicsUpdateByteLength++;
    }
    z0B++;                                            /* :2102 */
  }

  /* Label911（:2104-2188）：仅奇数 sub-row（$DC&1）写属性 */
  if ((zDC & 1u) != 0) {
    z11 = 0x10;
    for (;;) {
      uint8_t i;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 0xFF;  /* :2114 */
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 3;     /* type 3 */
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = z0D;
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = z0E;
      JackalRam.PPUGraphicsUpdateByteLength++;
      JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 8;     /* count */
      JackalRam.PPUGraphicsUpdateByteLength++;
      for (i = 0; i < 8u; i++) {   /* :2130-2159 */
        const uint8_t cur = BankPtr(bank, palCpu)[BankPtr(bank, pageCpu)[zDB]];
        /* :2137-2179：邻页地址 ±$80（上行 $DC<2 加、$DC>=2 同页；下行反之）——
           越表读命中 bank 窗口后续 ROM（头注几何），16 位进位/借位随加减退 */
        uint16_t nCpu = pageCpu;
        if (JackalRam.ScreenScrolledUp_Down == 0) {
          if (zDC < 2u) { nCpu = (uint16_t)(pageCpu + 0x80u); }
        } else {
          if (zDC >= 2u) { nCpu = (uint16_t)(pageCpu - 0x80u); }
        }
        {
          const uint8_t nxt = BankPtr(bank, palCpu)[BankPtr(bank, nCpu)[zDB]];
          JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] =
              (uint8_t)((cur & tblLoadLevelBGRow1_2[zDC]) | (nxt & tblLoadLevelBGRow3_4[zDC]));
          JackalRam.PPUGraphicsUpdateByteLength++;
        }
        zDB++;
        z11--;
        if (z11 == 0) {
          break;
        }
        if (z11 == 8) {   /* :2162-2168：后半换 entry（UB+4） */
          z0D = (uint8_t)(z0D + 4u);
          break;          /* 回到外层循环写新 entry 头 */
        }
      }
      if (z11 == 0) {
        break;
      }
    }
  }
  /* :2182-2187 ++++：收尾 $FF、0（type 终止） */
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 0xFF;
  JackalRam.PPUGraphicsUpdateByteLength++;
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 0;
  JackalRam.PPUGraphicsUpdateByteLength++;
}

/* ---------------------------------------------------------------- Label975 */

void Label975(void) {  /* :1764 */
  JackalRam.ScreenHorizontalScrollPosition_PPU = JackalRam.ScreenLeftScrollPosition;  /* :1765 */
  if (JackalRam.CurrentLevelScreenSubPosition == JackalRam.PreviousLevelScreenSubposition) {
    return;   /* :1769 无滚动 */
  }
  /* :1770-1779：VScroll = (sub==0) ? 0 : ($FF-sub) - $0F（SEC SBC，借位已置） */
  if (JackalRam.CurrentLevelScreenSubPosition == 0) {
    JackalRam.ScreenVerticalScrollPosition_PPU = 0;
  } else {
    JackalRam.ScreenVerticalScrollPosition_PPU =
        (uint8_t)((JackalRam.CurrentLevelScreenSubPosition ^ 0xFFu) - 0x0Fu);
  }
  JackalRam.PreviousLevelScreenSubposition = JackalRam.CurrentLevelScreenSubPosition;  /* :1780 */
  if (JackalRam.ScreenScrolledUp_Down == 0) {   /* :1782 BEQ Label886（上行） */
    if ((JackalRam.CurrentLevelScreenSubPosition & 7u) != 0) {   /* Label886（:1759） */
      return;
    }
  } else {   /* 下行：:1784-1786 */
    if ((JackalRam.CurrentLevelScreenSubPosition & 7u) != 7u) {
      return;
    }
  }
  /* :1787 装载入口：($10/$11)=$0300 碰撞指针 */
  JackalRam.Raw[0x10] = 0;
  JackalRam.Raw[0x11] = 3;
  if (JackalRam.ScreenScrolledUp_Down != 0) {
    goto scrolledDown;
  }
  /* ---- 上行分支（:1847-1897） ---- */
  JackalRam.Zp4F = (uint8_t)(JackalRam.Zp4F - 8u);      /* :1847 */
  JackalRam.Raw[0x05] = JackalRam.Zp4F;                 /* :1851 */
  /* :1852-1865：sub==0 或 (sub-$10)&$1F==0 → 属性地址 -8，<$C0 回绕 $F8 */
  if (JackalRam.CurrentLevelScreenSubPosition == 0 ||
      ((uint8_t)(JackalRam.CurrentLevelScreenSubPosition - 0x10u) & 0x1Fu) == 0) {
    JackalRam.Zp4A = (uint8_t)(JackalRam.Zp4A - 8u);
    if (JackalRam.Zp4A < 0xC0u) {
      JackalRam.Zp4A = 0xF8;
    }
  }
  /* :1866-1878：tile 地址 -$20，借位回绕 $23A0 */
  {
    const uint8_t borrow = (JackalRam.Zp48 < 0x20u) ? 1u : 0u;
    JackalRam.Zp48 = (uint8_t)(JackalRam.Zp48 - 0x20u);
    if (borrow) {
      JackalRam.Zp47--;
      if (JackalRam.Zp47 == 0x1Fu) {
        JackalRam.Zp47 = 0x23;
        JackalRam.Zp48 = 0xA0;
      }
    }
  }
  /* :1879-1897：sub-row/条带/页计数 */
  JackalRam.Zp4D++;
  if (JackalRam.Zp4D == 4u) {
    if (JackalRam.Zp4C == 7u) {
      JackalRam.Zp4B++;
      JackalRam.Zp4D = 2;
      JackalRam.Zp4C = 0;
    } else {
      JackalRam.Zp4C++;
      JackalRam.Zp4D = 0;
    }
  }
  label893();
  return;

scrolledDown:   /* ---- 下行分支（:1793-1846） ---- */
  JackalRam.Zp4F = (uint8_t)(JackalRam.Zp4F + 8u);      /* :1793 */
  JackalRam.Raw[0x05] = (uint8_t)(JackalRam.Zp4F + 0xE8u);  /* :1797 */
  JackalRam.Zp4B--;                                     /* :1800 */
  /* :1801 JSR Label888 快照点——ASM 在 DEC $4B 后即取快照（z4B-1、迁移前
     z4C/z4D、更新前 tile/属性地址），装载在分支末执行但用此快照；初版 C 把
     快照放在 label893 入口（全部迁移之后），下行装载源页/源行/地址全错，
     往返滚动后碰撞/nametable 出现错位行（用户报告水陆错位/炮台泡水根因）。
     余下指针更新（:1802-1845）只写活动 zp、不影响快照，对下一帧生效。 */
  label893();
  /* :1802-1810：(sub&$1F)==$0F → 属性地址 +8，==0 回绕 $C0 */
  if ((JackalRam.CurrentLevelScreenSubPosition & 0x1Fu) == 0x0Fu) {
    JackalRam.Zp4A = (uint8_t)(JackalRam.Zp4A + 8u);
    if (JackalRam.Zp4A == 0u) {
      JackalRam.Zp4A = 0xC0;
    }
  }
  JackalRam.Zp4B++;                                     /* :1813 */
  /* :1814-1829：tile 地址 +$20，进位 INC $47；无进位且 $48==$C0、$47==$23 → 回绕 $2000 */
  {
    const uint16_t sum = (uint16_t)(JackalRam.Zp48 + 0x20u);
    JackalRam.Zp48 = (uint8_t)sum;
    if (sum >= 0x100u) {
      JackalRam.Zp47++;
    } else if (JackalRam.Zp48 == 0xC0u && JackalRam.Zp47 == 0x23u) {
      JackalRam.Zp48 = 0x00;
      JackalRam.Zp47 = 0x20;
    }
  }
  /* :1830-1846：sub-row/条带倒数 */
  if (JackalRam.Zp4D == 2u && JackalRam.Zp4C == 0u) {
    JackalRam.Zp4D = 3;
    JackalRam.Zp4C = 7;
    JackalRam.Zp4B--;
  } else {
    JackalRam.Zp4D--;
    if ((int8_t)JackalRam.Zp4D < 0) {
      JackalRam.Zp4D = 3;
      JackalRam.Zp4C--;
    }
  }
}

/* ---------------------------------------------------------------- Label978 骨架 */
/* 生成/消亡机器（fctGetNextOpenSpriteSlot/subDespawnLesserObjects/Label1257）
   Task 3.5 起实装在 spawn.c（Bank7.ASM:6553-6831 逐行翻译）。 */

/* SPAWN 串口追踪（调试设施，非 ASM 语义）：真实生成前输出对象 ID，
   供 verify_phase3 对照生成表序列。 */
static void traceSpawn(const uint8_t *blk, uint8_t cursor) {
  char buf[20];
  char *p = buf;
  const char *s;
  uint8_t id;
  static const char hexd[] = "0123456789ABCDEF";
  if (JackalTraceHook == 0) {
    return;
  }
  id = blk[(uint8_t)(cursor + 2u)];
  s = "SPAWN id=";
  while (*s != 0) { *p++ = *s++; }
  *p++ = hexd[(id >> 4) & 0xF];
  *p++ = hexd[id & 0xF];
  *p = 0;
  JackalTraceHook(buf);
}

/* F1 分支调用点（:6519）：subDisablePaletteFlash_LoadDefaultPalette（:3544-3556）。
   与 game_control.c 内 GPM1 用的静态 subLoadLevelDefaultPalette 同源逻辑，
   此处按调用点独立展开（$FF 计时关闭 + Label152($0D+level)）。 */
static void subDisablePaletteFlash_LoadDefaultPalette(void) {
  JackalRam.LevelHelipadLightFlashTimer = 0xFF;
  Label152((uint8_t)(0x0Du + JackalRam.CurrentLevel));
}

void Label978(void) {  /* :6427 */
  uint16_t hdrCpu;
  const uint8_t *hdr;
  uint16_t blkCpu;
  const uint8_t *blk;
  /* :6428-6434 门：上次生成位置在下游（大于当前）则不查新生成 */
  if (JackalRam.CurrentLevelScreen < JackalRam.LastSpawnedEnemyY_HB) {
    return;
  }
  if (JackalRam.CurrentLevelScreen == JackalRam.LastSpawnedEnemyY_HB &&
      JackalRam.CurrentLevelScreenSubPosition < JackalRam.LastSpawnedEnemyY_LB) {
    return;
  }
  /* :6435-6449：($02/$03)=tblLevelObjectSpawnAddress[level]（Bank7 $F15D dw），
     ($00/$01)=头表[screen*2]（Bank6 窗口，生成块 cpu） */
  hdrCpu = dwAt(BankPtr(7, TBL_LEVEL_OBJECT_SPAWN_CPU), JackalRam.CurrentLevel);
  hdr = BankPtr(6, hdrCpu);
  blkCpu = (uint16_t)(hdr[JackalRam.CurrentLevelScreen * 2u] |
                      ((uint16_t)hdr[JackalRam.CurrentLevelScreen * 2u + 1u] << 8));
  blk = BankPtr(6, blkCpu);
  for (;;) {   /* 各分支尾部 JMP Label978 回环 */
    const uint8_t y = blk[JackalRam.SpawnBlockIndex];   /* :6451 LDA ($00),Y */
    if (y == 0xEFu) {   /* EF_LastObjectLoadedForThisScreen（:6489） */
      if (JackalRam.CurrentLevelScreenSubPosition != 0xEFu) {
        return;
      }
      JackalRam.SpawnBlockIndex = 0;
      return;
    }
    if (y == 0xF0u) {   /* F0_EndOfLevelScreenScrollObject（:6496） */
      if (blk[(uint8_t)(JackalRam.SpawnBlockIndex + 1u)] != JackalRam.CurrentLevelScreenSubPosition) {
        return;
      }
      subStopMusic();
      subInitiateSoundClip(WEEP_WEEP_BOSS_MUSIC_F0_CLIP);
      JackalRam.ScreenScrollingForF0ToBoss = 1;
      JackalRam.SpawnBlockIndex = (uint8_t)(JackalRam.SpawnBlockIndex + 2u);
      JackalRam.LastSpawnedEnemyY_LB = JackalRam.CurrentLevelScreenSubPosition;
      JackalRam.LastSpawnedEnemyY_HB = JackalRam.CurrentLevelScreen;
      continue;
    }
    if (y == 0xF1u) {   /* F1_ReloadDefaultLevelPallete（:6514） */
      if (blk[(uint8_t)(JackalRam.SpawnBlockIndex + 1u)] != JackalRam.CurrentLevelScreenSubPosition) {
        return;
      }
      subDisablePaletteFlash_LoadDefaultPalette();
      JackalRam.SpawnBlockIndex = (uint8_t)(JackalRam.SpawnBlockIndex + 2u);
      JackalRam.LastSpawnedEnemyY_LB = JackalRam.CurrentLevelScreenSubPosition;
      JackalRam.LastSpawnedEnemyY_HB = JackalRam.CurrentLevelScreen;
      continue;
    }
    if (y == 0xF2u) {   /* F2_LoadNewPalette_Graphics（:6528） */
      uint8_t idx;
      if (blk[(uint8_t)(JackalRam.SpawnBlockIndex + 1u)] != JackalRam.CurrentLevelScreenSubPosition) {
        return;
      }
      JackalRam.PPUGraphicsUpdateComplete = 0;
      JackalRam.PPUGraphicsUpdateTableIndex = tblF2LevelIndex[JackalRam.CurrentLevel];
      idx = tblLevelIndexForPPUGraphics_PalleteUpdateAddress[JackalRam.CurrentLevel];
      if (idx != 0) {
        Label152(idx);
      }
      JackalRam.SpawnBlockIndex = (uint8_t)(JackalRam.SpawnBlockIndex + 2u);
      JackalRam.LastSpawnedEnemyY_LB = JackalRam.CurrentLevelScreenSubPosition;
      JackalRam.LastSpawnedEnemyY_HB = JackalRam.CurrentLevelScreen;
      continue;
    }
    if (y != JackalRam.CurrentLevelScreenSubPosition) {   /* :6460 回滚防重生成 */
      return;
    }
    /* :6462-6466：普通对象 3 字节一项，游标先存 $08 再 +3 */
    {
      const uint8_t z08 = JackalRam.SpawnBlockIndex;
      uint8_t slot = fctGetNextOpenSpriteSlot();
      JackalRam.SpawnBlockIndex = (uint8_t)(JackalRam.SpawnBlockIndex + 3u);
      if (slot != 0xFFu) {                  /* :6467 BEQ ++ → Label1257 */
        traceSpawn(blk, z08);
        Label1257(slot, blk, z08);
        continue;
      }
      /* :6469-6475：高优先级（对象 ID bit7）尝试腾槽（恒成功，槽 0 兜底） */
      if ((int8_t)blk[(uint8_t)(z08 + 2u)] < 0) {
        slot = subDespawnLesserObjects_Offscreen_ForHighPriorityObjects();
        traceSpawn(blk, z08);
        Label1257(slot, blk, z08);
        continue;
      }
      /* :6476-6484：槽满放弃——扫过同 Y 余项后返回（原游戏丢弃语义） */
      while (blk[JackalRam.SpawnBlockIndex] == JackalRam.CurrentLevelScreenSubPosition) {
        JackalRam.SpawnBlockIndex = (uint8_t)(JackalRam.SpawnBlockIndex + 3u);
      }
      return;
    }
  }
}

/* ---------------------------------------------------------------- Label984 */

void Label984(void) {  /* :3791 */
  if ((uint8_t)(JackalRam.TitleScreenMode | JackalRam.Raw[0x26] |
                JackalRam.ScreenTransitionTimer) != 0) {
    return;   /* $26 从不被写（ASM 注释），保留 ORA 结构 */
  }
  if (JackalRam.GamePaused == 0) {
    if ((JackalRam.JeepControlsInput1Frame[0] & START_BUTTON) == 0) {
      return;
    }
    JackalRam.GamePaused = 1;
    subInitiateSoundClip(PAUSE_GAME_SOUND_CLIP);   /* :3803 JMP subInitiateSoundClip */
    return;
  }
  if ((JackalRam.JeepControlsInput1Frame[0] & START_BUTTON) != 0) {
    JackalRam.GamePaused = 0;
  }
}
