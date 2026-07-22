"""结构校验：返回错误字符串列表，空列表 = 通过。"""
from tables import cpu_to_offset, entry_length, entry_bytes, BANK_SIZE
from extractors import decode_nametable_rle

def validate_registry(rom, registry):
    errors = []
    for e in registry:
        try:
            off = cpu_to_offset(e.bank, e.cpu) % BANK_SIZE
            n = entry_length(e)
            if off + n > BANK_SIZE:
                errors.append(f'{e.name}: offset ${off:04X}+len {n} overruns bank {e.bank} ({e.asm_ref})')
        except Exception as ex:
            errors.append(f'{e.name}: {ex} ({e.asm_ref})')
        if e.kind == 'metasprite' and not (0 <= e.count <= 64):  # count 0 = tblNullObject 合法空精灵
            errors.append(f'{e.name}: sprite count {e.count} out of range ({e.asm_ref})')
        if e.kind == 'nametable_rle':
            try:
                segs = decode_nametable_rle(entry_bytes(rom, e))
                if sum(len(d) for _, d in segs) < 0x400:
                    errors.append(f'{e.name}: decoded total < $400 ({e.asm_ref})')
                for addr, _ in segs:
                    if not (0x2000 <= addr <= 0x2FFF):
                        errors.append(f'{e.name}: segment addr ${addr:04X} outside nametable ({e.asm_ref})')
            except Exception as ex:
                errors.append(f'{e.name}: nametable_rle decode: {ex} ({e.asm_ref})')
        if e.kind == 'ppu_stream':
            try:
                d = entry_bytes(rom, e)
                addr = (d[0] << 8) | d[1]
                if not (0x2000 <= addr <= 0x3F0F):
                    errors.append(f'{e.name}: stream addr ${addr:04X} out of range ({e.asm_ref})')
                if d[-1] not in (0xFE, 0xFF) or 0xFF in d[2:-1] or 0xFE in d[2:-1]:
                    errors.append(f'{e.name}: stream terminator misplaced ({e.asm_ref})')
            except Exception as ex:
                errors.append(f'{e.name}: ppu_stream: {ex} ({e.asm_ref})')
    # 指针回环：ptr_table 的每个 dw 必须指向某条已登记条目的起始 cpu
    for e in registry:
        if e.kind != 'ptr_table':
            continue
        try:
            raw = entry_bytes(rom, e)
        except Exception:
            continue
        targets = set(x.cpu for x in registry if x is not e)  # dw 目标可跨 bank（如 Bank7 串接表指向 Bank4/5）
        for i in range(0, len(raw) - 1, 2):
            ptr = raw[i] | (raw[i+1] << 8)
            if ptr and ptr not in targets:
                errors.append(f'{e.name}[{i//2}]: dw ${ptr:04X} not a registered entry in bank {e.bank} ({e.asm_ref})')
    return errors
