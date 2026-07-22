/* core/spawn.h：精灵生成/消亡机器（Bank7.ASM:6553-6831、7098-7147）。
   零页 $00-$12 是 subSpawnObjectFromParent 的传参区（ASM 调用方写零页后 JSR）；
   C 侧以 JackalSpawnZp 镜像该调用约定（各例程复用零页语义不同，不入 ram.h 命名字段——
   $00-$17 在 RAM 镜像中本就保留为 pad，调用点按 ASM 行号对照读写下标）。 */
#ifndef JACKAL_CORE_SPAWN_H
#define JACKAL_CORE_SPAWN_H

#include <stdint.h>

/* 零页 $00-$12 参数区镜像（含 $35 调用方 X 保存） */
extern uint8_t JackalSpawnZp[0x36];

/* fctGetNextOpenSpriteSlot（:6709）：倒序扫 SpriteObjectID==0，找到即
   subInitSpriteDataZERO（清 State/Data1-6/方向/Data8/WhichJeep/TypeIndex/
   GraphicsAttributes/SpriteAttributes($006A,X)/两亚像素，并无 RTS 落入
   subClearSpriteSpeed 清四速度——:6690 注释者疑问实为清除）。
   返回槽位 0-15；全满 $FF（DEX 下溢）。 */
uint8_t fctGetNextOpenSpriteSlot(void);
void subInitSpriteDataZERO(uint8_t x);
void subClearSpriteSpeed(uint8_t x);
/* :6695-6699：仅横向两字节（subClearSpriteSpeed 的横半部，ASM 公开标签） */
void subClearSpriteHorizSpeed(uint8_t x);
/* :6701-6705：仅纵向两字节（ASM 公开标签） */
void subClearSpriteVertSpeed(uint8_t x);

/* subDespawnLesserObjects_Offscreen_ForHighPriorityObjects（:6645）：四级优先——
   子弹$36/火焰$34/导弹$39 或屏外（State bit7）→ 任意低优先（ID bit7 清）→ 槽 0 兜底，
   恒成功；腾槽先 subDespawnSprite 清六字段再顺序落入 subInitSpriteDataZERO 全清。 */
uint8_t subDespawnLesserObjects_Offscreen_ForHighPriorityObjects(void);

/* subDespawnSprite（:7120）：清 ObjectID/State/TypeIndex/HitboxShapeIndex/HealthHP/
   GraphicsAttributes 六字段。 */
void subDespawnSprite(uint8_t x);

/* 状态迁移（:7130-7147）：Move=ADC 直接加（不保 bit7）；Set=保 bit7；
   ObjectID==0 时级联 JMP subDespawnSprite。 */
void subMoveSpriteToNextState(uint8_t x);
void subMoveSpriteToPreviousState(uint8_t x);
void subSetSpriteState(uint8_t x, uint8_t a);

/* Label244 尾段（:7098-7115）屏外消亡：横向 UB 负/≥2 消亡；纵向 UB==0 在屏；
   UB 负（below）VertPos≥$40 消亡；UB 正（above）VertPos<$C0 消亡。
   完整 Label244（位置更新+碰撞查询）待 Phase 4 敌人 AI 接入。 */
void subCheckSpriteDespawnIfOffscreen(uint8_t x);

/* subSpawnObjectFromParent（:6718-6805）：$00-$03 位置偏移入、$04-$07 位置结果、
   $08 ObjectID、$09-$12→Data4-6/方向/Data8/WhichJeep、$0F 拆 attr（低=Graphics/高=
   SpriteAttributes）、$10-$12→Data1-3；三表查 hitbox/HP/EnemyPoints（双吉普存活 +1）；
   返回槽位（Y 语义），无槽 $FF。父槽位由 $35 语义恢复（C 侧调用方自保存）。 */
uint8_t subSpawnObjectFromParent(void);

/* Label1257（:6567-6625）：块生成——更新 LastSpawnedEnemyY_*、读 X/对象 ID、
   X bit7=底部 $EF（否则顶部 0）、AbsHorizLB=X*4（两次 ASL 进位入 UB）、
   奇 SubPosition 且难度<3 抹 ID、三表查值、双吉普存活 EnemyPoints+1。
   ASM 末尾 JMP Label978 回环——C 侧由 scroll.c 调用点 continue 表达。 */
void Label1257(uint8_t x, const uint8_t *blk, uint8_t cursor);

#endif
