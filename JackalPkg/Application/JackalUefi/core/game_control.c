/* 游戏控制状态机 + NMI 帧序（Bank7.ASM 逐行翻译）。
   覆盖：NMI_VECTOR（:125-166）、subProcessGameControl（:625-643）、GCS0-4（:645-858）、
   标题/POW 子状态（:678-824）、Label774/795（:1312-1386）、状态 helpers（:1122-1156）、
   POW 裂屏移位（:1158-1189）、subTitleScreenInitialization/Label780/781/971（:2291-2333）、
   erase 例程（:1497-1536）、GCS5 的 GPM 分发（:3269-3283）、GPM0/1（:3410-3464）、
   GPM2/3 与 Label979/1007/ProcessLevelBGAnimation（:3471-3768，滚屏/生成本体在 scroll.c）、
   GPM4-10 Chinook（:3285-3408）、InitializeLevel（:3510-3542）、
   subLoadLevelDefaultPalette（:3547-3556）。
   subExecuteCodeViaIndirectJump（:1457-1478）语义：JSR 压栈返回地址定位紧随的 dw 表、
   ASL 索引、JMP ($00)——C 侧归约为函数指针表，表顺序=ASM dw 顺序，越界行为不防御（与 ASM 同）。 */
#include "ram.h"
#include "bank.h"
#include "ppu.h"
#include "loader.h"
#include "sound_stub.h"
#include "input_core.h"
#include "trace.h"
#include "scroll.h"
#include "sprite.h"
#include "jeep.h"
#include "weapon.h"
#include "interact.h"
#include "enemy_ai.h"
#include "end_of_game.h"
#include "game_control.h"
#include "jackal_assets.h"
#include "spawn.h"

#define NORMAL_PPU_MASK_VALUE 0x1Eu  /* NormalPPUMaskValue（Global.ASM:3） */
#define INITIAL_1UP_SCORE     0x02u  /* Initial1UPScore（JeepAttributes.ASM:8） */
#define INITIAL_LIFE_COUNT    0x05u  /* InitialLifeCount（JeepAttributes.ASM:9） */
#define HELIPAD_LIGHT_FLASH_RATE 0x20u  /* HelipadLightFlashRate（Global.ASM:29） */
/* tblScreenToStart/StopHelipadLightFlashing（:3771-3775，Bank7 $DE1B/$DE1C .PAD，
   每关两字节 start/stop 交错；ROM 实测 09 0B 07 09 08 0A 09 0B 09 0B 04 06） */
#define TBL_HELIPAD_FLASH_SCREENS_CPU 0xDE1Bu
/* tblEnemyPoints_DeathState（:8065，Bank7 $FA96，0x54 项；Task 3.5 签名定位核实） */
#define TBL_ENEMY_POINTS_DEATHSTATE_CPU 0xFA96u

/* tblPOWPortraitPPUUpdateAddresses（:1192-1196，Bank7 $C8FE，已对照 ROM 字节核实） */
#define POW_PORTRAIT_TABLE_CPU 0xC8FEu

static void Label795(void);  /* :1352，GCS3 先于定义使用 */
static void label824(void);    /* :1355，Label795/GCS8 共用（定义在 GCS6 段） */

/* ---------------------------------------------------------------- TRACE */

void (*JackalTraceHook)(const char *msg) = 0;

static uint8_t sTracePrevGcs = 0xFF;
static uint8_t sTracePrevGpm = 0xFF;

/* JackalReset 重置追踪基准（调试设施，非 ASM 语义；保证下一帧必输出一次 STATE） */
static void traceResetTrack(void) {
  sTracePrevGcs = 0xFF;
  sTracePrevGpm = 0xFF;
}

void JackalTraceState(void) {
  char buf[24];
  char *p = buf;
  const char *s;
  uint8_t gcs = JackalRam.GameControlState;
  uint8_t gpm = JackalRam.GamePlayMode;
  if (gcs == sTracePrevGcs && gpm == sTracePrevGpm) {
    return;
  }
  sTracePrevGcs = gcs;
  sTracePrevGpm = gpm;
  if (JackalTraceHook == 0) {
    return;
  }
  s = "STATE GCS=";
  while (*s != 0) { *p++ = *s++; }
  *p++ = (char)('0' + (gcs % 10u));
  s = " GPM=";
  while (*s != 0) { *p++ = *s++; }
  if (gpm >= 10u) { *p++ = '1'; }
  *p++ = (char)('0' + (gpm % 10u));
  *p = 0;
  JackalTraceHook(buf);
}

/* ---------------------------------------------------------------- 状态 helpers（:1122-1156） */

void subIncrementSubGameState_SetScreenTimeLB(uint8_t a) {  /* :1122（end_of_game.c 共用） */
  JackalRam.ScreenTimerLB = a;
  JackalRam.ControlSubState++;
}
static void subIncrementSubGameState(void) {                        /* :1124 */
  JackalRam.ControlSubState++;
}
static void Label788(uint8_t a) {                                   /* :1136 */
  JackalRam.ControlSubState = a;
}
static void subClear1F_Label788_0(void) {  /* :1134-1138 尾块：$1F=0 + Label788(A=0) */
  JackalRam.Raw[0x1F] = 0;   /* $1F written but not used（RAM_Symbols.ASM:32） */
  Label788(0);               /* 落入时 A=0（:1134 LDA #$00） */
}
static void subNextGameControlState(void) {                         /* :1132 */
  JackalRam.GameControlState++;
  subClear1F_Label788_0();
}
static void subSetScreeTimeLB_NextGameControlState(uint8_t a) {     /* :1130 */
  JackalRam.ScreenTimerLB = a;
  subNextGameControlState();
}
static void subSetScreeTimeLBTo80_NextGameControlState(void) {      /* :1128 */
  subSetScreeTimeLB_NextGameControlState(0x80);
}
static void subWrite_A_ToGameControlState_SetScreenTimer_ClearTitleScreenState(uint8_t a) {  /* :1140 */
  JackalRam.GameControlState = a;
  JackalRam.ScreenTimerLB = 0x50;
  subClear1F_Label788_0();  /* BNE -（:1144）直跳 :1134 尾块，不经 subNextGameControlState 的 INC */
}
/* fctCountDownScreenTimer（:1146-1156）：入口 (LB|UB)==0 → A=0（已到期）；
   否则 DEC LB，借位时 DEC UB，返回非 0。注意 LB 减到 0 当帧仍返回非 0，
   下一帧入口才见 0|0——调用方见到的"到期"比 LB 归零晚一帧，ASM 原样。 */
uint8_t fctCountDownScreenTimer(void) {  /* :1146-1156（end_of_game.c 共用） */
  if ((uint8_t)(JackalRam.ScreenTimerLB | JackalRam.ScreenTimerUB) == 0) {
    return 0;
  }
  JackalRam.ScreenTimerLB--;
  if (JackalRam.ScreenTimerLB == 0 && JackalRam.ScreenTimerUB != 0) {
    JackalRam.ScreenTimerUB--;
  }
  return 1;
}
static void subSetScreenTimerTo100(void) {  /* :1336：LB=0、UB=1（$100 帧） */
  JackalRam.ScreenTimerLB = 0;
  JackalRam.ScreenTimerUB = 1;
}

/* ---------------------------------------------------------------- erase 例程（:1497-1536） */

void subEraseAllSpriteData(void) {
  uint16_t i;
  for (i = 0; i < 0x20u; i++) {        /* SpriteAttributes,X X=$1F..0 → $6A-$89 */
    JackalRam.Raw[0x6Au + i] = 0;
  }
  for (i = 0x500u; i < 0x700u; i++) {  /* $0500-$06FF 两页 */
    JackalRam.Raw[i] = 0;
  }
  for (i = 0x700u; i < 0x76Cu; i++) {  /* $0700-$076B */
    JackalRam.Raw[i] = 0;
  }
}
void subEraseInGameJeepData(void) {    /* :1512：$50-$C1（Y=$71..0） */
  uint16_t i;
  for (i = 0x50u; i < 0xC2u; i++) {
    JackalRam.Raw[i] = 0;
  }
}
static void subEraseGameControlData(void) {  /* :1517：$35-$4F（Y=$1A..0） */
  uint16_t i;
  for (i = 0x35u; i < 0x50u; i++) {
    JackalRam.Raw[i] = 0;
  }
}
static void subEraseLevel6BossFlags(void) {  /* :1530：$113-$119（Y=7..1，$112 不写） */
  uint8_t y;
  for (y = 7; y != 0; y--) {
    JackalRam.Raw[0x112u + y] = 0;
  }
}

/* ---------------------------------------------------------------- 标题装载（:2291-2333） */

