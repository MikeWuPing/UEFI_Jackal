/* JackalRenderFrame：NES 渲染管线（BG + 精灵叠加，Task 3.4 实装精灵段）。
   语义与 tools/rom_extract/render_png.py:render_nametable 一致（Task 1.8 结论），
   外加硬件两条规则：
   - BG 组内 col0（透明）→ $3F00 全局背景色（NesDev 惯例，golden 同源实现）；
   - 垂直镜像：$2800/$2C00 物理镜像 $2000/$2400 → 垂直 nametable 位直接丢弃，
     纵向滚动按 $FC 的 $EF 回绕语义对 240 取模（滚入区由 Label975 列装载维护，Task 2.8）。
   横向：视口 X = PPUCTRL.bit0*256 + ScreenHorizontalScrollPosition_PPU + x，跨 256 进位
   切到相邻水平 nametable（story 右滚即读 $2400）。BG pattern 页 = PPUCTRL bit4
   （NormalPPUCTRL=$A8 bit4=0 → $0000 页）。
   精灵段（NesDev OAM 语义，Global.ASM:4 NormalPPUCTRLValue=$A8 → 8x16 精灵）：
   - 8x16：tile index bit0 选 pattern 页（0=$0000/1=$1000），&$FE=上 8 行块、|$01=下块；
     attr bit7 V 翻转=上下块交换且行内翻转、bit6 H 翻转、bit5=1 且 BG col≠0 → BG 前置、
     bit1-0 调色板组（$3F10+组*4+col）；PPUCTRL bit5=0 时退 8x8（pattern 页=bit3，本仓库
     未见该模式，语义照翻保留分支）。
   - OAM Y 字节+1 为显示首行；col=0 透明不写；OAM 顺序先者赢（同像素后序 sprite 不覆盖，
     即使先者因 BG 前置未显示——spriteWritten 先行标记）；Y≥$EF 行自然越底不裁。
   - NormalPPUMASK bit4=0 → 整段不画。 */
#include "ram.h"
#include "ppu.h"
#include "nes_palette.h"
#include "render.h"

#define FRAME_W 256u
#define FRAME_H 240u

static uint32_t sFb[FRAME_W * FRAME_H];
static uint8_t sBgCol[FRAME_W * FRAME_H];   /* BG 组内色索引（优先级判定的"BG 非 0"真值） */

const uint32_t *JackalRenderGetFb(void) {
  return sFb;
}

