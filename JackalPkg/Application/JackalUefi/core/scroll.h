/* JACKAL_SCROLL：滚屏寄存器、列装载（Label975/888/893/911）、生成表游走（Label978）、
   暂停（Label984）。Bank7.ASM 逐行翻译，地址/行为证据见 scroll.c 头注。 */
#ifndef JACKAL_CORE_SCROLL_H
#define JACKAL_CORE_SCROLL_H

/* Bank7.ASM:204 subProcessScreenScrolling：滚屏寄存器写 PPU。
   镜像模型中唯一有状态的效果是 Level6BossTankScroll_Current 跟进 Next。 */
void subProcessScreenScrolling(void);

/* Bank7.ASM:1764 Label975：滚屏位置同步 + 上/下行触发列装载（内部含 Label888/893/911） */
void Label975(void);

/* Bank7.ASM:6427 Label978：关卡生成表游走骨架。EF/F0/F1/F2 分支全译；
   对象生成分支经 stub（fctGetNextOpenSpriteSlot/subDespawnLesserObjects 恒返回无空槽，
   subSpawnObjectFromBlock 仅更新 LastSpawnedEnemyY_* 并 TRACE，Phase 4 实装） */
void Label978(void);

/* Bank7.ASM:3791 Label984：Start 边沿暂停/恢复（标题模式或黑屏计时中不响应） */
void Label984(void);

#endif
