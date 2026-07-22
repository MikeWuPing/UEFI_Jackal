/* platform/input.c：UEFI 键盘输入 → NES 手柄位（JACKAL_INPUT.Current 按住语义）。
   平台约束（已查证）：EDK2 PS/2 键盘驱动（Ps2KbdTextIn/Ps2KbdCtrller）只上报
   按下事件，不处理 PS/2 break code（0xF0 前缀的松开事件）——UEFI 控制台层面
   拿不到任意键的真实松开。SimpleTextInputEx 的 RegisterKeyNotify 同源（也只给按下）。
   因此按住只能靠键盘 typematic 自动重复（QEMU 约 500ms 首延 + ~10/s 连发）。
   模型：每次按键流（stroke）把对应键的"剩余按住帧"充到 HOLD_FRAMES——
   连发持续补给 → 长按持续按住；松开后自然衰减 ~0.5s（尾巴可接受）。
   这修复了旧模型（"本帧见到的键=按住"，帧覆盖 <5%，方向键实测失效）的问题。 */
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "../Jackal.h"
#include "input.h"

#define NES_KEY_SLOTS 8
#define HOLD_FRAMES   20   /* ~0.5s@37fps：覆盖 typematic 首延与连发间隔 */

STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL *mIn;
STATIC UINT8 mHold[NES_KEY_SLOTS + 1];   /* 各键剩余按住帧（槽 1-8 使用，0 无效） */
STATIC UINT8 mEscCount;              /* ESC 剩余（退出语义） */
STATIC UINT8 mPrev;

/* 键 → 槽位（0=无效） */
STATIC
UINT8
MapKeySlot (
  IN CONST EFI_INPUT_KEY *Key
  )
{
  if (Key->ScanCode == SCAN_RIGHT)      { return 1; }   /* NES_BTN_RIGHT */
  if (Key->ScanCode == SCAN_LEFT)       { return 2; }   /* NES_BTN_LEFT */
  if (Key->ScanCode == SCAN_DOWN)       { return 3; }   /* NES_BTN_DOWN */
  if (Key->ScanCode == SCAN_UP)         { return 4; }   /* NES_BTN_UP */
  if (Key->UnicodeChar == CHAR_CARRIAGE_RETURN) { return 5; }   /* START */
  if (Key->UnicodeChar == CHAR_BACKSPACE)       { return 6; }   /* SELECT */
  if (Key->UnicodeChar == L'z' || Key->UnicodeChar == L'Z') { return 7; }   /* B */
  if (Key->UnicodeChar == L'x' || Key->UnicodeChar == L'X') { return 8; }   /* A */
  return 0;
}

STATIC CONST UINT8 kSlotBit[NES_KEY_SLOTS + 1] = {
  0,
  NES_BTN_RIGHT, NES_BTN_LEFT, NES_BTN_DOWN, NES_BTN_UP,
  NES_BTN_START, NES_BTN_SELECT, NES_BTN_B, NES_BTN_A,
};

EFI_STATUS
InputInit (
  VOID
  )
{
  UINT8 i;
  mIn = gST->ConIn;
  if (mIn == NULL) {
    return EFI_UNSUPPORTED;
  }
  mIn->Reset (mIn, FALSE);
  for (i = 0; i <= NES_KEY_SLOTS; i++) {
    mHold[i] = 0;
  }
  mEscCount = 0;
  mPrev = 0;
  return EFI_SUCCESS;
}

VOID
InputPoll (
  OUT JACKAL_INPUT *Out
  )
{
  EFI_INPUT_KEY Key;
  UINT8         slot;
  UINT8         cur;
  UINT8         i;

  /* 按键流补给：typematic 连发在此持续充帧 */
  while (!EFI_ERROR (mIn->ReadKeyStroke (mIn, &Key))) {
    if (Key.ScanCode == SCAN_ESC) {
      mEscCount = HOLD_FRAMES;
      continue;
    }
    slot = MapKeySlot (&Key);
    if (slot != 0) {
      mHold[slot] = HOLD_FRAMES;
    }
  }
  /* 衰减合成：countdown>0 的键 = 当前按住 */
  cur = 0;
  for (i = 1; i <= NES_KEY_SLOTS; i++) {
    if (mHold[i] != 0) {
      cur |= kSlotBit[i];
      mHold[i]--;
    }
  }
  Out->Pressed = (UINT8)(cur & (UINT8)~mPrev);
  Out->Current = cur;
  Out->Escape  = (mEscCount != 0) ? 1u : 0u;
  if (mEscCount != 0) {
    mEscCount--;
  }
  mPrev = cur;
}
