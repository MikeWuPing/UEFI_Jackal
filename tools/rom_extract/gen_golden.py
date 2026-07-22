"""gen_golden.py：core C 翻译的 golden oracle。用 Phase 1 Python 提取侧从 ref/orgrom.nes
独立计算，产出 tools/core_test/golden/ppu.json；C 测试把值抄为静态常量对照。
三项内容：
1. level1_default_palette：Level1DefaultPalette 流的 32 数据字节（去 2 地址字节与 $FF 尾）；
2. scene6_pattern_crc32：按 subLoadNewPatternTable（Bank7.ASM:308-395）语义把场景 6（标题）
   的 pattern 段装载进 $0000-$1FFF 后的 crc32（装载实现复用 pattern_loader.load_scene，
   段 PPU 地址为大端双字节，与 nametable 流小端 dw 不同——证据 Bank3.ASM:9 `db $02,$B0`）；
3. title_nametable_crc32：Label925 索引 2（tblMainTitleScreenNametable，Bank4）解码写入
   $2000-$2FFF 后的 crc32（未写区域为 0，与 Label844/清零后初态一致）；
4. title_frame_bgrx_crc32：标题帧 256x240 BGRX（uint32 LE = R<<16|G<<8|B，内存序 [B,G,R,0]）
   的 crc32。渲染算法与 core/render.c:JackalRenderFrame 逐行同构（垂直镜像丢弃纵向
   nametable 位、col0→$3F00、横向进位切水平 nametable、BG pattern 页=PPUCTRL bit4），
   输入状态 = JackalReset 后两帧的 VRAM：scene6 pattern@$0000、title@$2000、
   story@$2400、palette@$3F00，scroll=0、ctrl=$A8；
5. level1_first_screen_nt2000/nt2400_crc32：GPM1 初态 + 31 次 Label975 上行装载
   （Label888/893/911 全译）后的 $2000/$2400 两 nametable 区 crc32。数据一律按
   bank 窗口读（越表邻读命中后续 ROM，与 C 侧 BankPtr 同语义）；队列消费与
   core/ppu.c:subInGamePPUUpdates 同语义。装载几何（Task 2.8 查证）：垂直镜像
   = $2000/$2400 左右排列，装载条带 16 宽——左 8 大块进 $2000 行、右 8 进 $2400 行；
   ScreenLoadIndex 倒序（$4B=0 → 表尾页=关卡底部）。
用法：python tools/rom_extract/gen_golden.py"""
import json
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'rom_extract'))

from ines import load_rom
from tables import by_name, entry_bytes
from extractors import decode_nametable_rle
from pattern_loader import load_scene
from render_png import NES_PALETTE

ROM_PATH = ROOT / 'ref' / 'orgrom.nes'
OUT_PATH = ROOT / 'tools' / 'core_test' / 'golden' / 'ppu.json'


def render_frame_bgrx_crc(vram, ctrl=0xA8, hscroll=0, vscroll=0):
    """core/render.c:JackalRenderFrame 的 Python 同构实现（改动必须双侧同步）。"""
    fb = bytearray(256 * 240 * 4)
    pat_base = 0x1000 if (ctrl & 0x10) else 0x0000
    for y in range(240):
        sy = y + vscroll
        if sy >= 240:
            sy -= 240
        nt_y = (sy >> 3) * 32
        for x in range(256):
            sx = x + hscroll
            nt_h = (ctrl & 1) ^ (sx >> 8)
            px = sx & 0xFF
            base = 0x2000 + nt_h * 0x400
            tile = vram[base + nt_y + (px >> 3)]
            attr = vram[base + 0x3C0 + (sy >> 5) * 8 + (px >> 5)]
            group = (attr >> ((((px >> 4) & 1) + ((sy >> 4) & 1) * 2) * 2)) & 3
            tofs = pat_base + tile * 16 + (sy & 7)
            lo, hi = vram[tofs], vram[tofs + 8]
            bit = 7 - (px & 7)
            col = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)
            pi = vram[0x3F00] if col == 0 else vram[0x3F00 + group * 4 + col]
            r, g, b = NES_PALETTE[pi & 0x3F]
            o = (y * 256 + x) * 4
            fb[o:o + 4] = bytes((b, g, r, 0))
    return f'0x{zlib.crc32(bytes(fb)) & 0xFFFFFFFF:08x}'


