/* subProcessControllerInputs（Bank7.ASM:1248-1310）。
   双读比对防抖 + 单人合并双手柄 + 边沿检测 (cur^held)&cur。
   UEFI 无手柄移位寄存器，platform 每帧采样一次；双读语义经 JackalInputReadHook
   保留——默认两次读都返回采样值（一致），host 测试可注入抖动（两次读不同 →
   该帧 JeepControlsInput1Frame[0] 清零、held 不更新，照 :1262-1264 失败路径）。 */
#ifndef JACKAL_CORE_INPUT_CORE_H
#define JACKAL_CORE_INPUT_CORE_H

#include <stdint.h>

/* padIndex：0=P1/1=P2；readIndex：0=第一次读/1=第二次读；sampled：本帧采样值。
   返回该次读取的生效值。NULL（默认）= 两次读都取 sampled。 */
typedef uint8_t (*JackalPadReadHook)(uint8_t padIndex, uint8_t readIndex, uint8_t sampled);
extern JackalPadReadHook JackalInputReadHook;

void subProcessControllerInputs(uint8_t pad1, uint8_t pad2);

#endif
