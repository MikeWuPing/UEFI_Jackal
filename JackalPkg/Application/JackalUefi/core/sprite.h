/* core/sprite.h：Bank1 精灵渲染（subProcessSpriteUpdates，Bank1.ASM:8-205）。
   NMI 中经 subChangeBank(1) 调用（Bank7.ASM:155 区域）；UEFI 侧直接调用，
   表/元精灵数据经 BankPtr(1, cpu) 读 Bank1 PRG 镜像。 */
#ifndef JACKAL_CORE_SPRITE_H
#define JACKAL_CORE_SPRITE_H

/* Bank1.ASM:8 subProcessSpriteUpdates：64 OAM 计数、槽位 $1F→0 倒序、
   SpriteSlotRotation+=$44 起始、每 tile +$C4 旋转、GCS5&&GPM3 分数显示、余槽 $F4 */
void subProcessSpriteUpdates(void);

#endif
