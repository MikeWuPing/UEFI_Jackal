/* JACKAL_PPU 实现：$0770 队列机器（Bank7.ASM:3219-3267、2556-2598）。
   寄存器归约：PPUADDR 写 = 设当前地址；PPUDATA 写 = JackalVram[addr] 后 addr 按
   PPUCTRL bit2 步进（0→+1 水平 / 1→+32 垂直）；PPUCTRL 中途改写与结尾还原 NormalPPUCTRL
   在镜像模型中无副作用，只保留步进语义；PPUSTATUS 读（地址 latch 复位）无对应物。
   $08==$3F 的 palette 特例（:3222-3228 四次 PPUADDR 写）是渲染开启时改写 palette 的
   硬件抖动对策，随后地址总会被正常重写（:3238-3243），镜像模型中归约为无副作用。 */
#include "ram.h"
#include "ppu.h"
#include "jackal_assets.h"

uint8_t JackalVram[0x4000];

/* Bank7.ASM:3211 tblPPUUpdate_VRAMIncrementSetting：type1 水平 / type2 垂直(+32) / type3 水平 */
static const uint8_t tblPPUUpdate_VRAMIncrementSetting[3] = {0x00, 0x04, 0x00};

/* Bank7.ASM:2600-2720 tblPPUGraphics_PalleteUpdateAddress（102 项，$00-$65）。
   helipad 项的 +19/+38 是 ASM 原表的子流偏移（(tbl + 19) 等写法原样保留）。
   $3D-$64（40 项 Level6 最终 Boss 坦克逐行图形）未登记，置 NULL——Phase 6 补登记后替换。 */
