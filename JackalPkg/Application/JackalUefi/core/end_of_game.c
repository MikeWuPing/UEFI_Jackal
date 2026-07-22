/* core/end_of_game.c：GCS9 结局逐行翻译（Bank0:2329-2680 + 文本表 $B357/$B35B/$B387）。
   12 子状态：横滚收拢 $80 + 三角标闪烁 → congrats 文本 → EOG 场景（夕阳/孤兵/heli）
   → credits 流 → Start → 回 Level 1（ContinuesUsed=$83 HARD 模式）。
   Label161（Bank7:2366-2379）为 EOG 场景装载（pattern 9 + Label925($10) + palette $3B）。
   文本表经 BankPtr(0) 读；零页借用映射见 end_of_game.h 头注。 */
#include "end_of_game.h"
#include "ram.h"
#include "bank.h"
#include "ppu.h"
#include "loader.h"
#include "sound_stub.h"
#include "game_control.h"

#define STAGE_COMPLETE_CLIP      0x4Cu   /* StageComplete_LevelSegue_SoundClip */
#define END_OF_GAME_HELI_CLIP    0x28u   /* EndOfGameHelicopterSoundClip */
#define GAME_COMPLETE_MUSIC      0x54u   /* GameCompleteMusic */
#define MISSION_TEXT_CLIP        0x2Eu   /* Level6BossDefeated_MissionAccomplishedTextSoundClip */
#define EXPLOSION_CLIP           0x17u   /* MainWeaponExplosionOnEnemy */

/* Bank0 文本表（ROM 签名定位核实） */
#define TBL_END_OF_GAME_TEXT_CPU        0xB357u  /* dw：congrats/credits 头指针 */
#define TBL_CONGRATS_MESSAGE_CPU        0xB35Bu
#define TBL_ENDING_CREDITS_TEXT_CPU     0xB387u

/* 零页借用别名（结局上下文，见头注） */
#define EOG_TextPtrLB    JackalRam.Raw[0x50]
#define EOG_TextPtrUB    JackalRam.Raw[0x52]
#define EOG_LineAddrUB   JackalRam.Raw[0x56]
#define EOG_LineAddrLB   JackalRam.Raw[0x58]
#define EOG_TextCursor   JackalRam.Raw[0x5A]
#define EOG_FrameTimer   JackalRam.Raw[0x54]
#define EOG_FlashCount   JackalRam.Raw[0x60]

/* ---------------------------------------------------------------- 辅助例程 */

/* fctGetEndOfGameTextHeaderAddress（:2680-2685）：$50/$52 = tblEndOfGameText[X] */
static void fctGetEndOfGameTextHeaderAddress(uint8_t x) {
  const uint8_t *tbl = BankPtr(0, TBL_END_OF_GAME_TEXT_CPU);
  EOG_TextPtrLB = tbl[x];
  EOG_TextPtrUB = tbl[(uint8_t)(x + 1u)];
}

