#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>

#include "../Jackal.h"
#include "gop.h"

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL  *mGop;
STATIC UINT32  *mStage;
STATIC UINTN   mOffX, mOffY;
STATIC BOOLEAN mSwapRb;

EFI_STATUS
GopInit (
  VOID
  )
{
  EFI_STATUS                            Status;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  UINTN                                 Size;
  UINT32                                Mode, BestMode;
  UINT64                                BestPixels;
  BOOLEAN                               Found;

  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BestMode   = mGop->Mode->Mode;
  BestPixels = (UINT64)-1;
  Found      = FALSE;
  for (Mode = 0; Mode < mGop->Mode->MaxMode; Mode++) {
    Status = mGop->QueryMode (mGop, Mode, &Size, &Info);
    if (EFI_ERROR (Status)) {
      continue;
    }
    if (Info->HorizontalResolution >= JACKAL_STAGE_W &&
        Info->VerticalResolution   >= JACKAL_STAGE_H) {
      UINT64 Pixels = (UINT64)Info->HorizontalResolution * Info->VerticalResolution;
      if (Pixels < BestPixels) {
        BestPixels = Pixels;
        BestMode   = Mode;
        Found      = TRUE;
      }
    }
  }
  if (!Found) {
    return EFI_UNSUPPORTED;
  }
  if (BestMode != mGop->Mode->Mode) {
    Status = mGop->SetMode (mGop, BestMode);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  if (mGop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
    mSwapRb = TRUE;
  } else if (mGop->Mode->Info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) {
    return EFI_UNSUPPORTED;
  }

  mStage = AllocatePool (JACKAL_STAGE_W * JACKAL_STAGE_H * sizeof (UINT32));
  if (mStage == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  mOffX = (mGop->Mode->Info->HorizontalResolution - JACKAL_STAGE_W) / 2;
  mOffY = (mGop->Mode->Info->VerticalResolution   - JACKAL_STAGE_H) / 2;

  // 整屏填黑，保证放大区以外是黑边
  {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = { 0, 0, 0, 0 };
    mGop->Blt (mGop, &Black, EfiBltVideoFill, 0, 0, 0, 0,
               mGop->Mode->Info->HorizontalResolution,
               mGop->Mode->Info->VerticalResolution, 0);
  }
  return EFI_SUCCESS;
}

VOID
GopPresent (
  IN CONST UINT32 *LogicalFb
  )
{
  UINTN  x, y;
  UINT32 c;

  for (y = 0; y < JACKAL_SCREEN_H; y++) {
    UINT32 *Row0 = mStage + (y * 2) * JACKAL_STAGE_W;
    UINT32 *Row1 = Row0 + JACKAL_STAGE_W;
    for (x = 0; x < JACKAL_SCREEN_W; x++) {
      c = LogicalFb[y * JACKAL_SCREEN_W + x];
      if (mSwapRb) {
        c = (c & 0x0000FF00u) | ((c & 0x000000FFu) << 16) | ((c & 0x00FF0000u) >> 16);
      }
      Row0[x * 2]     = c;
      Row0[x * 2 + 1] = c;
      Row1[x * 2]     = c;
      Row1[x * 2 + 1] = c;
    }
  }
  mGop->Blt (mGop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)mStage, EfiBltBufferToVideo,
             0, 0, mOffX, mOffY, JACKAL_STAGE_W, JACKAL_STAGE_H,
             JACKAL_STAGE_W * sizeof (UINT32));
}
