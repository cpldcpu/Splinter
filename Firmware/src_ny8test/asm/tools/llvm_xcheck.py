#!/usr/bin/env python3
"""Cross-check nyasm.py against the Nyquest LLVM assembler (NYC_NY8, runs under Wine).

Assembles every instruction with a sweep of operand values with both assemblers
and compares the words.

usage: llvm_xcheck.py <path to NYC_NY8 llvm/bin> [workdir]
"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ny8isa  # noqa: E402


def forms():
    for mn, base, fmt in ny8isa.ISA:
        if fmt == '-':
            yield mn, []
        elif fmt == 'f':
            for f in range(16):
                yield mn, [f]
        elif fmt == 'f5':
            for f in range(5, 16):
                yield mn, [f]
        elif fmt == 'r':
            for r in range(64):
                yield mn, [r]
        elif fmt == 'rd':
            for r in range(64):
                for d in (0, 1):
                    yield mn, [r, d]
        elif fmt == 'rb':
            for r in range(64):
                for b in range(8):
                    yield mn, [r, b]
        elif fmt == 'i':
            for i in range(256):
                yield mn, [i]
        else:
            # LLVM truncates numeric CALL/GOTO operands to 8 bits (see NY8_ISA.md),
            # so only the low 8 bits are compared for those two.
            bits = 8 if fmt in ('a8', 'a9') else 10
            for k in range(bits):
                yield mn, [1 << k]
            for a in (0, (1 << bits) - 1, 0x155 & ((1 << bits) - 1)):
                yield mn, [a]


def text_words(path):
    d = open(path, 'rb').read()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    secs = [struct.unpack_from('<IIIIIIIIII', d, shoff + i * shentsize) for i in range(shnum)]
    strtab = secs[shstrndx]
    for s in secs:
        if d[strtab[4] + s[0]:].split(b'\0', 1)[0] == b'.text':
            raw = d[s[4]:s[4] + s[5]]
            return [struct.unpack_from('<H', raw, i)[0] for i in range(0, len(raw), 2)]
    return []


def main():
    llvm = sys.argv[1]
    work = sys.argv[2] if len(sys.argv) > 2 else 'build/xcheck'
    os.makedirs(work, exist_ok=True)
    cases = list(forms())
    src = os.path.join(work, 'all.s')
    with open(src, 'w') as f:
        f.write('\t.text\n')
        for mn, v in cases:
            f.write(f'\t{mn.lower()}\t' + ', '.join(f'{x:#x}' for x in v) + '\n')
    obj = os.path.join(work, 'all.o')
    cmd = ['wine', os.path.join(llvm, 'clang.exe'), '--target=ny8', '-mcpu=ny8a051j', '-c', src, '-o', obj]
    r = subprocess.run(cmd, capture_output=True, text=True, env=dict(os.environ, WINEDEBUG='-all'))
    if r.returncode:
        sys.exit('clang failed:\n' + r.stderr[-2000:])
    ref = text_words(obj)
    if len(ref) != len(cases):
        sys.exit(f'word count differs: llvm {len(ref)}, cases {len(cases)}')
    bad = 0
    for (mn, v), w in zip(cases, ref):
        mine = ny8isa.encode(mn, v)
        if ny8isa.BY_NAME[mn][1] in ('a8', 'a9'):
            mine &= 0x3F00 | 0xFF
            w &= 0x3F00 | 0xFF
        if mine != w & 0x3FFF:
            bad += 1
            if bad <= 20:
                print(f'MISMATCH {mn} {v}: nyasm {mine:04X} llvm {w:04X}')
    print(f'{len(cases)} instruction forms compared, {bad} mismatches')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
