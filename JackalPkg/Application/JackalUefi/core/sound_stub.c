/* 声音 stub 实现：空操作。调用点保留以维持与反汇编逐行对照；
   若未来要核对调用时序，可在此加帧计数/trace 钩子，仍不引入音频输出。 */
#include "sound_stub.h"

void subProcessSound_Music(void) {
}

void subInitiateSoundClip(uint8_t clipIndex) {
  (void)clipIndex;
}

void subStopMusic(void) {
}
