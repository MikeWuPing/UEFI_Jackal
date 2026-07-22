#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

#include "timer.h"

STATIC EFI_EVENT mTimer;

EFI_STATUS
TimerInit (
  VOID
  )
{
  EFI_STATUS Status;

  Status = gBS->CreateEvent (EVT_TIMER, TPL_APPLICATION, NULL, NULL, &mTimer);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  return gBS->SetTimer (mTimer, TimerPeriodic, 166667);
}

VOID
TimerWaitFrame (
  VOID
  )
{
  UINTN Index;
  gBS->WaitForEvent (1, &mTimer, &Index);
}
