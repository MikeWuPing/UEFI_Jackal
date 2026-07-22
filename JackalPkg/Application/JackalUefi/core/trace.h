/* TRACE：GCS/GPM 状态迁移的追踪输出。platform 注册串口钩子（Task 2.7 接线），
   host 测试注册内存钩子；hook 为 NULL 时不输出。
   定义落在 game_control.c（与状态机同文件，避免单开一个 .c）。 */
#ifndef JACKAL_CORE_TRACE_H
#define JACKAL_CORE_TRACE_H

extern void (*JackalTraceHook)(const char *msg);

/* GCS/GPM 任一变化时经 hook 输出 "STATE GCS=x GPM=y"（仅变化时输出，防 60fps 洪泛）。
   JackalNmiFrame 在每帧 subProcessGameControl 之后调用。 */
void JackalTraceState(void);

#endif
