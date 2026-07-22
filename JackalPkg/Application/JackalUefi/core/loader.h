/* JACKAL_LOADER：pattern/nametable 装载与 PPU 初始化原语。
   subLoadNewPatternTable（Bank7.ASM:308-395）：场景表→load 表→RLE 段写 pattern 区；
   Label925（:2381-2452）：nametable RLE 流写 $2000 区，JMP InitPPU 收尾；
   InitPPU（:191-200）/ subSetPPUToBlackScreen（:240-249）/ Label844（:1387-1409）。 */
#ifndef JACKAL_CORE_LOADER_H
#define JACKAL_CORE_LOADER_H

#include <stdint.h>

/* 场景索引（tblLevel_ScenePPUPatternTableHeaderAddress，:397-407）：0-5 六关、
   6=标题、7=POW/GameOver、8=YEAH、9=EndOfGame、10(0x0A)=HERE */
void subLoadNewPatternTable(uint8_t sceneIndex);

/* X 为 tblSceneNametableData 的字节索引（0,2,...,18；:2454-2465 十项 dw） */
void Label925(uint8_t xIndex);
void Label159(void);   /* :2381 X=0 入口，等价 Label925(0) */

/* NormalPPUCTRL←$A8（NormalPPUCTRLValue，Global.ASM:4）、ScreenTransitionTimer←5 */
void InitPPU(void);

/* :240-249 硬件写归约为无副作用（UEFI 无 PPU 总线）；ASM 返回 A=0，
   调用点据此连写变量的语义已在各调用点显式展开 */
void subSetPPUToBlackScreen(void);

#endif
