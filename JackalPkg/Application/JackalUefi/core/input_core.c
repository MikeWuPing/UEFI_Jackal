/* subProcessControllerInputs（Bank7.ASM:1248-1310 逐行翻译）。
   $04/$05 = 第二次读的 P1/P2，$08/$09 = 第一次读的保存——C 侧用局部数组对应。
   失败路径（:1262）：X 此刻恒为 0（:1249 LDX #$00 后子例程以 X=0 返回），
   故只有 JeepControlsInput1Frame[0] 被清零——原厂 quirk 原样保留。
   单人模式（:1304-1310）：($FF & pad1) | pad2 合并进 P1；中间的 LDA #$30 是
   死代码（BNE 必转），保留注释不翻译。 */
#include "ram.h"
#include "input_core.h"

JackalPadReadHook JackalInputReadHook = 0;

static uint8_t readPad(uint8_t padIndex, uint8_t readIndex, uint8_t sampled) {
  if (JackalInputReadHook != 0) {
    return JackalInputReadHook(padIndex, readIndex, sampled);
  }
  return sampled;
}

/* ControllerReadOK 内层（:1297-1303）：边沿 = (cur^held)&cur，held 仅成功路径更新 */
static void processOnePad(uint8_t x, uint8_t cur) {
  uint8_t edge = (uint8_t)((cur ^ JackalRam.JeepControlsInput[x]) & cur);
  JackalRam.JeepControlsInput1Frame[x] = edge;
  JackalRam.JeepControlsInput[x] = cur;
}

void subProcessControllerInputs(uint8_t pad1, uint8_t pad2) {
  uint8_t first1 = readPad(0, 0, pad1);   /* $08 */
  uint8_t first2 = readPad(1, 0, pad2);   /* $09 */
  uint8_t cur1   = readPad(0, 1, pad1);   /* $04 */
  uint8_t cur2   = readPad(1, 1, pad2);   /* $05 */
  if (first1 != cur1 || first2 != cur2) {
    JackalRam.JeepControlsInput1Frame[0] = 0;   /* X=0 quirk（:1262-1264） */
    return;
  }
  if ((JackalRam.PlayerMode_1or2 & 0x04u) != 0) {   /* 双人：逐 pad 边沿 */
    processOnePad(0, cur1);
    processOnePad(1, cur2);
    return;
  }
  /* 单人：合并双手柄到 P1（:1304-1310），只处理 X=0 */
  processOnePad(0, (uint8_t)((0xFFu & cur1) | cur2));
}
