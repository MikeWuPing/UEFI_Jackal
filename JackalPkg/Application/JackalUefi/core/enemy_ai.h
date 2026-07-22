/* core/enemy_ai.h：敌人 AI 底座（Bank7:7005-7733、Bank6:3968-4066 共享例程群 +
   subProcessObjectLogic 分派入口 Bank7:7744）。
   零页约定：$10-$15=24 位坐标三元组（subpixel/LB/UB，X 横 $10-$12、Y 纵 $13-$15）；
   $D7=碰撞类型结果（fctGetCollision 系列写）；$06/$07/$08/$09=方向/速度计算 scratch。 */
#ifndef JACKAL_CORE_ENEMY_AI_H
#define JACKAL_CORE_ENEMY_AI_H

#include <stdint.h>

/* Label244（:7073-7115）：速度积分 + 滚动同步 + 屏外检查（含贴边消亡 :7090-7096） */
void Label244(uint8_t x);
/* :7020-7066：24 位速度积分（heli $BE/$BD 与 F0 滚动豁免） */
void subProcessObjectSpeed_UpdatePosition(uint8_t x);
/* :1639-1687：滚动同步（SubPosition 差纵向 ±、屏外 State bit7 标记/恢复、HorizPos 合成） */
void subUpdateSpritePositionForScrolling(uint8_t x);
/* :7068：积分+同步+屏外消亡（不含碰撞查询——Label244 的轻量版） */
void subUpdateSpritePositionForScrolling_Speed_CheckForDespawn(uint8_t x);

/* :7156：出租车距离（|$dx|+|dy|，>$FF 饱和 $FF） */
uint8_t fctGetDistanceBetweenEnemyAndJeep(uint8_t x);
/* 目标吉普（:7199-7241） */
void subCountDownForJeepTargetBy1(uint8_t x);
void subCountDownForJeepTarget(uint8_t x);
void subCheckWhichJeepToAttack(uint8_t x);
/* 目标吉普死亡换目标（:7229）：返回 1=已翻转（SEC）、0=目标存活（CLC） */
uint8_t subCheckEnemyTarget_AttackOtherJeepIfDead(uint8_t x);

/* :7249：敌→目标吉普 32 级方向（$00-$1F 圆，$00=Right/$08=Down/$10=Left/$18=Up） */
uint8_t subCalculateDirectionTowardJeep(uint8_t x);
/* :7269：$10-$15 预设坐标版（前眺/虚拟目标用） */
uint8_t subCalculateDirectionWithPresets(uint8_t x);
/* :7425：dir（A 入）+ mult（$08：UB 整数倍、LB 1/2^n 微调）→ 四速度字节 */
void subCalculateObjectSpeed(uint8_t x, uint8_t dir, uint8_t mult);
/* :7367-7410 取反 */
uint8_t fctInvertA(uint8_t a);
void subInvertSpriteHorizSpeed(uint8_t x);
void subInvertSpriteVertSpeed(uint8_t x);
void subInvertSpriteVertAndHorizSpeed(uint8_t x);
/* :7008：方向分组 Y=((dir+1)&$1F)/4（0=Right 起逆时针 8 组） */
uint8_t fctGetSpriteOrientationIndex(uint8_t dir);
/* :7530：dir（A 入）→ tblSpriteMirroringAndIndexForDirection（attr 高半、TypeIndex+低半） */
void subUpdateSpriteForDirectionChange(uint8_t x, uint8_t dir);
/* :7567：dir（A 入）→ tblWalkingAnimationAttributes（attr 高半、低半 1=++/F=--） */
void subProcessWalkingAnimation(uint8_t x, uint8_t dir);

/* Bank6 :3968-4066 前眺碰撞（返回碰撞类型（A）且写 Raw[0xD7]） */
uint8_t fctGetCollision_WithSpeed_NearLookAhead_BG(uint8_t x);
uint8_t fctGetCollision_WithSpeed_FarLookAhead_BG(uint8_t x);
uint8_t fctGetCollisionType_SwampInfantry(uint8_t x);

/* 公共死亡/爆炸（:7602-7672、:7718-7733） */
void subInfantryDeath(uint8_t x);
void subInfantryDeathAnimation(uint8_t x);
void subCheckForBossDeath_MultipleBossEnemies(uint8_t x);
void subSpriteDeath(uint8_t x);
void subSpriteExplosion(uint8_t x);

/* subProcessObjectLogic（:7744）：ID&$7F==0 RTS；$36 敌弹特判；其余查 dw 分派表 */
void subProcessObjectLogic(uint8_t x);

#endif