void subTitleScreenInitialization(void) {  /* :2291 */
  subStopMusic();
  Label159();
  subEraseAllSpriteData();
  subLoadNewPatternTable(6);              /* Title screen index */
  JackalRam.NormalPPUMASK = NORMAL_PPU_MASK_VALUE;
  /* LDY #$04 + subChangeBank：镜像模型 Label925 表内 bank 直读，无副作用 */
  Label925(2);                            /* 主标题 nametable */
  Label925(4);                            /* story 文本 nametable */
  Label152(6);                            /* tblTitleScreenPalette */
}
static void Label780(void) {  /* :2308：POW 肖像屏 1（POWPortraitTextIndex==0） */
  Label925(6);
  Label152(0x3C);             /* tblPOWPortraitBGPalette */
}
static void Label781(void) {  /* :2316：POW 肖像屏 2 */
  Label925(8);
  Label152(0x3C);
}
static void Label971(void) {  /* :2324：Game Started segue（blood boil + palette） */
  Label925(0x0A);
  Label152(0x3A);             /* tblLevelSeguePalette */
  JackalRam.ScreenVerticalScrollPosition_PPU = 0xE8;
}

/* ---------------------------------------------------------------- GCS0-2（:645-824） */

static const uint8_t tblTitleScreenJeepSpriteVerticalPosition_1_2Player[2] = {0x9A, 0xAA};  /* :655 */

static void GameControlState0(void) {  /* :645：标题初始化 */
  JackalRam.TitleScreenMode = 1;
  subTitleScreenInitialization();
  JackalRam.ScreenTimerUB = 2;
  subSetScreeTimeLB_NextGameControlState(0x80);
}

static void GameControlState1(void) {  /* :658：等输入；倒计时到 → story */
  JackalRam.SpriteHorizScreenPosition[0] = 0x58;
  JackalRam.SpriteTypeIndex[0] = 5;    /* 右向吉普 */
  JackalRam.SpriteVertScreenPosition[0] =
      tblTitleScreenJeepSpriteVerticalPosition_1_2Player[JackalRam.Player2Active];
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  subEraseAllSpriteData();
  subInitiateSoundClip(0);  /* TitleScreenStory_POWPortraitsSoundClip */
  subSetScreeTimeLBTo80_NextGameControlState();
}

/* subCheckForPOWPortraitShift_BottomHalfOfScreen（:1158-1189）：
   $50 滚动变量（RAM 复用）每帧 +$0C 至 $F8 封顶，并写 ScreenHorizontalScrollPosition_PPU。
   :1167-1176 大延时循环、:1178-1188 的 PPUSCROLL 反向写与 PPUCTRL|=1（下半屏切 nametable1
   实现上下半反向滚动）均为硬件寄存器行为，无镜像副作用；UEFI render 当前不支持裂屏，
   POW 肖像下半屏反向滚动记为已知差异（Phase 7 打磨）。 */
static void subCheckForPOWPortraitShift_BottomHalfOfScreen(void) {
  uint8_t a = JackalRam.Raw[0x50];
  if (a < 0xF8u) {
    a = (uint8_t)(a + 0x0Cu);
    JackalRam.Raw[0x50] = a;
  }
  JackalRam.ScreenHorizontalScrollPosition_PPU = a;
}

static void TitleScreenState0(void) {  /* :688：story 右滚至 $FF */
  JackalRam.ScreenHorizontalScrollPosition_PPU++;
  if (JackalRam.ScreenHorizontalScrollPosition_PPU != 0xFFu) {
    return;
  }
  subEraseAllSpriteData();
  subEraseInGameJeepData();
  JackalRam.ScreenTimerUB = 2;
  subIncrementSubGameState_SetScreenTimeLB(0x14);
}

static void TitleScreenState1(void) {  /* :700：阅读延时 → 装肖像屏 */
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  Label159();
  JackalRam.Raw[0x50] = 0;  /* $50 POW 画面复用为滚动位置 */
  if (JackalRam.POWPortraitTextIndex == 0) {
    Label780();
  } else {
    Label781();
  }
  subIncrementSubGameState();
}

static void TitleScreenState2(void) {  /* :714：肖像滚入，到位后装名字文本流参数 */
  const uint8_t *tbl;
  const uint8_t *text;
  uint8_t y;
  subCheckForPOWPortraitShift_BottomHalfOfScreen();
  if (JackalRam.Raw[0x50] < 0xF8u) {
    return;
  }
  if ((JackalRam.POWPortraitTextIndex & 1u) == 0) {  /* 每屏只 shing 一次 */
    subInitiateSoundClip(0);  /* POWPortraitScrollIntoPlace_Shing_SoundClip */
  }
  tbl = BankPtr(7, POW_PORTRAIT_TABLE_CPU);
  y = (uint8_t)(JackalRam.POWPortraitTextIndex << 1);
  JackalRam.Raw[0x56] = tbl[y];       /* 文本块地址低字节（$00/$01 间接寻址副本） */
  JackalRam.Raw[0x58] = tbl[y + 1u];  /* 高字节 */
  text = BankPtr(7, (uint16_t)(tbl[y] | ((uint16_t)tbl[y + 1u] << 8)));
  JackalRam.Raw[0x5A] = text[0];      /* 首行 PPU 地址 hi */
  JackalRam.Raw[0x5C] = text[1];      /* 首行 PPU 地址 lo */
  JackalRam.Raw[0x5E] = 2;            /* 文本游标 */
  JackalRam.Raw[0x54] = 0x20;         /* 打字延时 */
  JackalRam.ControlSubState++;        /* INC $19 */
}

static void TitleScreenState3(void) {  /* :747：逐字打名字（$0770 队列入队） */
  const uint8_t *text = BankPtr(7, (uint16_t)(JackalRam.Raw[0x56] |
                                              ((uint16_t)JackalRam.Raw[0x58] << 8)));
  uint8_t x, y;
  subCheckForPOWPortraitShift_BottomHalfOfScreen();
  JackalRam.Raw[0x54]--;
  if (JackalRam.Raw[0x54] != 0) {
    return;
  }
  subInitiateSoundClip(0);  /* POWPortraitTextPrintSoundClip */
  JackalRam.Raw[0x54] = 6;
  x = JackalRam.PPUGraphicsUpdateByteLength;
  JackalRam.PPUUpdateQueue[x++] = 1;                    /* type 1 水平写 */
  JackalRam.PPUUpdateQueue[x++] = JackalRam.Raw[0x5A];
  JackalRam.PPUUpdateQueue[x++] = JackalRam.Raw[0x5C];
  JackalRam.Raw[0x5C]++;                                /* INC $5C 在入队后（:762-764） */
  y = JackalRam.Raw[0x5E];
  JackalRam.PPUUpdateQueue[x++] = text[y];
  JackalRam.PPUUpdateQueue[x++] = 0xFF;
  JackalRam.PPUUpdateQueue[x++] = 0;
  JackalRam.PPUGraphicsUpdateByteLength = x;
  y++;
  JackalRam.Raw[0x5E] = y;
  if (text[y] < 0xFEu) {
    return;
  }
  if (text[y] == 0xFFu) {  /* 名字打完（:798-807） */
    JackalRam.POWPortraitTextIndex++;
    if (JackalRam.POWPortraitTextIndex >= 4u || JackalRam.POWPortraitTextIndex == 2u) {
      subIncrementSubGameState_SetScreenTimeLB(0x50);
    } else {
      Label788(2);
    }
    return;
  }
  /* $FE：换行（:787-796） */
  JackalRam.Raw[0x54] = 0x10;
  JackalRam.Raw[0x5A] = text[y + 1u];
  JackalRam.Raw[0x5C] = text[y + 2u];
  JackalRam.Raw[0x5E] = (uint8_t)(y + 3u);
}

static void TitleScreenState4(void) {  /* :809：肖像停留 → 下一屏或回 GCS0 */
  subCheckForPOWPortraitShift_BottomHalfOfScreen();
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  if (JackalRam.POWPortraitTextIndex >= 4u) {
    JackalRam.GameControlState = 0;
    return;
  }
  Label788(1);
}

static void GameControlState2(void) {  /* :678：$19 分发（subExecuteCodeViaIndirectJump 语义） */
  static void (*const tbl[5])(void) = {
    TitleScreenState0, TitleScreenState1, TitleScreenState2,
    TitleScreenState3, TitleScreenState4,
  };
  tbl[JackalRam.ControlSubState]();
}

/* ---------------------------------------------------------------- GCS3/4（:825-858） */

