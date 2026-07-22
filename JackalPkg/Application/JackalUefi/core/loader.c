/* JACKAL_LOADER 实现。
   数据访问全部经 BankPtr 直读 PRG 镜像，故 ASM 中的 bank 切换（:329-334 装载循环内
   STX CurrentBank、$FF 处 LDA $0A 还原）在镜像模型中归约为无副作用——CurrentBank
   净效果不变，保留注释以备逐行对照。
   声音心跳：subLoadNewPatternTable 仅在字面（literal）路径按 $09/$08 计数调
   subProcessSound_Music（:368-378，计数回绕语义照 6502 DEC/BNE 先减后判）；
   Label925 仅在 $7F 子段边界调用（:2441-2445）。声音为空操作 stub（无 APU 依赖）。 */
#include "ram.h"
#include "bank.h"
#include "ppu.h"
#include "loader.h"
#include "sound_stub.h"

#define NORMAL_PPU_CTRL_VALUE 0xA8u  /* NormalPPUCTRLValue（Global.ASM:4） */

/* tblLevel_ScenePPUPatternTableHeaderAddress（:397-407，Bank7 $C473） */
#define SCENE_TABLE_CPU 0xC473u

/* tblSceneNametableData（:2454-2465）十项的 bank/cpu（Task 2.2 登记的地址） */
typedef struct {
  uint8_t bank;
  uint16_t cpu;
} SCENE_NAMETABLE_REF;

static const SCENE_NAMETABLE_REF tblSceneNametableData[10] = {
  {7, 0xD173},  /* 0  tblBlackScreenNametable */
  {4, 0xB3DD},  /* 1  tblMainTitleScreenNametable */
  {4, 0xB53D},  /* 2  tblMainTitleScreenStoryTextNametable */
  {4, 0xB661},  /* 3  tblPOWPortraitScreen1Nametable */
  {4, 0xB74D},  /* 4  tblPOWPortraitScreen2Nametable */
  {5, 0xACD3},  /* 5  tblGameStarted_BloodBoilScreen */
  {5, 0xAE13},  /* 6  tblLevelComplete_SegueScreen */
  {4, 0xB83E},  /* 7  tblLevelComplete_YEAHScreenNametable */
  {4, 0xB9B4},  /* 8  tblEndOfGameScreen_SunsetNametable */
  {5, 0xA88A},  /* 9  tblLevelComplete_HEREScreenNametable */
};

void InitPPU(void) {
  JackalRam.ScreenTransitionTimer = 5;   /* :193 JSR + 的冗余首写 */
  JackalRam.NormalPPUCTRL = NORMAL_PPU_CTRL_VALUE;  /* PPUCTRL 写无镜像副作用 */
  JackalRam.ScreenTransitionTimer = 5;   /* 落入 + 块再写（ASM 注释自嘲 redundant） */
}

void subSetPPUToBlackScreen(void) {
  /* PPUCTRL←NormalPPUCTRL&$7F（关 NMI）、PPUADDR←0、PPUMASK←0（关渲染）：
     UEFI 无 PPU 总线，寄存器写无镜像副作用；黑屏视觉由 nametable 清零达成。
     ASM 返回 A=0，调用点据此连写多个变量——C 翻译在各调用点显式展开。 */
}

/* Label844（:1387-1409）：清 $23-$DF 与 $0300-$06FF，JMP InitPPU（ram.h 声明） */
void Label844(void) {
  uint16_t i;
  subSetPPUToBlackScreen();
  for (i = 0x23; i < 0xE0u; i++) {
    JackalRam.Raw[i] = 0;
  }
  for (i = 0x300u; i < 0x700u; i++) {
    JackalRam.Raw[i] = 0;
  }
  InitPPU();
}