static const uint8_t *const tblPPUGraphics_PalleteUpdateAddress[102] = {
  /* $00-$0C：标题/Continue/GameOver/记分 文本流与标题 palette */
  jackal_tblTitleScreenFlash1PlayerTextGraphicsUpdate, /* 00 */
  jackal_tblTitleScreenFlash1PlayerTextGraphicsUpdate, /* 01 */
  jackal_tblTitleScreenFlash2PlayerTextGraphicsUpdate, /* 02 */
  jackal_tblTitleScreenFlash1PlayerTextGraphicsUpdate, /* 03 */
  jackal_tblTitleScreenFlash1PlayerTextGraphicsUpdate, /* 04 */
  jackal_tblGameOverTextGraphicsUpdate,                /* 05 */
  jackal_tblTitleScreenPalette,                        /* 06 */
  jackal_tblContinueTextGraphicsUpdate,                /* 07 */
  jackal_tblContinueTextGraphicsUpdate,                /* 08 */
  jackal_tblContinueYesTextGraphicsUpdate,             /* 09 */
  jackal_tblContinueNoTextGraphicsUpdate,              /* 0A */
  jackal_tblSegueScreen1P_ScoreText,                   /* 0B */
  jackal_tblSegueScreen2P_ScoreText,                   /* 0C */
  /* $0D-$12：六关默认 palette */
  jackal_Level1DefaultPalette, jackal_Level2DefaultPalette, jackal_Level3DefaultPalette,
  jackal_Level4DefaultPalette, jackal_Level5DefaultPalette, jackal_Level6DefaultPalette,
  /* $13-$2A：helipad 闪光灯 palette（ASM 原表 +19/+38 子流偏移） */
  jackal_tblLevel1HelipadLightFlashPaletteUpdate,      /* 13 */
  jackal_tblLevel1HelipadLightFlashPaletteUpdate + 19, /* 14 */
  jackal_tblLevel1HelipadLightFlashPaletteUpdate + 38, /* 15 */
  jackal_tblLevel1HelipadLightFlashPaletteUpdate + 19, /* 16 */
  jackal_tblLevel2HelipadLightFlashPaletteUpdate,      /* 17 */
  jackal_tblLevel2HelipadLightFlashPaletteUpdate + 19, /* 18 */
  jackal_tblLevel2HelipadLightFlashPaletteUpdate + 38, /* 19 */
  jackal_tblLevel2HelipadLightFlashPaletteUpdate + 19, /* 1A */
  jackal_tblLevel3HelipadLightFlashPaletteUpdate,      /* 1B */
  jackal_tblLevel3HelipadLightFlashPaletteUpdate + 19, /* 1C */
  jackal_tblLevel3HelipadLightFlashPaletteUpdate + 38, /* 1D */
  jackal_tblLevel3HelipadLightFlashPaletteUpdate + 19, /* 1E */
  jackal_tblLevel4HelipadLightFlashPaletteUpdate,      /* 1F */
  jackal_tblLevel4HelipadLightFlashPaletteUpdate + 19, /* 20 */
  jackal_tblLevel4HelipadLightFlashPaletteUpdate + 38, /* 21 */
  jackal_tblLevel4HelipadLightFlashPaletteUpdate + 19, /* 22 */
  jackal_tblLevel5HelipadLightFlashPaletteUpdate,      /* 23 */
  jackal_tblLevel5HelipadLightFlashPaletteUpdate + 19, /* 24 */
  jackal_tblLevel5HelipadLightFlashPaletteUpdate + 38, /* 25 */
  jackal_tblLevel5HelipadLightFlashPaletteUpdate + 19, /* 26 */
  jackal_tblLevel6HelipadLightFlashPaletteUpdate,      /* 27 */
  jackal_tblLevel6HelipadLightFlashPaletteUpdate + 19, /* 28 */
  jackal_tblLevel6HelipadLightFlashPaletteUpdate + 38, /* 29 */
  jackal_tblLevel6HelipadLightFlashPaletteUpdate + 19, /* 2A */
  /* $2B-$34：Boss palette 更新 */
  jackal_tblLevel3BossBGPaletteUpdate,                 /* 2B */
  jackal_tblLevel5BossPaletteUpdate,                   /* 2C */
  jackal_tblLevel1BossSpritePaletteUpdate,             /* 2D */
  jackal_tblLevel6BuildingBossBGPaletteUpdate,         /* 2E */
  jackal_tblLevel6FinalBossTankDefaultPalette,         /* 2F */
  jackal_tblLevel6FinalBossTankInjuredColorPalette,    /* 30 */
  jackal_tblLevel6FinalBossTankNearDeathColorPalette,  /* 31 */
  jackal_tblLevel6FinalBossSkull_StripesOrangeBGPalette, /* 32 */
  jackal_tblLevel6FinalBossSkull_StripesGreenBGPalette,  /* 33 */
  jackal_tblLevel6FinalBossSkull_StripesGrayBGPalette,   /* 34 */
  /* $35-$3C：credits 抹除、POW 计数位置、segue/end/pow palette */
  jackal_tblEndOfGame_EraseCreditsLineOfText1,         /* 35 */
  jackal_tblEndOfGame_EraseCreditsLineOfText2,         /* 36 */
  jackal_tblLevelSeguePOWCountPosition_Player1,        /* 37 */
  jackal_tblLevelSeguePOWCountPosition_Player2,        /* 38 */
  jackal_tblLevelSegue_YEAH_Palette,                   /* 39 */
  jackal_tblLevelSeguePalette,                         /* 3A */
  jackal_tblEndOfGamePalette,                          /* 3B */
  jackal_tblPOWPortraitBGPalette,                      /* 3C */
  /* $3D-$64：Level6 最终 Boss 坦克逐行图形（正常 12 行 + 击毁 8 行，各左右两半）。
     未登记，Phase 6 补登记后替换 NULL。 */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  /* $65 */
  jackal_tblLevelSegue_HERE_BGPalette,
};

