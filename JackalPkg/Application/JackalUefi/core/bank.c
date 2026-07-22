/* JACKAL_BANK 实现 + JACKAL_RAM 全局实例。
   core 纯 C99 约定：只用 <stdint.h>/<stddef.h>，不依赖 UEFI 头文件，也不用 <string.h>
   （EDK2 应用默认无 C 标准库头；2KB RAM 清零用显式循环，宿主 cl 与 EDK2 工具链通吃）。
   注意：本文件注释不得出现 U-e-f-i-.h 连写——run_tests.py 按该子串过滤平台层源文件。
   Label844（清 $23-$DF 与 $0300-$06FF + JSR InitPPU，Bank7.ASM:1387-1409）的定义
   随 Task 2.5 落在 loader.c——它的收尾动作是 InitPPU，与装载原语同文件。 */
#include "ram.h"
#include "bank.h"
#include "jackal_prg_banks.h"

JACKAL_RAM JackalRam;

uint8_t BankRead8(uint16_t cpu) {
  if (cpu >= 0xC000u) {
    return JackalPrgBanks[7][cpu - 0xC000u];
  }
  return JackalPrgBanks[JackalRam.CurrentBank][cpu - 0x8000u];
}

const uint8_t *BankPtr(uint8_t bank, uint16_t cpu) {
  uint16_t off = (cpu >= 0xC000u) ? (uint16_t)(cpu - 0xC000u)
                                  : (uint16_t)(cpu - 0x8000u);
  return &JackalPrgBanks[bank][off];
}

void subChangeBank_YhasBank(uint8_t bank) {
  JackalRam.CurrentBank = bank; /* STY CurrentBank；UxROM 写 $8000,Y 无 UEFI 对应物 */
}

void subChangeBank_UseCurrentBank(void) {
  /* 窗口恒跟 CurrentBank，无副作用；保留调用点与汇编一致 */
}

void subChangeBank_YhasBank_BypassCurrentBank(uint8_t bank) {
  (void)bank; /* 不写 CurrentBank；跨 bank 读由 BankPtr 显式承担 */
}

/* RESET 语义（Bank7.ASM:47 起）：全零后 CurrentBank=0（初 bank0） */
void JackalRamInit(void) {
  uint8_t *p = JackalRam.Raw;
  uint16_t i;
  for (i = 0; i < 0x800u; i++) {
    p[i] = 0;
  }
  JackalRam.CurrentBank = 0;
}