def level1_first_screen_nametables(rom):
    """GPM1 初态 + Label975 上行装载 31 次后的完整 4KB VRAM（与 core/scroll.c 同算法，
    改动必须双侧同步）。Label975/888/893/911 对应 Bank7.ASM:1764-2188。"""
    e_idx = by_name('tblLevel1LayoutScreenLoadIndex')
    e_lay = by_name('tblLevel1ScreenLayout')
    e_def = by_name('tblLevel1Layout32x32Definition')
    e_pal = by_name('tblLevel1Layout32x32DefinitionPalette')
    bank = rom.banks[e_lay.bank]

    def wread(cpu):
        return bank[cpu - 0x8000]

    def b7(cpu):
        return rom.banks[7][cpu - 0xC000]

    bg_off = [b7(0xCFEE + i) for i in range(6)]     # tblLevelBGTileOffset（.PAD 未登记）
    row1_2 = (0xF0, 0xF0, 0x0F, 0x0F)               # tblLoadLevelBGRow1_2（:2192）
    row3_4 = (0x0F, 0x0F, 0xF0, 0xF0)               # tblLoadLevelBGRow3_4（:2194）
    vram = bytearray(0x4000)
    queue = bytearray(0x100)
    qlen = 0

    def qput(b):
        nonlocal qlen
        queue[qlen] = b & 0xFF
        qlen += 1

    def consume():
        nonlocal qlen
        y = 0
        while True:
            t = queue[y]
            if t == 0:
                break
            inc32 = (t == 2)
            y += 1
            addr = (queue[y] << 8) | queue[y + 1]
            y += 2
            if t == 3:
                count = queue[y]
                y += 1
                while True:
                    vram[addr & 0x3FFF] = queue[y]
                    y += 1
                    addr = (addr + (32 if inc32 else 1)) & 0xFFFF
                    count = (count - 1) & 0xFF
                    if count == 0:
                        break
            while True:
                b = queue[y]
                y += 1
                if b == 0xFF:
                    break
                vram[addr & 0x3FFF] = b
                addr = (addr + (32 if inc32 else 1)) & 0xFFFF
        queue[0] = 0
        qlen = 0

    # Label159 = Label925(0)（:3439）：tblSceneNametableData[0] = tblBlackScreenNametable
    for addr, data in decode_nametable_rle(entry_bytes(rom, by_name('tblBlackScreenNametable'))):
        vram[addr:addr + len(data)] = data

    z = {0x47: 0x23, 0x48: 0xA0, 0x49: 0x23, 0x4A: 0xF8,   # GPM1（:3443-3458）
         0x4B: 0, 0x4C: 0, 0x4D: 2, 0x4F: 0, 0x05: 0}
    prev_sub, subpos, screen = 0, 0, 0

    def label893():
        z08, z09 = z[0x47], z[0x48]
        z0A = z[0x4B]
        z0B = (z[0x4C] << 4) & 0xFF
        z0C = z[0x4D]
        z0D, z0E = z[0x49], z[0x4A]
        zDB, zDC = z0B, z0C
        page_cpu = e_lay.cpu + wread(e_idx.cpu + z0A) * 128
        qput(1)
        qput(z08)
        qput(z09)
        z0F = 0x10
        while True:                                     # Label913
            tile_id = wread(page_cpu + z0B)
            if tile_id >= bg_off[0]:                    # Level1 Y=0 与公共表同基址
                tile_id -= bg_off[0]
            z_y = z0C * 4
            for _ in range(4):                          # Label910（碰撞写略：不影响 VRAM）
                qput(wread(e_def.cpu + tile_id * 16 + z_y))
                z_y += 1
            z[0x05] = (z[0x05] + 1) & 0xFF
            z0F -= 1
            if z0F == 0:
                break
            if z0F == 8:
                z[0x05] = (z[0x05] - 8) & 0xFF
                qput(0xFF)
                qput(1)
                qput((z08 + 4) & 0xFF)
                qput(z09)
            z0B = (z0B + 1) & 0xFF
        if zDC & 1:                                     # Label911：奇数 sub-row 写属性
            z11 = 0x10
            while True:
                qput(0xFF)
                qput(3)
                qput(z0D)
                qput(z0E)
                qput(8)
                for _ in range(8):
                    cur = wread(e_pal.cpu + wread(page_cpu + zDB))
                    n_cpu = page_cpu + (0x80 if zDC < 2 else 0)
                    nxt = wread(e_pal.cpu + wread(n_cpu + zDB))
                    qput((cur & row1_2[zDC]) | (nxt & row3_4[zDC]))
                    zDB = (zDB + 1) & 0xFF
                    z11 -= 1
                if z11 == 0:
                    break
                z0D = (z0D + 4) & 0xFF
        qput(0xFF)
        qput(0)

    for _ in range(31):                                 # GPM2 31 帧（:3471-3498）
        if subpos != prev_sub:
            prev_sub = subpos
            if (subpos & 7) == 0:                       # 上行（$4E=0）触发
                z[0x4F] = (z[0x4F] - 8) & 0xFF
                z[0x05] = z[0x4F]
                if subpos == 0 or ((subpos - 0x10) & 0x1F) == 0:
                    z[0x4A] = (z[0x4A] - 8) & 0xFF
                    if z[0x4A] < 0xC0:
                        z[0x4A] = 0xF8
                borr = z[0x48] < 0x20
                z[0x48] = (z[0x48] - 0x20) & 0xFF
                if borr:
                    z[0x47] = (z[0x47] - 1) & 0xFF
                    if z[0x47] == 0x1F:
                        z[0x47] = 0x23
                        z[0x48] = 0xA0
                z[0x4D] += 1
                if z[0x4D] == 4:
                    if z[0x4C] == 7:
                        z[0x4B] += 1
                        z[0x4D] = 2
                        z[0x4C] = 0
                    else:
                        z[0x4C] += 1
                        z[0x4D] = 0
                label893()
        consume()
        if screen == 0:
            subpos = (subpos + 8) & 0xFF
            if subpos == 0xF0:
                screen, subpos = 1, 0
    return vram


