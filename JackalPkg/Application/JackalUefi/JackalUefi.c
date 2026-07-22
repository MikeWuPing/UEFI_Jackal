#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/SerialPortLib.h>

#include "Version.h"
#include "Jackal.h"
#include "platform/gop.h"
#include "platform/input.h"
#include "platform/timer.h"
#include "core/game_control.h"
#include "core/ram.h"
#include "core/render.h"
#include "core/trace.h"

/* 自动验收帧计数（仅在验证构建开关下使用） */
#if defined(JACKAL_DEBUG_AUTO_START) || defined(JACKAL_DEBUG_AUTOSCROLL)
STATIC UINTN gVerifyFrame = 0;
/* 第 N 帧注入 Start（标题已稳定处于 GCS1） */
#define JACKAL_DEBUG_AUTO_START_FRAME  30

/* 非原版行为，仅自动验收用：boss 战模拟玩家驾驶器——只合成 DPad/A 键输入
   （胜负仍走真实碰撞/HP/死亡系统）。策略：与最近 boss 坦克保持 ~$38 距离
   （近了退、远了绕场走）、对齐（|dx| 或 |dy| < $0C）时面向坦克并射主武器。 */
STATIC
VOID
DebugBossAssistDrive (
  IN OUT JACKAL_INPUT *Input
  )
{
  INT16   dx = 0, dy = 0;
  INT16   adx = 0x7FFF, ady = 0x7FFF;
  UINT8   i;
  BOOLEAN found = FALSE;
  STATIC UINTN fireCd = 0;

  /* 非原版行为，仅自动验收用：命数兜底——敌弹修复后（label979 $35 父槽修复）
     炮台/步兵火力真实化，AUTOSCROLL 钉顶吉普成为弹靶，会在 boss 前耗尽命数
     GCS6 出局。爆炸/死亡/重生仍走真实路径，仅防 Game Over 阻断验收目标。 */
  if (JackalRam.Jeep1LifeCount < 3u) {
    JackalRam.Jeep1LifeCount = 5;
  }

  if (JackalRam.GamePlayMode != 3 ||
      JackalRam.ScreenVerticalScrollLockForBossFight == 0 ||
      JackalRam.SpriteState[0x10] != 1) {
    return;   /* 非 boss 战或吉普非正常态不接管 */
  }
  for (i = 0; i < 0x10u; i++) {
    INT16 tx, ty;
    if ((JackalRam.SpriteObjectID[i] & 0x7Fu) != 0x0Au ||
        (int8_t)JackalRam.SpriteState[i] < 0) {
      continue;
    }
    tx = (INT16)(INT8)(JackalRam.SpriteHorizScreenPosition[i] -
                       JackalRam.SpriteHorizScreenPosition[0x10]);
    ty = (INT16)(INT8)(JackalRam.SpriteVertScreenPosition[i] -
                       JackalRam.SpriteVertScreenPosition[0x10]);
    if (tx < 0) { tx = (INT16)-tx; }
    if (ty < 0) { ty = (INT16)-ty; }
    if (tx + ty < adx + ady) {
      dx = (INT16)(INT8)(JackalRam.SpriteHorizScreenPosition[i] -
                         JackalRam.SpriteHorizScreenPosition[0x10]);
      dy = (INT16)(INT8)(JackalRam.SpriteVertScreenPosition[i] -
                         JackalRam.SpriteVertScreenPosition[0x10]);
      adx = tx; ady = ty;
      found = TRUE;
    }
  }
  if (!found) {
    return;
  }
  Input->Current &= 0xF0u;   /* 清 DPad：方向由驾驶器合成 */
  if (adx < 0x0Cu || ady < 0x0Cu) {
    /* 对齐：面向坦克（tendency 决定手雷方向） */
    if (adx < ady) {
      Input->Current |= (dy > 0) ? NES_BTN_DOWN : NES_BTN_UP;
    } else {
      Input->Current |= (dx > 0) ? NES_BTN_RIGHT : NES_BTN_LEFT;
    }
    if (fireCd == 0) {
      Input->Current |= NES_BTN_A;
      fireCd = 28;
    }
  } else if (adx < 0x28u && ady < 0x28u) {
    /* 太近：垂直于坦克方向撤退 */
    if (adx < ady) {
      Input->Current |= (dx > 0) ? NES_BTN_LEFT : NES_BTN_RIGHT;
    } else {
      Input->Current |= (dy > 0) ? NES_BTN_UP : NES_BTN_DOWN;
    }
  } else {
    /* 太远：回场心附近（x=$80/y=$90），给手雷制造射程 */
    if (JackalRam.SpriteHorizScreenPosition[0x10] < 0x78u) {
      Input->Current |= NES_BTN_RIGHT;
    } else if (JackalRam.SpriteHorizScreenPosition[0x10] > 0x88u) {
      Input->Current |= NES_BTN_LEFT;
    } else if (JackalRam.SpriteVertScreenPosition[0x10] < 0x88u) {
      Input->Current |= NES_BTN_DOWN;
    } else {
      Input->Current |= NES_BTN_UP;
    }
  }
  if (fireCd != 0) {
    fireCd--;
  }
}
#endif

