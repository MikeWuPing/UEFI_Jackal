/* 声音 stub：保留调用点、无 APU 依赖（项目硬约束：最终 app 不需要声音，
   音效调用点只保留等价的时间/状态行为）。后续任务按需补充更多 stub 入口。 */
#ifndef JACKAL_CORE_SOUND_STUB_H
#define JACKAL_CORE_SOUND_STUB_H

#include <stdint.h>

/* Bank7.ASM 音乐/音效处理例程：装载期心跳与帧尾调用点 */
void subProcessSound_Music(void);

/* 音效/音乐触发调用点：空操作。clipIndex 照 ASM 实参原样传入以便对照，
   当前不产生任何状态行为（标题/剧情/爆炸/关卡 BGM 各索引值见调用点注释）。 */
void subInitiateSoundClip(uint8_t clipIndex);
void subStopMusic(void);

#endif