static void GameControlState3(void) {  /* :825：开始键闪烁 → Label795 → GCS4 */
  uint8_t x = JackalRam.ControlSubState;  /* LDX $19 */
  if (x == 0) {
    subInitiateSoundClip(0);  /* MainWeaponExplosionOnEnemy */
    JackalRam.TitleScreenMode = 0;
    subIncrementSubGameState_SetScreenTimeLB(0x50);
    return;
  }
  if (x == 1) {
    uint8_t a;
    JackalRam.ScreenTimerLB--;
    if (JackalRam.ScreenTimerLB == 0) {
      subIncrementSubGameState();
      return;
    }
    /* 闪烁：((LB&$08)<<4) | (Player2Active+1) → Label152（$8x=隐藏位） */
    a = (uint8_t)(1u + JackalRam.Player2Active);
    a = (uint8_t)(a | ((uint8_t)(JackalRam.ScreenTimerLB & 0x08u) << 4));
    Label152(a);
    return;
  }
  /* x>=2（DEX DEX 后落入，:851） */
  Label795();
  subNextGameControlState();
}

static void GameControlState4(void) {  /* :854 */
  subEraseAllSpriteData();
  JackalRam.GamePlayMode = 0;
  subNextGameControlState();
}

/* ---------------------------------------------------------------- Label774/795（:1312-1386） */

static void Label774(void) {  /* GCS<3 时每帧先查 Start/Select 边沿 */
  uint8_t a = (uint8_t)(JackalRam.JeepControlsInput1Frame[0] & 0x30u);  /* Start+Select */
  if (a == 0) {
    return;
  }
  subSetScreenTimerTo100();
  if (JackalRam.GameControlState == 1) {
    if ((a & 0x20u) != 0) {  /* Select：2-x 技巧翻转 Player2Active（0→1→0） */
      JackalRam.Player2Active++;
      a = (uint8_t)(2u - JackalRam.Player2Active);
      if (a != 0) {
        return;
      }
      JackalRam.Player2Active = a;
      return;
    }
    /* Start → GCS=3（LB=$50 闪烁段） */
    subWrite_A_ToGameControlState_SetScreenTimer_ClearTitleScreenState(3);
    return;
  }
  /* GCS==0/2 按 Start/Select：回标题（:1330-1334） */
  subTitleScreenInitialization();
  subSetScreenTimerTo100();
  JackalRam.GameControlState = 1;
}

static const uint8_t tblPlayerMode[2] = {0x01, 0x07};  /* :1383 */

static void Label795(void) {  /* :1352：开局初始化（Label844 + Label824） */
  Label844();
  label824();
}

/* ---------------------------------------------------------------- GPM（:3269-3542） */

static void GamePlayModeState0(void) {  /* :3410：Level 1 首进 intro（Game Started segue） */
  if (JackalRam.TitleScreenMode != 0 || JackalRam.CurrentLevel != 0) {
    JackalRam.GamePlayMode++;
    return;
  }
  if (JackalRam.SpriteData1[0] != 0) {  /* $0660 intro 进行中：$0670(SpriteData2[0]) 倒计时 */
    JackalRam.SpriteData2[0]--;
    if (JackalRam.SpriteData2[0] != 0) {
      return;
    }
    JackalRam.GamePlayMode++;
    return;
  }
  Label159();
  subLoadNewPatternTable(8);            /* Game Started index */
  Label971();
  subInitiateSoundClip(0);              /* GameStartedIntroSoundClip */
  JackalRam.SpriteHorizScreenPosition[1] = 0x30;  /* $05A1 segue 地图精灵 */
  JackalRam.SpriteVertScreenPosition[1] = 0x77;   /* $0561 */
  JackalRam.SpriteTypeIndex[1] = 0xB0;            /* $0501 */
  JackalRam.SpriteData2[0] = 0x90;                /* $0670 intro 倒计时 144 帧 */
  JackalRam.SpriteData1[0]++;                     /* $0660 intro 进行中标记 */
}

static void subLoadLevelDefaultPalette(void) {  /* :3547（X/Y 保全无 C 侧效果） */
  Label152((uint8_t)(0x0Du + JackalRam.CurrentLevel));
}

static void GamePlayModeState1(void) {  /* :3438：黑屏装载关卡 */
  Label159();
  subEraseAllSpriteData();
  subEraseGameControlData();
  JackalRam.GamePlayMode++;
  JackalRam.Zp47 = 0x23;
  JackalRam.Zp49 = 0x23;
  JackalRam.Zp48 = 0xA0;
  JackalRam.Zp4A = 0xF8;
  JackalRam.Zp4B = 0;
  JackalRam.Zp4C = 0;
  JackalRam.PPUGraphicsUpdateTableIndex = 0;
  JackalRam.PPUGraphicsUpdateComplete = 0;
  JackalRam.Zp4D = 2;
  subLoadNewPatternTable(JackalRam.CurrentLevel);
  subLoadLevelDefaultPalette();
  JackalRam.ScreenTransitionTimer = 0x25;
}

/* ---------------------------------------------------------------- GPM2/3 与主循环 */
/* Phase 3/4 实装的调用点桩（结构保留，ASM 行号备查）：
   Label995（:5115 区域吉普主逻辑，Task 3.6/3.7 起实装在 jeep.c/weapon.c——
   移动+射击分发；滚动协同与死亡处理 Task 3.9）、Label996/subProcessJeepMainWeapon/
   subProcessJeepBullet/Label1001（Task 3.7 起实装在 weapon.c）、
   Label1005（Task 3.8 起实装在 interact.c）、subProcessObjectLogic
   （Task 4.2 起实装在 enemy_ai.c——分派表 50 项，Logic 随 Task 4.3+ 逐组登记）、
   Label1011（:3832 in-game pattern 流式更新，Phase 6）。 */
/* Label1011（:3833-3942）：in-game pattern 流式更新——首帧读 7 字节描述符
   （bank/PPU 地址/源指针/剩余），其后每帧队列 $18 余量门控按
   tblPPUGraphicsUpdateBytesToLoad 流式写 type 3 段，16 位剩余递减 + 指针推进，
   尽 → 清 TableIndex/Complete。18 型（$00 EMPTY-$11）三表值已对照 ROM 核实
   （bank 表签名 07 02…01 04 02 05 定位 $DF2D；action dw 逐表与 .PAD 几何吻合）。 */
static const uint16_t tblInGamePalette_GraphicsUpdateActionAddress[18] = {  /* :3965-3983 */
  0xDF2C, 0xAB86, 0xAEAD, 0xA9F8, 0xAABF, 0xB2F4, 0xB4DB, 0xB652, 0xBA19,
  0xBA30, 0xBA47, 0xBAAE, 0xBB15, 0xBB7C, 0xB6E5, 0xBBC9, 0xBBE3, 0xA98C,
};
static const uint8_t tblPPUGraphicsUpdateBank[18] = {  /* :3945-3963 */
  0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
  0x02, 0x02, 0x02, 0x02, 0x02, 0x01, 0x04, 0x02, 0x05,
};
static const uint8_t tblPPUGraphicsUpdateBytesToLoad[18] = {  /* :3812-3830 */
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10,
  0x10, 0x20, 0x20, 0x20, 0x20, 0x40, 0x40, 0x40, 0x20,
};

