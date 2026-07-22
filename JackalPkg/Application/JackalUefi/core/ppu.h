/* JACKAL_PPU：CHR RAM 镜像与 $0770 PPU 更新队列机器。
   JackalVram 布局镜像 NES VRAM 地址空间：pattern $0000-$1FFF、nametable $2000-$2FFF、
   palette $3F00-$3F1F（$3F20-$3FFF 镜像区不做别名，写入按 14 位地址回绕）。
   PPUADDR/PPUDATA/PPUCTRL 寄存器副作用在镜像模型中的归约见 ppu.c 头注释。 */
#ifndef JACKAL_CORE_PPU_H
#define JACKAL_CORE_PPU_H

#include <stdint.h>

extern uint8_t JackalVram[0x4000];

/* Bank7.ASM:3219 subInGamePPUUpdates：解析 $0770 队列写入 JackalVram，结束后
   清队列 type 与 PPUGraphicsUpdateByteLength */
void subInGamePPUUpdates(void);

/* Bank7.ASM:2556 Label152：把 tblPPUGraphics_PalleteUpdateAddress[index & $7F] 指向的
   流附加到队列（先写 type=1，流尾 $FE→队列补 $FF、$FF→不补）；
   index bit7=1 为隐藏文本变体：前 2 个流字节（PPU 地址）保留，其后数据字节覆 0 */
void Label152(uint8_t index);

/* Bank7.ASM:2591 Label757：队尾写 0（type 终止符）并把长度计+1 */
void Label757(void);

/* Bank7.ASM:2493 subInsertPPUUpdateTerminator：队列[*x] 写 $FF、[*x+1] 写 0，
   PPUGraphicsUpdateByteLength = *x+1（x 为调用方持有的队列游标，对应 X 寄存器） */
void subInsertPPUUpdateTerminator(uint8_t *x);

#endif
