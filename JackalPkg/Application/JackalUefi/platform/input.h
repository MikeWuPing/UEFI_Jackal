#ifndef JACKAL_INPUT_H
#define JACKAL_INPUT_H

#include <Uefi.h>
#include "../Jackal.h"

EFI_STATUS InputInit (VOID);
VOID       InputPoll (OUT JACKAL_INPUT *Out);

#endif