void Label1011(void) {
  uint8_t idx = JackalRam.PPUGraphicsUpdateTableIndex;
  uint8_t x, count;
  uint16_t remaining, srcAddr, ppuAddr, full;
  if (idx == 0) {
    return;                                     /* :3834-3835 BEQ ++ */
  }
  if (JackalRam.PPUGraphicsUpdateComplete == 0) {
    const uint8_t *d;
    JackalRam.PPUGraphicsUpdateComplete = 1;    /* :3838 */
    /* :3845-3848 bank 切换：镜像模型无副作用（BankPtr 直读） */
    d = BankPtr(tblPPUGraphicsUpdateBank[idx],
                tblInGamePalette_GraphicsUpdateActionAddress[idx]);
    JackalRam.Raw[0xC0] = d[0];                 /* 源数据 bank */
    JackalRam.Raw[0xBE] = d[1];                 /* PPU 地址 UB/LB */
    JackalRam.Raw[0xBF] = d[2];
    JackalRam.Raw[0xBC] = d[3];                 /* 源指针 LB/UB */
    JackalRam.Raw[0xBD] = d[4];
    JackalRam.Raw[0xC1] = d[5];                 /* 剩余 UB/LB */
    JackalRam.Raw[0xC2] = d[6];
    return;                                     /* :3870-3871 首帧只读描述符 */
  }
  x = JackalRam.PPUGraphicsUpdateByteLength;
  if (x >= 0x18u) {
    return;                                     /* :3874-3875 队列余量门 */
  }
  full = tblPPUGraphicsUpdateBytesToLoad[idx];
  remaining = (uint16_t)(((uint16_t)JackalRam.Raw[0xC1] << 8) | JackalRam.Raw[0xC2]);
  count = remaining < full ? (uint8_t)remaining : (uint8_t)full;  /* :3890-3896 尾段截断 */
  JackalRam.PPUUpdateQueue[x] = 3;              x++;   /* :3880 type 3 */
  JackalRam.PPUUpdateQueue[x] = JackalRam.Raw[0xBE]; x++;
  JackalRam.PPUUpdateQueue[x] = JackalRam.Raw[0xBF]; x++;
  JackalRam.PPUUpdateQueue[x] = count;          x++;
  {
    const uint8_t *src = BankPtr(JackalRam.Raw[0xC0],
        (uint16_t)(((uint16_t)JackalRam.Raw[0xBD] << 8) | JackalRam.Raw[0xBC]));
    uint8_t i;
    for (i = 0; i < count; i++) {               /* :3900-3903 逐字节入队 */
      JackalRam.PPUUpdateQueue[x] = src[i];
      x++;
    }
  }
  /* :3904-3931：剩余 16 位递减；未尽 → 源/目标指针推进 full（$10） */
  remaining = (uint16_t)(remaining - count);
  JackalRam.Raw[0xC2] = (uint8_t)remaining;
  JackalRam.Raw[0xC1] = (uint8_t)(remaining >> 8);
  if (remaining == 0) {
    JackalRam.PPUGraphicsUpdateTableIndex = 0;  /* :3932-3934 */
    JackalRam.PPUGraphicsUpdateComplete = 0;
  } else {
    srcAddr = (uint16_t)((((uint16_t)JackalRam.Raw[0xBD]) << 8 | JackalRam.Raw[0xBC]) + full);
    ppuAddr = (uint16_t)((((uint16_t)JackalRam.Raw[0xBE]) << 8 | JackalRam.Raw[0xBF]) + full);
    JackalRam.Raw[0xBC] = (uint8_t)srcAddr;
    JackalRam.Raw[0xBD] = (uint8_t)(srcAddr >> 8);
    JackalRam.Raw[0xBF] = (uint8_t)ppuAddr;
    JackalRam.Raw[0xBE] = (uint8_t)(ppuAddr >> 8);
  }
  JackalRam.PPUUpdateQueue[x] = 0xFF;           x++;   /* :3935-3940 终止符 */
  JackalRam.PPUUpdateQueue[x] = 0;              x++;
  JackalRam.PPUGraphicsUpdateByteLength = x;
}

static void label979(void);  /* GPM2 先于定义 JMP 引用（:3498） */
static void processLevelBGAnimation(void);
static void InitializeLevel(void);

static void GamePlayModeState2(void) {  /* :3471：首屏滚动装载 + 生成游走 */
  subChangeBank_YhasBank(jackal_tblLevelLayoutBank[JackalRam.CurrentLevel]);  /* :3472-3474 */
  Label975();
  if (JackalRam.CurrentLevelScreen == 0) {
    /* :3478-3493：每帧 +8 像素；到 $F0 首屏完成（screen=1，游标/boss 裂屏变量清） */
    JackalRam.CurrentLevelScreenSubPosition =
        (uint8_t)(JackalRam.CurrentLevelScreenSubPosition + 8u);
    if (JackalRam.CurrentLevelScreenSubPosition == 0xF0u) {
      JackalRam.CurrentLevelScreen = 1;
      JackalRam.CurrentLevelScreenSubPosition = 0;
      JackalRam.SpawnBlockIndex = 0;
      JackalRam.Level6BossTankScroll_Next = 0;
    }
    subChangeBank_YhasBank(6);   /* :3495 */
    Label978();
    label979();                  /* :3498 JMP Label979 */
    return;
  }
  /* :3499-3508：续关/多周目/非 Level 1 直接 InitializeLevel，否则走 Chinook 序列 */
  if (JackalRam.TitleScreenMode != 0 || JackalRam.CurrentLevel != 0 ||
      JackalRam.ContinuesUsed != 0) {
    InitializeLevel();
    return;
  }
  JackalRam.GamePlayMode = 4;
}

static void GamePlayModeState3(void) {  /* :3570：正常关卡主循环 */
  int8_t x;
  if ((int8_t)JackalRam.ContinuesUsed < 0) {   /* :3571 BPL + */
    JackalRam.DifficultyBasedOnWeapon = 3;     /* 二周目强制最高难度 */
  }
  Label984();                                  /* :3575 暂停 */
  if (JackalRam.GamePaused != 0) {
    return;
  }
  if ((uint8_t)(JackalRam.Jeep1LifeCount + JackalRam.Jeep2LifeCount) == 0) {  /* :3578 */
    JackalRam.GameControlState = 6;            /* 双亡 → Game Over（:3583） */
    JackalRam.ControlSubState = 0;
    return;
  }
  if (JackalRam.LevelBossEntitiesRemaining == 0) {   /* :3588-3618 boss 歼灭收尾 */
    if (JackalRam.EndofLevelDelayAfterBossDeath == 0) {
      /* :3592-3604 残敌处决扫描（X=$0F..0：HP bit7 跳过、ID&$7F==0 跳过，
         否则 SpriteState = tblEnemyPoints_DeathState[id]&$0F（死亡动画状态） */
      int8_t i;
      for (i = 0x0F; i >= 0; i--) {
        uint8_t id;
        if ((int8_t)JackalRam.SpriteHealthHP[i] < 0) { continue; }
        id = (uint8_t)(JackalRam.SpriteObjectID[i] & 0x7Fu);
        if (id == 0) { continue; }
        JackalRam.SpriteState[i] =
            (uint8_t)(BankPtr(7, TBL_ENEMY_POINTS_DEATHSTATE_CPU)[id] & 0x0Fu);
      }
    }
    /* :3605-3608 CurrentLevel 无用分支（ASM "unnecessary" 注释，无效果，结构保留） */
    {
      const uint16_t t = (uint16_t)(JackalRam.EndofLevelDelayAfterBossDeath + 2u);
      JackalRam.EndofLevelDelayAfterBossDeath = (uint8_t)t;
      if (t >= 0x100u) {                       /* :3613 BCC ++ 否则 :3614 */
        JackalRam.GameControlState = 7;        /* 关卡完成 */
        JackalRam.ControlSubState = 0;
        return;
      }
    }
  }
  subChangeBank_YhasBank(jackal_tblLevelLayoutBank[JackalRam.CurrentLevel]);  /* :3620-3622 */
  Label975();
  subChangeBank_YhasBank(6);                   /* :3624 */
  Label978();
  /* :3627-3642：F0 滚动（boss 前垂直锁未置时）subpos+1，跨屏 screen++ */
  if (JackalRam.ScreenVerticalScrollLockForBossFight == 0 &&
      JackalRam.ScreenScrollingForF0ToBoss != 0) {
    JackalRam.CurrentLevelScreenSubPosition++;
    if (JackalRam.CurrentLevelScreenSubPosition >= 0xF0u) {
      JackalRam.CurrentLevelScreenSubPosition = 0;
      JackalRam.CurrentLevelScreen++;
    }
  }
  if (JackalRam.ScreenScrollingForF0ToBoss != 0) {
    /* :3643-3665 F0 拖拽：吉普 Y<$D0 且存活则 +1/帧 */
    if (JackalRam.JeepVertScreenPosition < 0xD0u && JackalRam.Jeep1LifeCount != 0) {
      JackalRam.JeepVertScreenPosition++;
      JackalRam.Jeep1VertPosition++;
    }
    if (JackalRam.Player2Active != 0 &&
        JackalRam.Jeep2VertScreenPosition < 0xD0u && JackalRam.Jeep2LifeCount != 0) {
      JackalRam.Jeep2VertScreenPosition++;
      JackalRam.Jeep2VertPosition++;
    }
  } else if (JackalRam.Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding == 0) {
    /* :3667-3675 无 F0：吉普主逻辑（Label995 桩，Phase 3） */
    JackalRam.Raw[0x39] = 0x10;
    Label995();
    JackalRam.Jeep1EscalatorEffectActive = 0;
    JackalRam.Jeep2EscalatorEffectActive = 0;
    JackalRam.Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding = 0;
  }
  /* :3676-3683：$39=$1F，X=7..0：Label996 + 主武器 */
  JackalRam.Raw[0x39] = 0x1F;
  for (x = 7; x >= 0; x--) {
    Label996((uint8_t)x);
    subProcessJeepMainWeapon((uint8_t)x);
    JackalRam.Raw[0x39]--;
  }
  /* :3684-3694：$39=$17，X=5..0：吉普子弹；RNG bit0=0 才 Label1001（LSR;BCS+） */
  JackalRam.Raw[0x39] = 0x17;
  for (x = 5; x >= 0; x--) {
    subProcessJeepBullet((uint8_t)x);
    if ((JackalRam.RNG_INCEveryFrame & 1u) == 0) {
      Label1001((uint8_t)x);
    }
    JackalRam.Raw[0x39]--;
  }
  subChangeBank_YhasBank(6);                   /* :3695 */
  JackalRam.JeepAtHelipadDropoff[0] = 0;       /* :3697-3699（$5A/$5B） */
  JackalRam.JeepAtHelipadDropoff[1] = 0;
  label979();
}

