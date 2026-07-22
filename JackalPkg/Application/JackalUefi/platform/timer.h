#ifndef JACKAL_TIMER_H
#define JACKAL_TIMER_H

#include <Uefi.h>

EFI_STATUS TimerInit (VOID);
VOID       TimerWaitFrame (VOID);

#endif