/* subEndOfGameSunShimmer（:2687-2697）：RNG&1 → 夕阳 TypeIndex++（$C4-$C7 循环） */
static void subEndOfGameSunShimmer(void) {
  if ((JackalRam.RNG_INCEveryFrame & 1u) == 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[1]++;
  if (JackalRam.SpriteTypeIndex[1] >= 0xC8u) {
    JackalRam.SpriteTypeIndex[1] = 0xC4;
  }
}

/* subEndOfGame_FinalBossTank_CheckForScroll_FlashComplete（:2699-2708）：
   $066F 每 2 帧倒数；返回 A=1（计时中）/0（完）。:2706 注：返 1 后还有多余 RTS（原样） */
static uint8_t subEndOfGame_FinalBossTank_CheckForScroll_FlashComplete(void) {
  if (JackalRam.Raw[0x066F] == 0) {
    return 0;
  }
  if ((JackalRam.RNG_INCEveryFrame & 1u) != 0) {
    JackalRam.Raw[0x066F]--;
  }
  return 1;
}

/* Label161（Bank7:2366-2379）：EOG 场景装载（bank 暂存还原无副作用） */
static void Label161(void) {
  subLoadNewPatternTable(9);        /* End of game index（scene[9]=$C4F7） */
  Label925(0x10);                   /* tblEndOfGameScreen_SunsetNametable */
  Label152(0x3B);                   /* tblEndOfGamePalette */
}

/* ---------------------------------------------------------------- 状态 0-3 */

static void EndOfGameState8(void);   /* :2554（State2 :2438 先于定义 JSR 引用） */

static void EndOfGameState0(void) {  /* :2346-2372：初始化 */
  subEraseInGameJeepData();
  JackalRam.SpriteTypeIndex[0x10] = 0;      /* Jeep1TypeIndex：无吉普可视 */
  JackalRam.SpriteTypeIndex[0x11] = 0;
  subInitiateSoundClip(STAGE_COMPLETE_CLIP);   /* stub */
  JackalRam.Raw[0x066F] = 0xF0;
  fctGetEndOfGameTextHeaderAddress(0);         /* X=0：congrats 头 */
  EOG_LineAddrUB = BankPtr(0, TBL_CONGRATS_MESSAGE_CPU)[0];   /* $22 */
  EOG_LineAddrLB = BankPtr(0, TBL_CONGRATS_MESSAGE_CPU)[1];   /* $BA */
  EOG_TextCursor = 0x02;
  JackalRam.Raw[0x5E] = 0x33;               /* 三角标绿 palette */
  EOG_FlashCount = 0;
  EOG_FrameTimer = 0x60;
  JackalRam.ControlSubState++;
}

static void EndOfGameState1(void) {  /* :2374-2430：横滚收拢 + 三角标闪烁 */
  uint8_t ppuDone;
  int8_t s;
  (void)subEndOfGame_FinalBossTank_CheckForScroll_FlashComplete();
  /* :2377-2378 注：返回值从不检查（仅计时副作用），ASM 原样 */
  /* PPU 横滚趋近 $80（>$80 净 -1、<$80 +1；==$80 → INY 标记完成） */
  ppuDone = (JackalRam.ScreenHorizontalScrollPosition_PPU == 0x80u);
  if (!ppuDone) {
    if (JackalRam.ScreenHorizontalScrollPosition_PPU > 0x80u) {
      JackalRam.ScreenHorizontalScrollPosition_PPU--;
    } else {
      JackalRam.ScreenHorizontalScrollPosition_PPU++;
    }
  }
  /* 坦克滚动未到 $80：趋近 + $4B 跟随（:2394-2405）并 RTS */
  if (JackalRam.Level6BossTankScroll_Next != 0x80u) {
    if (JackalRam.Level6BossTankScroll_Next > 0x80u) {
      JackalRam.Level6BossTankScroll_Next--;
    } else {
      JackalRam.Level6BossTankScroll_Next++;
    }
    for (s = 0x0F; s >= 0; s--) {
      if (JackalRam.SpriteObjectID[s] == 0x4Bu) {
        JackalRam.SpriteHorizScreenPosition[s] = JackalRam.Level6BossTankScroll_Next;
        break;
      }
    }
    return;
  }
  if (!ppuDone) {                                  /* :2407-2408 CPY #$00 BEQ - */
    return;
  }
  /* 三角标闪烁（:2409-2421）：$60 每帧 ++（bit7 置 → 收尾段），每 4 帧 $32/$31 交替 */
  if ((int8_t)EOG_FlashCount >= 0) {
    EOG_FlashCount++;
    if ((EOG_FlashCount & 3u) != 0) {
      return;
    }
    Label152((EOG_FlashCount & 4u) != 0 ? 0x31u : 0x32u);
    return;
  }
  /* palette 收尾（:2422-2430）：每 $20 帧 Label152($5E)；$5E>=$35 → State2 */
  if ((JackalRam.RNG_INCEveryFrame & 0x1Fu) != 0) {
    return;
  }
  if (JackalRam.Raw[0x5E] >= 0x35u) {
    JackalRam.ControlSubState++;                     /* :2427 BCS -- */
    return;
  }
  Label152(JackalRam.Raw[0x5E]);                     /* 绿→灰终更 */
  JackalRam.Raw[0x5E]++;
}

static void EndOfGameState2(void) {  /* :2432-2445：congrats 文本写（复用 State8） */
  if (subEndOfGame_FinalBossTank_CheckForScroll_FlashComplete() != 0) {
    return;                                       /* 滚动闪烁未完 */
  }
  EndOfGameState8();
  if (JackalRam.ControlSubState == 0x0Au) {         /* 文本完（State8 置 $0A） */
    JackalRam.ScreenTimerLB = 0xC0;
    JackalRam.ControlSubState = 3;                  /* :2444-2445 强制 State3 */
  }
}

static void EndOfGameState3(void) {  /* :2450-2453：阅读延时 */
  if (fctCountDownScreenTimer() == 0) {
    JackalRam.ControlSubState++;
  }
}

/* ---------------------------------------------------------------- 状态 4-6 */

static void EndOfGameState4(void) {  /* :2455-2480：EOG 场景装载 */
  subStopMusic();                                 /* stub */
  Label159();
  subEraseAllSpriteData();
  subEraseInGameJeepData();
  Label161();
  subInitiateSoundClip(END_OF_GAME_HELI_CLIP);    /* stub */
  JackalRam.SpriteTypeIndex[1] = 0xC4;            /* 夕阳 */
  JackalRam.SpriteHorizScreenPosition[1] = 0x80;
  JackalRam.SpriteVertScreenPosition[1] = 0x6F;
  JackalRam.SpriteTypeIndex[2] = 0xC8;            /* 孤兵 */
  JackalRam.SpriteHorizScreenPosition[2] = 0x62;
  JackalRam.SpriteVertScreenPosition[2] = 0x9E;
  subIncrementSubGameState_SetScreenTimeLB(0xF0);
}

static void EndOfGameState5(void) {  /* :2482-2496：夕阳 shimmer + 待 heli */
  subEndOfGameSunShimmer();
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[0] = 0xBA;            /* heli 1 */
  JackalRam.SpriteHorizScreenPosition[0] = 0x78;
  JackalRam.SpriteVertScreenPosition[0] = 0x6A;
  JackalRam.ControlSubState++;
}

static void EndOfGameState6(void) {  /* :2497-2536：heli 接近 + 逐级放大 */
  subEndOfGameSunShimmer();
  /* :2499-2506：旋翼动画（RNG&2>>1 写入 TypeIndex bit0） */
  JackalRam.SpriteTypeIndex[0] =
      (uint8_t)((JackalRam.SpriteTypeIndex[0] & 0xFEu) |
                ((JackalRam.RNG_INCEveryFrame & 2u) >> 1));
  if ((JackalRam.RNG_INCEveryFrame & 7u) != 0) {
    return;                                       /* 每 8 帧才移动 */
  }
  JackalRam.SpriteHorizScreenPosition[0]++;
  JackalRam.SpriteVertScreenPosition[0]--;
  if (JackalRam.SpriteHorizScreenPosition[0] == 0x90u ||
      JackalRam.SpriteHorizScreenPosition[0] == 0x9Eu ||
      JackalRam.SpriteHorizScreenPosition[0] == 0xA8u ||
      JackalRam.SpriteHorizScreenPosition[0] == 0xB2u) {
    /* :2530-2534 放大一档（&$FE + 2，bit0 旋翼位保留语义） */
    JackalRam.SpriteTypeIndex[0] =
        (uint8_t)((JackalRam.SpriteTypeIndex[0] & 0xFEu) + 2u);
    return;
  }
  if (JackalRam.SpriteHorizScreenPosition[0] < 0xB6u) {
    return;
  }
  JackalRam.Raw[0x0660] = 0xC2;                   /* heli 终态 sprite type */
  subIncrementSubGameState_SetScreenTimeLB(0xF0);
}

/* ---------------------------------------------------------------- 状态 7-11 */

static void EndOfGameState7(void) {  /* :2537-2552：credits 初始化 */
  fctGetEndOfGameTextHeaderAddress(2);            /* X=2：credits 头 */
  EOG_LineAddrUB = BankPtr(0, TBL_ENDING_CREDITS_TEXT_CPU)[0];   /* $20 */
  EOG_LineAddrLB = BankPtr(0, TBL_ENDING_CREDITS_TEXT_CPU)[1];   /* $A4 */
  EOG_TextCursor = 0x02;
  EOG_FrameTimer = 0xF0;
  subInitiateSoundClip(GAME_COMPLETE_MUSIC);      /* stub */
  JackalRam.ControlSubState++;
}

static void EndOfGameState8(void) {  /* :2554-2633：文本写（每 $54 间隔写 1 字） */
  uint8_t x, b;
  EOG_FrameTimer--;
  if (EOG_FrameTimer != 0) {
    return;
  }
  EOG_FrameTimer = 0x05;
  x = JackalRam.PPUGraphicsUpdateByteLength;      /* :2559 LDX $21 */
  JackalRam.PPUUpdateQueue[x] = 1; x++;           /* type1 */
  JackalRam.PPUUpdateQueue[x] = EOG_LineAddrUB; x++;
  EOG_LineAddrLB++;                               /* :2567 后写 LB（INC 语义） */
  JackalRam.PPUUpdateQueue[x] = EOG_LineAddrLB;
  if ((EOG_LineAddrLB & 0x1Fu) == 0) {            /* :2570-2579 行地址跨页回绕 */
    EOG_LineAddrUB = (uint8_t)(EOG_LineAddrUB + 4u);
    EOG_LineAddrLB = (uint8_t)(EOG_LineAddrLB - 0x20u);
  }
  x++;
  b = EOG_TextCursor;
  EOG_TextCursor++;                               /* :2582-2584 游标回绕 → 指针 UB++ */
  if (EOG_TextCursor == 0) {
    EOG_TextPtrUB++;
  }
  {
    const uint8_t *txt = BankPtr(0, (uint16_t)(((uint16_t)EOG_TextPtrUB << 8) | EOG_TextPtrLB));
    uint8_t tile = txt[b];
    if (tile >= 0xFDu) {                          /* :2590 $FD/$FE/$FF 收尾变体 */
      if (tile == 0xFFu) {                        /* credits 全部完 → State10 */
        JackalRam.ScreenTimerLB = 0xFF;
        JackalRam.ControlSubState = 0x0A;
        return;
      }
      if (tile == 0xFEu) {                        /* 行尾：读下一行地址、$54=$28 */
        EOG_FrameTimer = 0x28;
      } else {                                    /* $FD：本条 credit 完 → State9、$54=$A0 */
        JackalRam.ControlSubState++;
        EOG_FrameTimer = 0xA0;
      }
      EOG_LineAddrUB = txt[(uint8_t)(b + 1u)];
      EOG_LineAddrLB = txt[(uint8_t)(b + 2u)];
      EOG_TextCursor = (uint8_t)(b + 3u);
      return;
    }
    JackalRam.PPUUpdateQueue[x] = tile; x++;
    if (tile == 0x2Au || tile == 0x2Du) {         /* :2593-2601 任务达成文本音效（仅 State2 语境） */
      if (JackalRam.ControlSubState == 2u) {
        subInitiateSoundClip(MISSION_TEXT_CLIP);  /* stub */
      }
    }
  }
  JackalRam.PPUUpdateQueue[x] = 0xFF; x++;
  JackalRam.PPUUpdateQueue[x] = 0; x++;
  JackalRam.PPUGraphicsUpdateByteLength = x;      /* :2608 STX $21 */
}

static void EndOfGameState9(void) {  /* :2635-2646：阅读延时 → 抹行 → 下一条 */
  EOG_FrameTimer--;
  if (EOG_FrameTimer != 0) {
    return;
  }
  EOG_FrameTimer = 0x10;
  Label152(0x35);                                 /* 抹第 1 行 */
  Label152(0x36);                                 /* 抹第 2 行 */
  JackalRam.ControlSubState = 8;
}

static void EndOfGameState10(void) {  /* :2648-2669：阅读/等待 Start */
  if (JackalRam.ScreenTimerLB == 0xC0u) {         /* :2649-2655 到时先抹行一次 */
    Label152(0x35);
    Label152(0x36);
  }
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  if ((JackalRam.JeepControlsInput[0] & 0x10u) == 0) {   /* StartButton 按住 */
    return;
  }
  subStopMusic();                                 /* stub */
  subInitiateSoundClip(EXPLOSION_CLIP);           /* stub */
  JackalRam.ControlSubState = 0x0B;
  JackalRam.Raw[0x50] = 0x54;                     /* :2667-2668 重启前延时 */
}

static void EndOfGameState11(void) {  /* :2671-2677：延时 → 回 Level 1（HARD） */
  JackalRam.Raw[0x50]--;
  if (JackalRam.Raw[0x50] != 0) {
    return;
  }
  subEraseInGameJeepData();
  JackalRam.ContinuesUsed = 0x83;                 /* :2675 注：无可续关+通关+HARD 模式 */
  subTransitionFromEndOfGameToLevel1();           /* Bank7:1059（CurrentLevel++ 回绕、GCS=5/GPM=1） */
}

void subProcessEndOfGameLogic(void) {  /* :2329-2344（dw 12 项） */
  static void (*const tbl[12])(void) = {
    EndOfGameState0, EndOfGameState1, EndOfGameState2, EndOfGameState3,
    EndOfGameState4, EndOfGameState5, EndOfGameState6, EndOfGameState7,
    EndOfGameState8, EndOfGameState9, EndOfGameState10, EndOfGameState11,
  };
  tbl[JackalRam.ControlSubState]();
}