/* Label979（:3700-3710）：16 槽对象逻辑循环 + helipad 计时 + BG 动画 */
static void label979(void) {
  int8_t x;
  for (x = 15; x >= 0; x--) {
    JackalRam.Raw[0x35] = (uint8_t)x;          /* :3702-3703 $35/$39 双游标 */
    JackalRam.Raw[0x39] = (uint8_t)x;
    /* :3702 STX $35 的真实语义：父子生成（subSpawnObjectFromParent）的父槽。
       初版只写 Raw[0x35] 镜像（全仓库无读者），生成侧读 JackalSpawnZp[0x35]
       拿到陈旧父槽——炮塔子弹错落在槽 0 位置首帧即灭（用户报告 bug #2 根因）。
       enemy_*.c 内各 JackalSpawnZp[0x35]=x 自设点中，label979 循环外调用者
       （GPM3 吉普/POW 直落）仍必需，循环内者为冗余但无害，保留不动。 */
    JackalSpawnZp[0x35] = (uint8_t)x;
    subProcessObjectLogic((uint8_t)x);
    if ((JackalRam.RNG_INCEveryFrame & 1u) != 0) {   /* LSR; BCC + → bit0=1 调用 */
      Label1005((uint8_t)x);
    }
  }
  /* :3711-3714：helipad 计时（==0 → Label1007；bit7=禁用不 DEC；否则 DEC） */
  if (JackalRam.LevelHelipadLightFlashTimer == 0) {
    if (label1007() == 0) {
      return;   /* :3743 BCS -：队列忙，GPM3 提前 RTS */
    }
  } else if ((int8_t)JackalRam.LevelHelipadLightFlashTimer >= 0) {
    JackalRam.LevelHelipadLightFlashTimer--;
  }
  processLevelBGAnimation();
}

/* Label1007（:3740-3768）：helipad 闪光灯 palette 轮换。返回 1=落入 BG 动画、0=RTS */
uint8_t label1007(void) {
  uint8_t x2;
  if (JackalRam.PPUGraphicsUpdateByteLength >= 0x21u) {   /* :3741 队列余量检查 */
    return 0;
  }
  JackalRam.LevelHelipadLightFlashTimer = HELIPAD_LIGHT_FLASH_RATE;   /* :3744 */
  x2 = (uint8_t)(JackalRam.CurrentLevel * 2u);
  /* :3749-3753：start <= screen < stop 才轮换（$DE1B 起 start/stop 交错对） */
  if (JackalRam.CurrentLevelScreen >= BankPtr(7, TBL_HELIPAD_FLASH_SCREENS_CPU)[x2] &&
      JackalRam.CurrentLevelScreen < BankPtr(7, TBL_HELIPAD_FLASH_SCREENS_CPU)[(uint8_t)(x2 + 1u)]) {
    /* :3754-3761：Label152(level*4 + palette + $13) */
    Label152((uint8_t)((uint8_t)(JackalRam.CurrentLevel * 4u) +
                       JackalRam.HelipadLightFlashPalette + 0x13u));
    JackalRam.HelipadLightFlashPalette++;               /* :3762 */
    if (JackalRam.HelipadLightFlashPalette >= 4u) {     /* :3764 */
      JackalRam.HelipadLightFlashPalette = 0;
    }
  }
  return 1;
}

/* ProcessLevelBGAnimation（:3717-3737）：Level 3（CurrentLevel==2）水流动画，
   其余关卡空操作但调用结构保留；尾部 Label1011（Phase 6 桩）。 */
static void processLevelBGAnimation(void) {
  if (JackalRam.CurrentLevel == 2u && JackalRam.PPUGraphicsUpdateTableIndex == 0) {
    JackalRam.LevelBGAnimatedTileUpdateTimer--;
    if (JackalRam.LevelBGAnimatedTileUpdateTimer == 0) {
      JackalRam.LevelBGAnimatedTileUpdateTimer = 0x10;
      JackalRam.PPUGraphicsUpdateTableIndex =
          (JackalRam.LevelBGAnimatedTileUpdateIndex == 0) ? 3u : 4u;
      JackalRam.PPUGraphicsUpdateComplete = 0;
      JackalRam.LevelBGAnimatedTileUpdateIndex ^= 1u;
    }
  }
  Label1011();   /* :3737 */
}


static void lblUpdateChinookPropellerPosition(void) {  /* :3398：每 4 帧 $7A/$7B 交替 */
  if ((JackalRam.RNG_INCEveryFrame & 3u) != 0) {
    return;
  }
  JackalRam.SpriteTypeIndex[0] =
      (JackalRam.SpriteTypeIndex[0] == 0x7Au) ? 0x7Bu : 0x7Au;
}

static void GamePlayModeState4(void) {  /* :3285：Chinook 入场数据 */
  JackalRam.SpriteTypeIndex[0] = 0x7A;
  JackalRam.SpriteHorizScreenPosition[0] = 0x78;
  JackalRam.SpriteVertScreenPosition[0] = 0xF8;
  JackalRam.GamePlayMode++;
}

static void GamePlayModeState5(void) {  /* :3296：升至屏顶 */
  lblUpdateChinookPropellerPosition();
  JackalRam.SpriteVertScreenPosition[0]--;
  if (JackalRam.SpriteVertScreenPosition[0] < 0x30u) {
    JackalRam.GamePlayMode++;
  }
}

static void GamePlayModeState6(void) {  /* :3304：下降右移（RNG 门控；vert 判定仅在 horiz 帧） */
  lblUpdateChinookPropellerPosition();
  if ((JackalRam.RNG_INCEveryFrame & 4u) != 0) {
    return;
  }
  JackalRam.SpriteVertScreenPosition[0]++;
  if ((JackalRam.RNG_INCEveryFrame & 1u) == 0) {  /* LSR; BCC → bit0=1 才继续 */
    return;
  }
  JackalRam.SpriteHorizScreenPosition[0]++;
  if (JackalRam.SpriteVertScreenPosition[0] >= 0x40u) {
    JackalRam.GamePlayMode++;
  }
}

static void GamePlayModeState7(void) {  /* :3319：吉普就位 */
  JackalRam.JeepHorizScreenPosition = 0x80;
  JackalRam.Jeep2HorizScreenPosition = 0x80;
  JackalRam.JeepVertScreenPosition = 0x50;
  JackalRam.Jeep2VertScreenPosition = 0x50;
  JackalRam.Jeep1GraphicsAttributes = 0;   /* 绿 */
  JackalRam.Jeep2GraphicsAttributes = 1;   /* 棕 */
  JackalRam.JeepTypeIndex = 3;
  JackalRam.JeepAttributes = 0xC0;         /* = Jeep1Attributes（$007A） */
  JackalRam.GamePlayMode++;
}

static void GamePlayModeState8(void) {  /* :3339：部署吉普 */
  lblUpdateChinookPropellerPosition();
  if (JackalRam.JeepVertScreenPosition != 0x80u) {
    JackalRam.JeepVertScreenPosition++;
    JackalRam.JeepHorizScreenPosition--;
  } else {
    JackalRam.Jeep1Attributes = 0;         /* 镜像/绿 palette */
    JackalRam.JeepTypeIndex = 1;           /* 朝上 */
    if (JackalRam.Player2Active == 0) {
      JackalRam.GamePlayMode++;
      return;
    }
  }
  if (JackalRam.Player2Active == 0) {      /* ++ 入口（:3353） */
    return;
  }
  if (JackalRam.JeepHorizScreenPosition >= 0x70u) {  /* Jeep2 晚一点下车 */
    return;
  }
  JackalRam.Jeep2TypeIndex = 3;
  JackalRam.Jeep2Attributes = 0x80;
  if (JackalRam.Jeep2VertScreenPosition != 0x80u) {
    JackalRam.Jeep2VertScreenPosition++;
    JackalRam.Jeep2HorizScreenPosition++;
    return;
  }
  JackalRam.Jeep2Attributes = 0;
  JackalRam.Jeep2TypeIndex = 1;
  JackalRam.GamePlayMode++;                /* BNE -（A=1 必转，:3372） */
}

static void GamePlayModeState9(void) {  /* :3374：升空（RNG 门控同 GPM6 结构） */
  lblUpdateChinookPropellerPosition();
  if ((JackalRam.RNG_INCEveryFrame & 4u) != 0) {
    return;
  }
  JackalRam.SpriteVertScreenPosition[0]--;
  if ((JackalRam.RNG_INCEveryFrame & 1u) == 0) {
    return;
  }
  JackalRam.SpriteHorizScreenPosition[0]--;
  if (JackalRam.SpriteVertScreenPosition[0] < 0x30u) {
    JackalRam.GamePlayMode++;
  }
}