void subLoadNewPatternTable(uint8_t sceneIndex) {
  uint8_t zp09 = 0x20;   /* $09 声音心跳低计数 */
  uint8_t zp08 = 0x03;   /* $08 声音心跳高计数 */
  const uint8_t *sceneTbl = BankPtr(7, SCENE_TABLE_CPU);
  uint16_t loadCpu = (uint16_t)(sceneTbl[sceneIndex * 2u] |
                                ((uint16_t)sceneTbl[sceneIndex * 2u + 1u] << 8));
  const uint8_t *lt = BankPtr(7, loadCpu);
  uint16_t p = 0;
  /* :310-316 等待 PPU + subSetPPUToBlackScreen：无副作用；A=0 语义显式展开 */
  subSetPPUToBlackScreen();
  JackalRam.PPUGraphicsUpdateTableIndex = 0;
  JackalRam.PPUGraphicsUpdateComplete = 0;
  for (;;) {
    uint8_t bank = lt[p];
    const uint8_t *seg;
    uint16_t addr, s;
    if (bank == 0xFF) {
      break;   /* :327-335 还原 CurrentBank（镜像模型净效果不变）后 JMP InitPPU */
    }
    seg = BankPtr(bank, (uint16_t)(lt[p + 1u] | ((uint16_t)lt[p + 2u] << 8)));
    p += 3;
    addr = (uint16_t)(((uint16_t)seg[0] << 8) | seg[1]);  /* 段 PPU 地址大端（Bank3.ASM:9） */
    s = 2;
    for (;;) {
      uint8_t b = seg[s];
      s++;
      if (b == 0xFF) {
        break;
      }
      if (b & 0x80u) {   /* 字面 (b&$7F) 字节（0=256），声音心跳仅此路径 */
        uint16_t count = (uint16_t)(b & 0x7Fu);
        if (count == 0) {
          count = 256;
        }
        while (count-- != 0) {
          JackalVram[addr & 0x3FFFu] = seg[s];
          s++;
          addr++;
          zp09--;
          if (zp09 == 0) {
            zp08--;
            if (zp08 == 0) {
              zp08 = 3;
              zp09 = 0x20;
              subProcessSound_Music();   /* 声音 stub：保留调用点、无 APU 依赖 */
            }
          }
        }
      } else {   /* 重复下一字节 b 次（0=256） */
        uint16_t count = (b != 0) ? b : 256u;
        uint8_t v = seg[s];
        s++;
        while (count-- != 0) {
          JackalVram[addr & 0x3FFFu] = v;
          addr++;
        }
      }
    }
  }
  InitPPU();
}

void Label925(uint8_t xIndex) {
  const SCENE_NAMETABLE_REF *ref = &tblSceneNametableData[xIndex >> 1];
  const uint8_t *stream = BankPtr(ref->bank, ref->cpu);
  uint16_t pos = 0;
  /* :2389-2399 等待 PPU + subSetPPUToBlackScreen：无副作用；A=0 语义显式展开 */
  subSetPPUToBlackScreen();
  JackalRam.PPUGraphicsUpdateByteLength = 0;
  JackalRam.ScreenVerticalScrollPosition_PPU = 0;
  JackalRam.ScreenHorizontalScrollPosition_PPU = 0;
  JackalRam.Level6BossTankScroll_Next = 0;
  JackalRam.Level6BossTankScroll_Current = 0;
  for (;;) {
    /* Label935：段地址小端 dw（byte[0]=lo 后写、byte[1]=hi 先写，:2403-2408） */
    uint16_t addr = (uint16_t)(stream[pos] | ((uint16_t)stream[pos + 1u] << 8));
    pos += 2;
    for (;;) {
      uint8_t b = stream[pos];
      pos++;
      if (b == 0xFF) {
        InitPPU();   /* :2452 JMP InitPPU */
        return;
      }
      if (b == 0x7F) {   /* 子段结束：声音 stub 调用点后读下一段 */
        subProcessSound_Music();
        break;
      }
      if (b & 0x80u) {   /* 字面 (b&$7F) 字节（0=256） */
        uint16_t count = (uint16_t)(b & 0x7Fu);
        if (count == 0) {
          count = 256;
        }
        while (count-- != 0) {
          JackalVram[addr & 0x3FFFu] = stream[pos];
          pos++;
          addr++;
        }
      } else {   /* 重复下一字节 b 次（0=256） */
        uint16_t count = (b != 0) ? b : 256u;
        uint8_t v = stream[pos];
        pos++;
        while (count-- != 0) {
          JackalVram[addr & 0x3FFFu] = v;
          addr++;
        }
      }
    }
  }
}

void Label159(void) {
  Label925(0);
}
