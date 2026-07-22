#ifndef JACKAL_GOP_H
#define JACKAL_GOP_H

#include <Uefi.h>

#define JACKAL_STAGE_W  512
#define JACKAL_STAGE_H  480

EFI_STATUS GopInit (VOID);
VOID       GopPresent (IN CONST UINT32 *LogicalFb);

#endif
