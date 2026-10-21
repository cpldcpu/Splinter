#!/usr/bin/env python3
"""Disassembler for NY8 14-bit code.

Input: a $readmemh file (one word per line), an ny8_image.h ({addr, word} pairs) or a
list of 'addr word' hex pairs (e.g. a ROM dump). Words are in toolchain encoding;
use --xor 0x3F00 for words read back from an OTP part if the OTP stores them inverted
(see NY8_ISA.md).

usage: ny8dis.py file [--xor N] [--all]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ny8isa  # noqa: E402


def load(path):
    text = open(path).read()
    pairs = re.findall(r'\{\s*0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*\}', text)
    if pairs:
        return {int(a, 16): int(w, 16) for a, w in pairs}
    words = {}
    lines = [l.split('//')[0].split(';')[0].split() for l in text.splitlines()]
    lines = [l for l in lines if l]
    if all(len(l) >= 2 for l in lines):
        for l in lines:
            words[int(l[0].rstrip(':'), 16)] = int(l[1], 16)
    else:
        for i, l in enumerate(lines):
            words[i] = int(l[0], 16)
    return words


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('file')
    ap.add_argument('--xor', type=lambda s: int(s, 0), default=0)
    ap.add_argument('--all', action='store_true', help='also list 0x0000 words of a hex image')
    a = ap.parse_args()
    words = load(a.file)
    last = max(words) if words else 0
    if not a.all:
        while last > 0 and words.get(last, 0) == 0:
            last -= 1
    for addr in sorted(words):
        if addr > last:
            break
        w = (words[addr] ^ a.xor) & 0x3FFF
        print(f'{addr:03X}  {w:04X}  {ny8isa.disasm(w)}')


if __name__ == '__main__':
    main()