static void GamePlayModeState10(void) {  /* :3389：离屏 → InitializeLevel */
  lblUpdateChinookPropellerPosition();
  JackalRam.SpriteVertScreenPosition[0]--;
  if ((JackalRam.SpriteVertScreenPosition[0] & 0x80u) == 0) {  /* BPL：未翻负 */
    return;
  }
  JackalRam.SpriteTypeIndex[0] = 0;
  InitializeLevel();
}

static const uint8_t tblBeginLevelJeepHorizontalSpawnLocation[2] = {0x50, 0xB0};       /* :3562 */
static const uint8_t tblInitialLevelHorizontalScrollPosition[6] = {0x00, 0x80, 0x44, 0x00, 0x90, 0xCD};  /* :3558 */
static const uint8_t tblLevelBGMusicIndex[6] = {0x38, 0x3C, 0x40, 0x38, 0x3C, 0x40};   /* :3566 */

static void InitializeLevel(void) {  /* :3510 */
  int8_t x;
  for (x = 1; x >= 0; x--) {         /* X=1,0：双吉普 spawn 位置 */
    JackalRam.SpriteVertScreenPosition[0x10u + (uint8_t)x] = 0x80;   /* JeepVertScreenPosition,X */
    JackalRam.JeepVertPosition[x] = 0x80;
    JackalRam.SpriteHorizScreenPosition[0x10u + (uint8_t)x] = tblBeginLevelJeepHorizontalSpawnLocation[x];
    JackalRam.JeepHorizPosition[x] = tblBeginLevelJeepHorizontalSpawnLocation[x];
  }
  subEraseLevel6BossFlags();
  if (JackalRam.CurrentLevel == 0) {  /* 仅 Level 1：装载新精灵数据索引 */
    JackalRam.PPUGraphicsUpdateComplete = 0;
    JackalRam.PPUGraphicsUpdateTableIndex = 5;
  }
  JackalRam.ScreenLeftScrollPosition = tblInitialLevelHorizontalScrollPosition[JackalRam.CurrentLevel];
  JackalRam.LevelBossEntitiesRemaining = 0xFF;
  JackalRam.LevelHelipadLightFlashTimer = 7;   /* Helipad Light Flash Rate */
  JackalRam.GamePlayMode = 3;
  if (JackalRam.CurrentLevel == 0 && JackalRam.ContinuesUsed == 0) {
    return;
  }
  subStopMusic();
  subInitiateSoundClip(tblLevelBGMusicIndex[JackalRam.CurrentLevel]);
}

/* ---------------------------------------------------------------- GCS 分发与桩 */

static void GameControlState5(void) {  /* :3269：GPM 分发（表顺序=ASM dw 顺序） */
  static void (*const tbl[11])(void) = {
    GamePlayModeState0, GamePlayModeState1, GamePlayModeState2, GamePlayModeState3,
    GamePlayModeState4, GamePlayModeState5, GamePlayModeState6, GamePlayModeState7,
    GamePlayModeState8, GamePlayModeState9, GamePlayModeState10,
  };
  tbl[JackalRam.GamePlayMode]();
}

/* GCS6-9：Game Over/Continue、关卡过渡、结局（:860-1120）。
   GCS9（结局，Bank0 subProcessEndOfGameLogic 大型过场）归 Phase 7。 */
#define NUMBER_OF_CONTINUES 3u          /* NumberOfContinues（JeepAttributes.ASM:10） */
#define GAMEOVER_CONTINUE_CLIP 0x50u    /* GameOver_Continue_SoundClip（Sound.ASM:97） */

/* label824（:1355-1380）：分清/模式/1UP 阈值/命数（Label795 与 GCS8 共用） */
static void label824(void) {
  int8_t x;
  for (x = 0x0B; x >= 0; x--) {
    JackalRam.Raw[0x07E4u + (uint8_t)x] = 0;
  }
  JackalRam.TitleScreenMode = 0;
  JackalRam.PlayerMode_1or2 = tblPlayerMode[JackalRam.Player2Active];
  JackalRam.Jeep1Next1Up = INITIAL_1UP_SCORE;
  JackalRam.Jeep2Next1Up = INITIAL_1UP_SCORE;
  JackalRam.Jeep2LifeCount = 0;
  JackalRam.Jeep1LifeCount = INITIAL_LIFE_COUNT;
  if (JackalRam.Player2Active != 0) {
    JackalRam.Jeep2LifeCount = INITIAL_LIFE_COUNT;
  }
}

static void GameControlState6(void) {  /* :860-895：Game Over + Continue 入口 */
  if (JackalRam.ControlSubState == 0) {
    subLoadNewPatternTable(7);               /* End of Game index */
    Label159();
    subEraseAllSpriteData();
    subEraseInGameJeepData();
    Label152(6);
    subInitiateSoundClip(GAMEOVER_CONTINUE_CLIP);   /* stub */
    if (JackalRam.ContinuesUsed < NUMBER_OF_CONTINUES) {
      JackalRam.GameControlState = 8;        /* 还有续关 → Continue 屏（:875-882 三文本） */
      Label152(7);
      Label152(9);
      Label152(0x0A);
      return;
    }
    Label152(5);                             /* 无续关 → 仅 GAME OVER 文本 */
    JackalRam.ScreenTimerUB = 2;
    subIncrementSubGameState_SetScreenTimeLB(0xC0);
    return;
  }
  /* Label797（:890-895）：计时尽 → Label801 回标题 */
  if (fctCountDownScreenTimer() == 0) {
    subWrite_A_ToGameControlState_SetScreenTimer_ClearTitleScreenState(0);  /* Label801（:822） */
  }
}

static const uint8_t tblContinueScreenGrenadeVerticalPosition[2] = {0x84, 0x94};  /* :1112 YES/NO */

static void GameControlState8(void) {  /* :1073-1114：Continue 屏 */
  if (JackalRam.ControlSubState == 0) {
    JackalRam.SpriteTypeIndex[0x10] = 0x08;          /* Jeep1TypeIndex=8：手雷光标 */
    JackalRam.SpriteHorizScreenPosition[0x10] = 0x68;
    if ((JackalRam.JeepControlsInput1Frame[0] & 0x10u) != 0) {   /* StartButton 边沿 */
      if (JackalRam.JeepMainWeapon[0] != 0) {        /* $50=1：NO → Label801 回标题 */
        subWrite_A_ToGameControlState_SetScreenTimer_ClearTitleScreenState(0);
        return;
      }
      JackalRam.ContinuesUsed++;
      label824();                                    /* :1089 JSR Label824 */
      JackalRam.ControlSubState = 1;                 /* :1091 $19=1 */
      subStopMusic();                                /* stub */
      subInitiateSoundClip(0x17u);                   /* MainWeaponExplosionOnEnemy stub */
      /* :1095 JSR + → :1103 光标定位一次；:1096 JMP +++ → :1121（LB=$80、$19++=2） */
      JackalRam.SpriteVertScreenPosition[0x10] =
          tblContinueScreenGrenadeVerticalPosition[JackalRam.JeepMainWeapon[0]];
      JackalRam.ScreenTimerLB = 0x80;
      JackalRam.ControlSubState++;
      return;
    }
    if ((JackalRam.JeepControlsInput1Frame[0] & 0x20u) != 0) {   /* SelectButton 边沿 */
      JackalRam.JeepMainWeapon[0] ^= 1u;             /* YES/NO 切换（$50 借用） */
    }
    JackalRam.SpriteVertScreenPosition[0x10] =
        tblContinueScreenGrenadeVerticalPosition[JackalRam.JeepMainWeapon[0]];
    return;
  }
  /* Label821（:1107-1110）：确认计时尽 → :1110 BEQ --。匿名标号静态解析落在 :1103
     光标段（会死锁，与原版实际行为不符——原版确认后黑屏重开当前关）；按语义落到
     检查点重开：GCS=5/GPM=2（:3499 ContinuesUsed!=0 → InitializeLevel 直达装载）。 */
  if (fctCountDownScreenTimer() == 0) {
    JackalRam.GameControlState = 5;
    JackalRam.GamePlayMode = 2;
  }
}
#define STAGE_COMPLETE_CLIP   0x4Cu    /* StageComplete_LevelSegue_SoundClip（Sound.ASM:93） */

/* ---------------------------------------------------------------- GCS7（:897-1071） */

