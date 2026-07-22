"""镜像 Bank7 subLoadNewPatternTable（Bank7.ASM:308-395）的装载行为。
格式结论：load 表 = 若干 {db bank, dw segmentCpu} + db $FF；
segment = ppuAddrHi, ppuAddrLo, RLE 流；
RLE 控制字节 0xFF=结束；bit7=1=字面拷贝 (b&0x7F) 字节；bit7=0=重复下一字节 (b&0x7F) 次；
计数 0 按 256（6502 DEC/BNE 先减后判）。
"""
from ines import BANK_SIZE

CHR_RAM_SIZE = 0x4000  # pattern $0000-$1FFF + nametable $2000-$3FFF

def decode_rle(data, start):
    out = bytearray()
    i = start
    while True:
        b = data[i]; i += 1
        if b == 0xFF:
            return bytes(out), i
        count = b & 0x7F
        if count == 0:
            count = 256
        if b & 0x80:
            out += data[i:i+count]; i += count
        else:
            out += bytes([data[i]]) * count; i += 1

def parse_segment(rom, bank, cpu):
    d = rom.banks[bank]
    o = cpu - 0x8000
    ppu_addr = (d[o] << 8) | d[o+1]
    payload, _ = decode_rle(d, o+2)
    return ppu_addr, payload

def parse_load_table(rom, table_cpu):
    d = rom.banks[7]
    o = table_cpu - 0xC000
    entries = []
    while d[o] != 0xFF:
        entries.append((d[o], d[o+1] | (d[o+2] << 8)))
        o += 3
    return entries

def load_scene(rom, scene_table_cpu, scene_index):
    d = rom.banks[7]
    o = scene_table_cpu - 0xC000 + scene_index * 2
    table_cpu = d[o] | (d[o+1] << 8)
    chr_ram = bytearray(CHR_RAM_SIZE)
    for bank, seg_cpu in parse_load_table(rom, table_cpu):
        ppu_addr, payload = parse_segment(rom, bank, seg_cpu)
        if ppu_addr + len(payload) > CHR_RAM_SIZE:
            raise ValueError(f'segment ${seg_cpu:04X} bank {bank} overruns CHR RAM at ${ppu_addr:04X}+{len(payload)}')
        chr_ram[ppu_addr:ppu_addr+len(payload)] = payload
    return chr_ram
