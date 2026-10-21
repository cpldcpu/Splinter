#!/usr/bin/env python3
"""NY8 14-bit instruction set: encoding table, encoder and decoder.

The encoding is the one used by Nyquest's toolchain (NYC_NY8 2.30, LLVM target
`-mcpu=ny8a051j`), obtained by assembling every instruction form and reading back
the object code. Shared by the assembler (nyasm.py), the disassembler (ny8dis.py)
and the reference model (ny8iss.py).

Operand formats
  -     no operand
  f     4-bit F/S-page address (bits 3:0)
  f5    like f, but only 5..F (0..4 are other instructions)
  r     6-bit register address (bits 5:0, bit 6 = 0)
  rd    register + destination d (bit 7; 0 = ACC, 1 = register)
  rb    register + bit number b (bits 9:7)
  i     8-bit immediate (bits 7:0)
  a8 / a9 / a10   program address (CALL / GOTO / LGOTO, LCALL)
"""

ISA = [
    # mnemonic  base    format
    ('NOP',     0x0000, '-'),
    ('SLEEP',   0x0001, '-'),
    ('CLRWDT',  0x0002, '-'),
    ('T0MD',    0x0003, '-'),
    ('ENI',     0x0004, '-'),
    ('IOST',    0x0000, 'f5'),
    ('RET',     0x0010, '-'),
    ('RETIE',   0x0011, '-'),
    ('DAA',     0x0012, '-'),
    ('DISI',    0x0013, '-'),
    ('T0MDR',   0x0014, '-'),
    ('IOSTR',   0x0010, 'f5'),
    ('SFUN',    0x0020, 'f'),
    ('SFUNR',   0x0030, 'f'),
    ('MOVAR',   0x0080, 'r'),
    ('MOVR',    0x0100, 'rd'),
    ('CLRA',    0x0200, '-'),
    ('INT',     0x0210, '-'),
    ('TABLEA',  0x0211, '-'),
    ('CALLA',   0x0212, '-'),
    ('GOTOA',   0x0213, '-'),
    ('CLRR',    0x0280, 'r'),
    ('ADDAR',   0x0300, 'rd'),
    ('SUBAR',   0x0400, 'rd'),
    ('INCR',    0x0500, 'rd'),
    ('DECR',    0x0600, 'rd'),
    ('COMR',    0x0700, 'rd'),
    ('BTRSS',   0x0800, 'rb'),
    ('BTRSC',   0x0C00, 'rb'),
    ('ANDAR',   0x1000, 'rd'),
    ('IORAR',   0x1100, 'rd'),
    ('XORAR',   0x1200, 'rd'),
    ('RRR',     0x1300, 'rd'),
    ('RLR',     0x1400, 'rd'),
    ('SWAPR',   0x1500, 'rd'),
    ('INCRSZ',  0x1600, 'rd'),
    ('DECRSZ',  0x1700, 'rd'),
    ('BSR',     0x1800, 'rb'),
    ('BCR',     0x1C00, 'rb'),
    ('RETIA',   0x2000, 'i'),
    ('MOVIA',   0x2100, 'i'),
    ('ANDIA',   0x2200, 'i'),
    ('IORIA',   0x2300, 'i'),
    ('XORIA',   0x2400, 'i'),
    ('ADDIA',   0x2500, 'i'),
    ('ADCIA',   0x2600, 'i'),
    ('SUBIA',   0x2700, 'i'),
    ('LCALL',   0x2800, 'a10'),
    ('SBCIA',   0x3000, 'i'),
    ('CALL',    0x3100, 'a8'),
    ('GOTO',    0x3200, 'a9'),
    ('ADCAR',   0x3400, 'rd'),
    ('SBCAR',   0x3500, 'rd'),
    ('CMPAR',   0x3680, 'r'),
    ('LGOTO',   0x3800, 'a10'),
]

MASK = {'-': 0x3FFF, 'f': 0x3FF0, 'f5': 0x3FF0, 'r': 0x3FC0, 'rd': 0x3F40, 'rb': 0x3C40,
        'i': 0x3F00, 'a8': 0x3F00, 'a9': 0x3E00, 'a10': 0x3C00}
NOPS = {'-': 0, 'f': 1, 'f5': 1, 'r': 1, 'rd': 2, 'rb': 2, 'i': 1, 'a8': 1, 'a9': 1, 'a10': 1}

BY_NAME = {m: (base, fmt) for m, base, fmt in ISA}


class EncodeError(Exception):
    pass


def _rng(mn, v, lo, hi, what):
    if not lo <= v <= hi:
        raise EncodeError(f'{what} {v:#x} out of range for {mn} ({lo:#x}..{hi:#x})')
    return v


def encode(mn, vals):
    """Encode mnemonic with already evaluated operand values. Returns the 14-bit word."""
    base, fmt = BY_NAME[mn]
    if len(vals) != NOPS[fmt]:
        raise EncodeError(f'{mn} expects {NOPS[fmt]} operand(s), got {len(vals)}')
    if fmt == '-':
        return base
    if fmt == 'f':
        return base | _rng(mn, vals[0], 0, 15, 'address')
    if fmt == 'f5':
        return base | _rng(mn, vals[0], 5, 15, 'address')
    if fmt == 'r':
        return base | _rng(mn, vals[0], 0, 0x3F, 'register')
    if fmt == 'rd':
        return base | _rng(mn, vals[1], 0, 1, 'destination') << 7 | _rng(mn, vals[0], 0, 0x3F, 'register')
    if fmt == 'rb':
        return base | _rng(mn, vals[1], 0, 7, 'bit') << 7 | _rng(mn, vals[0], 0, 0x3F, 'register')
    if fmt == 'i':
        v = vals[0]
        if -128 <= v < 0:
            v &= 0xFF
        return base | _rng(mn, v, 0, 0xFF, 'immediate')
    bits = int(fmt[1:])
    return base | (vals[0] & ((1 << bits) - 1))


def decode(w):
    """Return (mnemonic, fields dict) or None for an undefined word."""
    w &= 0x3FFF
    for mn, base, fmt in ISA:
        if w & MASK[fmt] != base:
            continue
        if fmt == '-':
            return mn, {}
        if fmt in ('f', 'f5'):
            f = w & 0xF
            if fmt == 'f5' and f < 5:
                continue
            return mn, {'f': f}
        if fmt == 'r':
            return mn, {'r': w & 0x3F}
        if fmt == 'rd':
            return mn, {'r': w & 0x3F, 'd': w >> 7 & 1}
        if fmt == 'rb':
            return mn, {'r': w & 0x3F, 'b': w >> 7 & 7}
        if fmt == 'i':
            return mn, {'i': w & 0xFF}
        return mn, {'a': w & ((1 << int(fmt[1:])) - 1)}
    return None


def disasm(w):
    d = decode(w)
    if d is None:
        return f'DW 0x{w:04X}'
    mn, f = d
    fmt = BY_NAME[mn][1]
    if fmt == '-':
        return mn
    if fmt in ('f', 'f5'):
        return f'{mn} 0x{f["f"]:X}'
    if fmt == 'r':
        return f'{mn} 0x{f["r"]:02X}'
    if fmt == 'rd':
        return f'{mn} 0x{f["r"]:02X}, {f["d"]}'
    if fmt == 'rb':
        return f'{mn} 0x{f["r"]:02X}, {f["b"]}'
    if fmt == 'i':
        return f'{mn} 0x{f["i"]:02X}'
    return f'{mn} 0x{f["a"]:03X}'