/* Label806（:2335-2343）：YEAHHHH 场景（pattern $08 + Label925($0E) + palette $39） */
static void Label806(void) {
  subLoadNewPatternTable(8);
  Label925(0x0E);
  Label152(0x39);
}
/* Label804（:2345-2353）：Segue 场景（pattern $0A + Label925($12) + palette $65） */
static void Label804(void) {
  subLoadNewPatternTable(0x0A);
  Label925(0x12);
  Label152(0x65);
}
/* Label808（:2355-2364）：Segue 图 + VScroll 归 $E8 */
static void Label808(void) {
  Label925(0x0C);
  Label152(0x3A);
  JackalRam.ScreenVerticalScrollPosition_PPU = 0xE8;
}

/* subAdd2DigitTextToPPUUpdate（:2502-2518）：BCD 字节拆两位，各 +1 写入队列 */
static void subAdd2DigitTextToPPUUpdate(uint8_t b) {
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = (uint8_t)((b >> 4) + 1u);
  JackalRam.PPUGraphicsUpdateByteLength++;
  JackalRam.PPUUpdateQueue[JackalRam.PPUGraphicsUpdateByteLength] = (uint8_t)((b & 0x0Fu) + 1u);
  JackalRam.PPUGraphicsUpdateByteLength++;
}

/* subDisplayPlayersScore_LevelSegue（:2535-2551）：Label152($0B/$0C) + 双分数 6 位 */
static void subDisplayPlayersScore_LevelSegue(void) {
  int8_t y;
  Label152((uint8_t)((JackalRam.RNG_INCEveryFrame & 1u) + 0x0Bu));
  for (y = 2; y >= 0; y--) {
    uint8_t b = (JackalRam.RNG_INCEveryFrame & 1u) != 0
        ? JackalRam.Jeep2Score[y] : JackalRam.Jeep1Score[y];   /* :2544 帧交替 P1/P2 */
    subAdd2DigitTextToPPUUpdate(b);
  }
  subInsertPPUUpdateTerminator(&JackalRam.PPUGraphicsUpdateByteLength);
}

/* subDisplayPlayersPOWSavedCount_LevelSegue（:2520-2533）：Label152($37/$38) + POW 数 */
static void subDisplayPlayersPOWSavedCount_LevelSegue(void) {
  uint8_t b = (JackalRam.RNG_INCEveryFrame & 1u) != 0
      ? JackalRam.Jeep2POWHeliDropOffCount : JackalRam.Jeep1POWHeliDropOffCount;
  Label152((uint8_t)((JackalRam.RNG_INCEveryFrame & 1u) + 0x37u));
  subAdd2DigitTextToPPUUpdate(b);
  subInsertPPUUpdateTerminator(&JackalRam.PPUGraphicsUpdateByteLength);
}

static void StageTransitionState0(void) {  /* :914-942：黑屏 + 场景装载 */
  Label159();
  subEraseAllSpriteData();
  if (JackalRam.JeepMainWeapon[0] == 0) {        /* :917 $50（P1 主武器）==0 → Label804 */
    Label804();
  } else {                                       /* !=0 → Label806（YEAHHHH）+ 精灵排布 */
    Label806();
    JackalRam.SpriteHorizScreenPosition[1] = 0x80;   /* $05A1 */
    JackalRam.SpriteVertScreenPosition[1] = 0x7F;    /* $0561 */
    JackalRam.SpriteTypeIndex[1] = 0xB3;             /* $0501 */
    JackalRam.SpriteTypeIndex[2] =
        (JackalRam.JeepMainWeapon[0] < 2u) ? 0xC9u : 0xCAu;   /* :929-933 $50>=2 → $CA */
    JackalRam.SpriteHorizScreenPosition[2] = 0x88;   /* $05A2 */
    JackalRam.SpriteVertScreenPosition[2] = 0x28;    /* $0562 */
  }
  subStopMusic();                                /* stub */
  subInitiateSoundClip(STAGE_COMPLETE_CLIP);     /* stub */
  subIncrementSubGameState_SetScreenTimeLB(0x88);
}

static void StageTransitionState1(void) {  /* :944-985：场景尾 + POW 图标/小吉普/地图精灵 */
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  Label808();
  JackalRam.SpriteTypeIndex[0] = 0xB8;         /* POW 图标两枚 */
  JackalRam.SpriteTypeIndex[1] = 0xB8;
  JackalRam.SpriteGraphicsAttributes[0] = 0;
  JackalRam.SpriteGraphicsAttributes[1] = 0;
  JackalRam.SpriteHorizScreenPosition[0] = 0x90;
  JackalRam.SpriteHorizScreenPosition[1] = 0x90;
  JackalRam.SpriteVertScreenPosition[0] = 0x58;
  JackalRam.SpriteVertScreenPosition[1] = 0xA0;
  JackalRam.SpriteTypeIndex[2] = 0xB2;         /* 小吉普 */
  JackalRam.SpriteHorizScreenPosition[2] = 0x49;
  JackalRam.SpriteVertScreenPosition[2] = 0xD8;
  JackalRam.SpriteHorizScreenPosition[3] = 0x30;   /* 关底地图精灵 */
  JackalRam.SpriteVertScreenPosition[3] = 0x77;
  JackalRam.SpriteTypeIndex[3] = 0xB0;
  subIncrementSubGameState_SetScreenTimeLB(0x48);
}

static void StageTransitionState2(void) {  /* :987-994：分数/POW 显示 + 计时 */
  subDisplayPlayersScore_LevelSegue();
  subDisplayPlayersPOWSavedCount_LevelSegue();
  if (fctCountDownScreenTimer() == 0) {
    subIncrementSubGameState_SetScreenTimeLB(0xD0);
  }
}

static const uint8_t tblLittleJeepStopPoints[6] = {0xC0, 0xA0, 0x80, 0x60, 0x40, 0x20};  /* :996 */

static void StageTransitionState3(void) {  /* :999-1051：小吉普上行 + POW 计数结算 */
  int8_t x;
  if (JackalRam.SpriteVertScreenPosition[2] >= tblLittleJeepStopPoints[JackalRam.CurrentLevel]) {
    JackalRam.SpriteVertScreenPosition[2]--;   /* :1004 小吉普上移 1px/帧（:1003 BCC 跳过） */
  }
  subDisplayPlayersScore_LevelSegue();
  subDisplayPlayersPOWSavedCount_LevelSegue();
  if ((uint8_t)(JackalRam.JeepPOWHeliDropOffCount[0] | JackalRam.JeepPOWHeliDropOffCount[1]) == 0) {
    /* Label813（:1047-1051）：结算完 → 计时尽 → State4 */
    if (fctCountDownScreenTimer() == 0) {
      subIncrementSubGameState_SetScreenTimeLB(0x94);
    }
    return;
  }
  (void)fctCountDownScreenTimer();             /* :1010 计时只作步进，结果不判 */
  if ((JackalRam.RNG_INCEveryFrame & 7u) != 0) {
    return;                                    /* 每 8 帧结算一轮 */
  }
  for (x = 1; x >= 0; x--) {
    uint8_t c;
    if (JackalRam.JeepPOWHeliDropOffCount[x] == 0) {
      continue;
    }
    c = (uint8_t)(JackalRam.JeepPOWHeliDropOffCount[x] - 1u);
    if ((c & 0x0Fu) >= 0x0Au) {                /* BCD 借位调整：低半 >=$0A 再减 6（:1020-1026） */
      c = (uint8_t)(c - 6u);
    }
    JackalRam.JeepPOWHeliDropOffCount[x] = c;
    {
      uint8_t add[3] = {0x00, 0x20, 0x00};     /* $01=$00/$02=$20（:1582 注） */
      subAddToPlayerScore((uint8_t)x, add);    /* 每只 POW +$20 mid */
    }
  }
  if ((uint8_t)(JackalRam.JeepPOWHeliDropOffCount[0] | JackalRam.JeepPOWHeliDropOffCount[1]) == 0 &&
      JackalRam.ScreenTimerLB == 0) {
    JackalRam.ScreenTimerLB = 0x60;            /* :1041-1044 结算完重启延时 */
  }
}

/* subTransitionFromEndOfGameToLevel1（:1059-1071）：CurrentLevel++（==6 回 0）、
   GCS=5/GPM=1——StageTransitionState4 与 EndOfGameState11 共用 */
void subTransitionFromEndOfGameToLevel1(void) {
  JackalRam.CurrentLevel++;
  if (JackalRam.CurrentLevel >= 6u) {
    JackalRam.CurrentLevel = 0;                /* :1064 六关完成回 Level 1 */
  }
  JackalRam.GameControlState = 5;
  JackalRam.GamePlayMode = 1;
}

static void StageTransitionState4(void) {  /* :1053-1058：计时尽 → 次关 */
  if (fctCountDownScreenTimer() != 0) {
    return;
  }
  subTransitionFromEndOfGameToLevel1();
}