#ifdef JACKAL_DEBUG_FORCE_ENDING
STATIC UINTN gEndingFrame = 0;
/* 非原版行为，仅自动验收用：第 N 帧直驱 GCS=9 结局机（L4/L5 按用户指示跳过，
   连续通关无法到达 L6 关底；结局机完整逻辑链由宿主 test_end_of_game 锚定） */
#define JACKAL_DEBUG_FORCE_ENDING_FRAME  45
#endif

STATIC
VOID
SerialPrintAscii (
  IN CONST CHAR8 *Str
  )
{
  SerialPortWrite ((UINT8 *)Str, AsciiStrLen (Str));
}

STATIC
VOID
SerialPrintLine (
  IN CONST CHAR8 *Str
  )
{
  SerialPrintAscii (Str);
  SerialPrintAscii ("\r\n");
}

STATIC
EFI_STATUS
Fail (
  IN CONST CHAR8 *Stage,
  IN EFI_STATUS  Status
  )
{
  CHAR8 Buf[64];

  AsciiSPrint (Buf, sizeof (Buf), "ERROR %a 0x%lx\r\n", Stage, (UINT64)Status);
  SerialPrintAscii (Buf);
  Print (L"ERROR %a 0x%lx\n", Stage, (UINT64)Status);
  gBS->Stall (5000000);
  return Status;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS   Status;
  JACKAL_INPUT Input;

  SerialPrintAscii ("APP_VERSION=" APP_VERSION_ASCII "\r\n");
  Print (L"JackalUefi %s\n", APP_VERSION_STRING);

  Status = InputInit ();
  if (EFI_ERROR (Status)) {
    return Fail ("INPUT", Status);
  }
  Status = GopInit ();
  if (EFI_ERROR (Status)) {
    return Fail ("GOP", Status);
  }
  Status = TimerInit ();
  if (EFI_ERROR (Status)) {
    return Fail ("TIMER", Status);
  }
  JackalTraceHook = SerialPrintLine;
  JackalReset ();

  for (;;) {
    TimerWaitFrame ();
    InputPoll (&Input);
    if (Input.Escape ||
        ((Input.Current & NES_BTN_SELECT) && (Input.Current & NES_BTN_START))) {
      break;
    }
#ifdef JACKAL_DEBUG_AUTO_START
    /* 非原版行为，仅自动验收用：第 N 帧自动注入 Start 边沿（标题→开局） */
    gVerifyFrame++;
    if (gVerifyFrame == JACKAL_DEBUG_AUTO_START_FRAME) {
      Input.Current |= NES_BTN_START;
    }
#endif
#ifdef JACKAL_DEBUG_FORCE_ENDING
    /* 非原版行为，仅自动验收用：第 N 帧直驱 GCS=9 结局机 */
    gEndingFrame++;
    if (gEndingFrame == JACKAL_DEBUG_FORCE_ENDING_FRAME) {
      JackalRam.CurrentLevel = 5;              /* State11 回绕语义（5+1=6→0） */
      JackalRam.GameControlState = 9;
      JackalRam.ControlSubState = 0;
    }
#endif
#if defined(JACKAL_DEBUG_AUTO_START) || defined(JACKAL_DEBUG_AUTOSCROLL)
    DebugBossAssistDrive (&Input);   /* 输入注入点：须在 JackalNmiFrame 消费前 */
#endif
    JackalNmiFrame (&Input);
#ifdef JACKAL_DEBUG_AUTOSCROLL
    /* 非原版行为，仅自动验收用：GPM3 中模拟玩家贴顶按住 Up——JeepVertPosition 压到 $5F，
       Label1096（原版滚动协同）每帧贴回 $60 并推进 SubPosition/screen（滚屏走原版路径）。
       boss 锁滚后释放（钉顶会被 boss 围殴致死，无法验证 GCS7；原版玩家此时自由走位） */
    if (JackalRam.GamePlayMode == 3 && JackalRam.ScreenVerticalScrollLockForBossFight == 0) {
      JackalRam.JeepVertPosition[0] = 0x5F;
    }
#endif
#ifdef JACKAL_DEBUG_MAX_WEAPON
    /* 非原版行为，仅试玩便利用（用户点单）：双吉普主武器恒满级 3（火箭筒+横纵溅射），
       难度系数同步置顶（:5813-5819/:4857-4863 武器-难度联动语义）；死亡归零下帧即恢复。
       命数恒 99（HUD 单 digit 超界显示为花屏字符，仅外观）；GCS6 双亡检查恒不触发。 */
    JackalRam.JeepMainWeapon[0] = 3;
    JackalRam.JeepMainWeapon[1] = 3;
    JackalRam.DifficultyBasedOnWeapon = 3;
    JackalRam.Jeep1LifeCount = 99;
    JackalRam.Jeep2LifeCount = 99;
#endif
    JackalRenderFrame ();
    GopPresent (JackalRenderGetFb ());
  }

  SerialPrintAscii ("EXIT\r\n");
  return EFI_SUCCESS;
}