def main():
    rom = load_rom(ROM_PATH)
    pal_stream = entry_bytes(rom, by_name('Level1DefaultPalette'))
    palette = list(pal_stream[2:34])
    assert len(palette) == 32
    scene_tbl_cpu = by_name('tblLevel_ScenePPUPatternTableHeaderAddress').cpu
    scene6 = bytes(load_scene(rom, scene_tbl_cpu, 6)[:0x2000])
    nt = bytearray(0x1000)
    for addr, data in decode_nametable_rle(entry_bytes(rom, by_name('tblMainTitleScreenNametable'))):
        nt[addr - 0x2000:addr - 0x2000 + len(data)] = data
    # 标题帧 VRAM（= JackalReset 后两帧状态）
    vram = bytearray(0x4000)
    vram[:0x2000] = scene6
    for name in ('tblMainTitleScreenNametable', 'tblMainTitleScreenStoryTextNametable'):
        for addr, data in decode_nametable_rle(entry_bytes(rom, by_name(name))):
            vram[addr:addr + len(data)] = data
    title_pal = entry_bytes(rom, by_name('tblTitleScreenPalette'))
    vram[0x3F00:0x3F20] = title_pal[2:34]
    l1_vram = level1_first_screen_nametables(rom)
    golden = {
        'level1_default_palette': palette,
        'scene6_pattern_crc32': f'0x{zlib.crc32(scene6) & 0xFFFFFFFF:08x}',
        'title_nametable_crc32': f'0x{zlib.crc32(bytes(nt)) & 0xFFFFFFFF:08x}',
        'title_frame_bgrx_crc32': render_frame_bgrx_crc(vram),
        'level1_first_screen_nt2000_crc32': f'0x{zlib.crc32(bytes(l1_vram[0x2000:0x2400])) & 0xFFFFFFFF:08x}',
        'level1_first_screen_nt2400_crc32': f'0x{zlib.crc32(bytes(l1_vram[0x2400:0x2800])) & 0xFFFFFFFF:08x}',
    }
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUT_PATH.write_text(json.dumps(golden, indent=2) + '\n', encoding='ascii', newline='\n')
    print(json.dumps(golden, indent=2))


if __name__ == '__main__':
    main()