static void GameControlState7(void) {  /* :897-913：结局判 + $19 子状态分发 */
  static void (*const tbl[5])(void) = {
    StageTransitionState0, StageTransitionState1, StageTransitionState2,
    StageTransitionState3, StageTransitionState4,
  };
  if (JackalRam.CurrentLevel == 5u) {
    JackalRam.GameControlState = 9;            /* :898-903 L6 完成 → 结局 */
    return;
  }
  tbl[JackalRam.ControlSubState]();
}
static void GameControlState9(void) {  /* :1116-1119：结局（Bank0，Task 7.4 实装） */
  /* LDY #$00 + subChangeBank：镜像模型无副作用（BankPtr(0) 直读） */
  subProcessEndOfGameLogic();
}

void subProcessGameControl(void) {  /* :625 */
  static void (*const tbl[10])(void) = {
    GameControlState0, GameControlState1, GameControlState2, GameControlState3,
    GameControlState4, GameControlState5, GameControlState6, GameControlState7,
    GameControlState8, GameControlState9,
  };
  JackalRam.RNG_INCEveryFrame++;
  if (JackalRam.GameControlState < 3) {
    Label774();
  }
  tbl[JackalRam.GameControlState]();
}

/* ---------------------------------------------------------------- RESET / NMI */

void JackalReset(void) {  /* RESET_VECTOR（:47-114） */
  subSetPPUToBlackScreen();
  JackalRamInit();                 /* :66-78 RAM 清零（镜像模型含 $700 区，ASM quirk 段见头注） */
  JackalRam.HighScore[1] = 0x50;   /* HighScore_xx99xx=$50 → $5000（:107） */
  InitPPU();                       /* subInitAPUAndPPU 尾（APU 写无镜像副作用） */
  subStopMusic();
  traceResetTrack();
}

/* subProcessScreenScrolling（:204-238）实装在 scroll.c；NMI 两处调用点与 ASM 一致。 */

void JackalNmiFrame(const JACKAL_INPUT *in) {  /* NMI_VECTOR（:125-166） */
  if (JackalRam.GameFrameLogicInProgress != 0) {
    /* lag 帧（:178-181）：只滚屏+声音。同步模型自然不发生，语义保留。 */
    subProcessScreenScrolling();
    subProcessSound_Music();
    return;
  }
  /* PPUADDR/PPUMASK/OAMADDR/OAMDMA 硬件写无镜像副作用（OAM shadow→屏由 render 阶段消费） */
  subInGamePPUUpdates();
  if (JackalRam.ScreenTransitionTimer != 0) {
    JackalRam.ScreenTransitionTimer--;
    /* DEC 后≠0 → PPUMASK=0 黑屏；==0 → NormalPPUMASK。渲染规则（Task 2.7）：
       本帧黑屏 ⟺ 此刻 ScreenTransitionTimer≠0。 */
  }
  subProcessScreenScrolling();
  JackalRam.GameFrameLogicInProgress++;
  subProcessSound_Music();
  subProcessControllerInputs(in->Current, 0);  /* UEFI 单键盘：pad2 恒 0 */
  /* LDY #$01 + subChangeBank + JSR subProcessSpriteUpdates（:154-156）：Bank1 精灵更新。
     UEFI 侧窗口恒跟 CurrentBank（bank.h），切 bank 无硬件动作，直接调用；
     表/元精灵数据经 BankPtr(1,...) 读 Bank1 镜像。 */
  subProcessSpriteUpdates();
  subProcessGameControl();
  Label757();                       /* 队列尾写 0（:2540 区域） */
  JackalRam.GameFrameLogicInProgress = 0;
  JackalTraceState();
#if defined(JACKAL_DEBUG_AUTO_START) || defined(JACKAL_DEBUG_AUTOSCROLL)
  /* 滚屏停滞诊断（非原版行为，仅验证构建）：每 120 帧输出滚屏关键状态 */
  {
    static uint16_t sDbgFrame;
    static const char hexd[] = "0123456789ABCDEF";
    char buf[80];
    char *p;
    const char *s;
    uint8_t v;
    sDbgFrame++;
    if (sDbgFrame % 120u == 0 && JackalTraceHook != 0) {
      p = buf;
      s = "SCROLL sc=";
      while (*s != 0) { *p++ = *s++; }
      v = JackalRam.CurrentLevelScreen;           *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " sub=";  while (*s != 0) { *p++ = *s++; }
      v = JackalRam.CurrentLevelScreenSubPosition; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " ppu=";  while (*s != 0) { *p++ = *s++; }
      v = JackalRam.ScreenVerticalScrollPosition_PPU; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " f0=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.ScreenScrollingForF0ToBoss;   *p++ = hexd[v & 0xF];
      s = " lock="; while (*s != 0) { *p++ = *s++; }
      v = JackalRam.ScreenVerticalScrollLockForBossFight; *p++ = hexd[v & 0xF];
      s = " gpm=";  while (*s != 0) { *p++ = *s++; }
      v = JackalRam.GamePlayMode;                 *p++ = hexd[v & 0xF];
      s = " jv=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.JeepVertPosition[0];          *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " js=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.SpriteState[0x10];            *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " ci=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.JeepControlsInput[0];         *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " td=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.JeepDirectionTendency[0];     *p++ = hexd[v & 0xF];
      s = " fc=";   while (*s != 0) { *p++ = *s++; }
      v = JackalRam.JeepFacingDirection[0];       *p++ = hexd[v & 0xF];
      s = " w=";    while (*s != 0) { *p++ = *s++; }
      v = JackalRam.SpriteState[0x18];            *p++ = hexd[v & 0xF];
      v = JackalRam.SpriteHorizScreenPosition[0x18]; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      v = JackalRam.SpriteVertScreenPosition[0x18];  *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " z47=";  while (*s != 0) { *p++ = *s++; }
      v = JackalRam.Zp47;                         *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " z48=";  while (*s != 0) { *p++ = *s++; }
      v = JackalRam.Zp48;                         *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " qlen="; while (*s != 0) { *p++ = *s++; }
      v = JackalRam.PPUGraphicsUpdateByteLength;  *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
      s = " vsum="; while (*s != 0) { *p++ = *s++; }
      {
        uint16_t sum = 0;
        uint16_t i;
        for (i = 0; i < 0x40u; i++) {   /* $2000 首行 64 字节校验和 */
          sum = (uint16_t)(sum + JackalVram[0x2000u + i]);
        }
        *p++ = hexd[(sum >> 12) & 0xF]; *p++ = hexd[(sum >> 8) & 0xF];
        *p++ = hexd[(sum >> 4) & 0xF];  *p++ = hexd[sum & 0xF];
      }
      *p = 0;
      JackalTraceHook(buf);
    }
    /* boss 战精灵槽转储：锁定后每 120 帧列出活动对象（ID/状态/屏幕坐标） */
    if (sDbgFrame % 120u == 0 && JackalTraceHook != 0 &&
        JackalRam.ScreenVerticalScrollLockForBossFight != 0) {
      uint8_t i;
      for (i = 0; i < 0x10u; i++) {
        if (JackalRam.SpriteObjectID[i] == 0) {
          continue;
        }
        p = buf;
        s = "OBJ s=";
        while (*s != 0) { *p++ = *s++; }
        *p++ = hexd[i & 0xF];
        s = " id="; while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteObjectID[i];            *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        s = " st="; while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteState[i];               *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        s = " x=";  while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteHorizScreenPosition[i]; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        s = " y=";  while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteVertScreenPosition[i];  *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        s = " hp="; while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteHealthHP[i];            *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        s = " ti="; while (*s != 0) { *p++ = *s++; }
        v = JackalRam.SpriteTypeIndex[i];           *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
        *p = 0;
        JackalTraceHook(buf);
      }
      /* OAM 前 12 项转储（tile/attr/x/y）——识别实际渲染内容 */
      {
        uint8_t n;
        for (n = 0; n < 12u; n++) {
          p = buf;
          s = "OAM n=";
          while (*s != 0) { *p++ = *s++; }
          *p++ = hexd[n & 0xF];
          s = " y=";  while (*s != 0) { *p++ = *s++; }
          v = JackalRam.OamShadow[n * 4u];      *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
          s = " t=";  while (*s != 0) { *p++ = *s++; }
          v = JackalRam.OamShadow[n * 4u + 1u]; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
          s = " a=";  while (*s != 0) { *p++ = *s++; }
          v = JackalRam.OamShadow[n * 4u + 2u]; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
          s = " x=";  while (*s != 0) { *p++ = *s++; }
          v = JackalRam.OamShadow[n * 4u + 3u]; *p++ = hexd[(v >> 4) & 0xF]; *p++ = hexd[v & 0xF];
          *p = 0;
          JackalTraceHook(buf);
        }
      }
    }
  }
#endif
}
