/* core/render：JackalVram（CHR RAM + nametable + palette）→ 256x240 BGRX framebuffer。
   纯 C99，无平台依赖；platform 层取帧指针送 GOP。 */
#ifndef JACKAL_CORE_RENDER_H
#define JACKAL_CORE_RENDER_H

#include <stdint.h>

/* 按当前 JackalVram/JackalRam（滚动、NormalPPUCTRL、ScreenTransitionTimer）合成一帧。
   ScreenTransitionTimer≠0 → 全帧 NES 黑（$0F），对应 NMI 的 PPUMASK=0 语义（Task 2.6）。 */
void JackalRenderFrame(void);

/* 上一帧合成结果：256*240 个 uint32（BGRX：(R<<16)|(G<<8)|B）。 */
const uint32_t *JackalRenderGetFb(void);

#endif
