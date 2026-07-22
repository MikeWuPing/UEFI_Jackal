/* JACKAL_BANK：UxROM（mapper 2）bank 窗口语义。
   硬件上切 bank 是对 $8000,Y 先读后写两次（Bank7.ASM:296-306），UEFI 侧归约为
   对 JackalPrgBanks 全 PRG 镜像的索引：$8000-$BFFF 窗口恒跟 JackalRam.CurrentBank，
   $C000-$FFFF 恒为 Bank7。跨 bank 数据访问一律走 BankPtr 显式指定 bank，
   因此 Bypass/UseCurrentBank 在 UEFI 侧不改变任何状态（保留调用点与汇编一致）。
   前置条件：cpu 地址必须落在 $8000-$FFFF（NES PRG 窗口）；越界调用是翻译错误。 */
#ifndef JACKAL_CORE_BANK_H
#define JACKAL_CORE_BANK_H

#include <stdint.h>

/* $8000-$BFFF → CurrentBank 窗口；$C000-$FFFF → Bank7 固定窗口 */
uint8_t BankRead8(uint16_t cpu);

/* 显式 bank 取址：bank 0-6 对应 cpu ∈ [$8000,$BFFF]，bank 7 对应 cpu ∈ [$C000,$FFFF] */
const uint8_t *BankPtr(uint8_t bank, uint16_t cpu);

/* Bank7.ASM:296 subChangeBank_YhasBank：STY CurrentBank 后落入 UseCurrentBank */
void subChangeBank_YhasBank(uint8_t bank);

/* Bank7.ASM:299 subChangeBank_UseCurrentBank：LDY CurrentBank 后落入 Bypass。
   UEFI 侧窗口恒跟 CurrentBank，无硬件动作；保留函数以便调用点逐行对照。 */
void subChangeBank_UseCurrentBank(void);

/* Bank7.ASM:302 subChangeBank_YhasBank_BypassCurrentBank：硬件切窗口但不写 CurrentBank。
   UEFI 侧跨 bank 读走 BankPtr(bank, addr)，窗口状态无对应物；(void)bank。 */
void subChangeBank_YhasBank_BypassCurrentBank(uint8_t bank);

#endif