void subInGamePPUUpdates(void) {
  uint8_t y = 0;
  for (;;) {
    uint8_t type, ctrl, count, b;
    uint16_t addr;
    /* $08==$3F palette 特例：四次 PPUADDR 写为硬件抖动对策，镜像模型无副作用（见文件头） */
    type = JackalRam.PPUUpdateQueue[y];
    if (type == 0) {
      break;
    }
    ctrl = (uint8_t)((JackalRam.NormalPPUCTRL & 0x18u) |
                     tblPPUUpdate_VRAMIncrementSetting[type - 1]);
    y++;
    addr = (uint16_t)JackalRam.PPUUpdateQueue[y];  /* PPUADDR 高字节 */
    y++;
    addr = (uint16_t)((addr << 8) | JackalRam.PPUUpdateQueue[y]);  /* 低字节 */
    y++;
    if (type == 3) {  /* 带字节计数：写 count 个（0=256，6502 DEX/BNE 先减后判） */
      count = JackalRam.PPUUpdateQueue[y];
      y++;
      do {
        JackalVram[addr & 0x3FFFu] = JackalRam.PPUUpdateQueue[y];
        y++;
        addr = (uint16_t)(addr + ((ctrl & 0x04u) ? 32 : 1));
        count--;
      } while (count != 0);
    }
    /* type 1/2（及 type 3 count 耗尽后落入共享回路）：逐字节写到 $FF（:3248-3252） */
    for (;;) {
      b = JackalRam.PPUUpdateQueue[y];
      y++;
      if (b == 0xFF) {
        break;
      }
      JackalVram[addr & 0x3FFFu] = b;
      addr = (uint16_t)(addr + ((ctrl & 0x04u) ? 32 : 1));
    }
  }
  JackalRam.PPUUpdateQueue[0] = 0;   /* 清 type，防重放（:3252） */
  JackalRam.PPUGraphicsUpdateByteLength = 0;
  /* PPUCTRL ← NormalPPUCTRL 还原：镜像模型无寄存器，无对应动作 */
}

void Label152(uint8_t index) {
  uint8_t zp03 = 2;   /* $03 隐藏计数：前 2 个流字节（PPU 地址 hi/lo）即使隐藏也保留 */
  const uint8_t *stream = tblPPUGraphics_PalleteUpdateAddress[index & 0x7Fu];
  uint8_t y = 0;
  uint8_t x;
  /* :2557-2560 JSR +++++ 路径：队尾先写 type=1 */
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = 1;
  JackalRam.PPUGraphicsUpdateByteLength++;
  x = JackalRam.PPUGraphicsUpdateByteLength;
  for (;;) {
    uint8_t b = stream[y];
    y++;
    if (b == 0xFF) {
      break;                    /* $FF 直接收尾：不写终止符 */
    }
    if (b == 0xFE) {
      JackalRam.PPUUpdateQueue[x] = 0xFF;  /* $FE 补写 $FF 收尾 */
      x++;
      break;
    }
    JackalRam.PPUUpdateQueue[x] = b;
    if (index & 0x80u) {        /* 隐藏文本变体（:2571-2578） */
      if (zp03 == 0) {
        JackalRam.PPUUpdateQueue[x] = 0;
      } else {
        zp03--;
      }
    }
    x++;
    if (x == 0) {               /* X 回绕兜底：INX 后 Z 置位落入 $FE 收尾路径（:2579-2581） */
      JackalRam.PPUUpdateQueue[x] = 0xFF;
      x++;
      break;
    }
  }
  JackalRam.PPUGraphicsUpdateByteLength = x;
}

void Label757(void) {
  uint8_t x = JackalRam.PPUGraphicsUpdateByteLength;
  JackalRam.PPUUpdateQueue[x] = 0;
  x++;
  JackalRam.PPUGraphicsUpdateByteLength = x;
}

void subInsertPPUUpdateTerminator(uint8_t *x) {
  JackalRam.PPUUpdateQueue[*x] = 0xFF;
  (*x)++;
  JackalRam.PPUUpdateQueue[*x] = 0;
  JackalRam.PPUGraphicsUpdateByteLength = *x;
}