/* 单 OAM 项叠加（8x16/8x8 两模式；spriteWritten 先序占用语义见头注） */
static void renderSprite(uint8_t i, uint8_t *spriteWritten) {
  uint8_t y0 = JackalRam.OamShadow[i * 4u];
  uint8_t tile = JackalRam.OamShadow[i * 4u + 1u];
  uint8_t attr = JackalRam.OamShadow[i * 4u + 2u];
  uint8_t x0 = JackalRam.OamShadow[i * 4u + 3u];
  uint8_t h16 = (JackalRam.NormalPPUCTRL & 0x20u) != 0;  /* bit5：1=8x16 */
  uint8_t rows = h16 ? 16u : 8u;
  uint8_t row;
  for (row = 0; row < rows; row++) {
    uint16_t sy = (uint16_t)y0 + 1u + row;               /* OAM Y+1 首行；uint16 防 $FF 回绕 */
    uint8_t r, t, line;
    uint16_t patBase;
    uint8_t lo, hi, cx;
    if (sy >= FRAME_H) {
      continue;
    }
    r = (attr & 0x80u) != 0 ? (uint8_t)(rows - 1u - row) : row;   /* V 翻转 */
    if (h16) {
      t = (r < 8u) ? (uint8_t)(tile & 0xFEu) : (uint8_t)(tile | 0x01u);
      line = (uint8_t)(r & 7u);
      patBase = (tile & 1u) ? 0x1000u : 0x0000u;
    } else {
      t = tile;
      line = r;
      patBase = (JackalRam.NormalPPUCTRL & 0x08u) ? 0x1000u : 0x0000u;  /* 8x8：页=bit3 */
    }
    lo = JackalVram[patBase + (uint16_t)t * 16u + line];
    hi = JackalVram[patBase + (uint16_t)t * 16u + line + 8u];
    for (cx = 0; cx < 8u; cx++) {
      uint16_t sx = (uint16_t)x0 + cx;
      uint8_t bit, col;
      uint16_t idx;
      if (sx >= FRAME_W) {
        continue;
      }
      bit = (attr & 0x40u) != 0 ? cx : (uint8_t)(7u - cx);        /* H 翻转 */
      col = (uint8_t)(((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1));
      if (col == 0) {
        continue;                          /* 透明 */
      }
      idx = (uint16_t)(sy * FRAME_W + sx);
      if (spriteWritten[idx] != 0) {
        continue;                          /* 前序 sprite 已占用（先者赢） */
      }
      spriteWritten[idx] = 1;
      if (sBgCol[idx] != 0 && (attr & 0x20u) != 0) {
        continue;                          /* BG 前置（占用已记，后序 sprite 亦不可见） */
      }
      sFb[idx] = JackalNesPaletteBGRX[
          JackalVram[(uint16_t)(0x3F10u + (uint16_t)(attr & 3u) * 4u + col)] & 0x3Fu];
    }
  }
}

void JackalRenderFrame(void) {
  uint32_t x, y;
  uint8_t ctrl = JackalRam.NormalPPUCTRL;
  uint16_t patBase = (ctrl & 0x10u) ? 0x1000u : 0x0000u;
  if (JackalRam.ScreenTransitionTimer != 0) {  /* PPUMASK=0 黑屏（NMI 语义） */
    uint32_t black = JackalNesPaletteBGRX[0x0F];
    for (y = 0; y < FRAME_H; y++) {
      for (x = 0; x < FRAME_W; x++) {
        sFb[y * FRAME_W + x] = black;
      }
    }
    return;
  }
  for (y = 0; y < FRAME_H; y++) {
    uint16_t sy = (uint16_t)(y + JackalRam.ScreenVerticalScrollPosition_PPU);
    uint16_t ntY;
    if (sy >= FRAME_H) {
      sy -= FRAME_H;   /* $EF 回绕；垂直镜像下 nametable 位翻转不可见，直接取模 */
    }
    ntY = (uint16_t)((sy >> 3) * 32u);
    for (x = 0; x < FRAME_W; x++) {
      uint16_t sx = (uint16_t)(x + JackalRam.ScreenHorizontalScrollPosition_PPU);
      uint8_t ntH = (uint8_t)((ctrl & 1u) ^ (uint8_t)(sx >> 8));
      uint8_t px = (uint8_t)(sx & 0xFFu);
      uint16_t base = (uint16_t)(0x2000u + (uint16_t)ntH * 0x400u);
      uint8_t tile = JackalVram[base + ntY + (px >> 3)];
      uint8_t attr = JackalVram[base + 0x3C0u + (uint16_t)(sy >> 5) * 8u + (px >> 5)];
      uint8_t group = (uint8_t)((attr >> (((uint8_t)((px >> 4) & 1u) +
                                           (uint8_t)((sy >> 4) & 1u) * 2u) * 2u)) & 3u);
      uint16_t tofs = (uint16_t)(patBase + (uint16_t)tile * 16u + (sy & 7u));
      uint8_t lo = JackalVram[tofs];
      uint8_t hi = JackalVram[tofs + 8u];
      uint8_t bit = (uint8_t)(7u - (px & 7u));
      uint8_t col = (uint8_t)(((lo >> bit) & 1u) | (((hi >> bit) & 1u) << 1));
      uint8_t pi = (col == 0) ? JackalVram[0x3F00u]
                              : JackalVram[(uint16_t)(0x3F00u + (uint16_t)group * 4u + col)];
      sBgCol[y * FRAME_W + x] = col;
      sFb[y * FRAME_W + x] = JackalNesPaletteBGRX[pi & 0x3Fu];
    }
  }
  if ((JackalRam.NormalPPUMASK & 0x10u) != 0) {   /* bit4：显示精灵 */
    static uint8_t spriteWritten[FRAME_W * FRAME_H];
    uint8_t i;
    uint32_t n;
    for (n = 0; n < FRAME_W * FRAME_H; n++) {
      spriteWritten[n] = 0;
    }
    for (i = 0; i < 64u; i++) {
      renderSprite(i, spriteWritten);
    }
  }
}
