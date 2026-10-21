#!/usr/bin/env python3
"""Two-pass assembler for the Nyquest NY8 14-bit instruction set (NY8A050/NY8A051 family).

Encoding: tools/ny8isa.py (Nyquest toolchain encoding, cross-checked against the
NYC_NY8 LLVM assembler, see tools/check_llvm.sh).

Source syntax
  label:    MNEMONIC operands         ; comment
  NAME      EQU  expr                 (also NAME = expr, .equ NAME, expr)
            ORG  expr
            DW   expr[, expr ...]     raw 14-bit words (tables for TABLEA)
            INCLUDE "file"            searched next to the source, then -I dirs
            IFDEF / IFNDEF NAME, IF expr, ELSE, ENDIF   (NAME may come from -D NAME=value)
            ERROR "text"              stops with an error (inside IF)
            END                       stops assembly (optional)
  Operand order follows the Nyquest syntax: R,d for byte ops (d: 0/A = ACC, 1/R = register,
  default 1 when omitted), R,b for bit ops.
  Expressions: Python integer syntax (0x.., 0b.., decimal, 'c'), + - * / % << >> & | ^ ~,
  comparisons (< <= == != >= >), and/or/not, low(x), high(x), $ = address of the current
  instruction. / is integer division. Symbols are case insensitive.

Outputs
  -o file.hex   $readmemh image for the simulator (always 1024 words, gaps = 0)
  -l file.lst   listing
  -H file.h     {address, word} table for the Splinter programmer (src_ny8test/ny8_image.h),
                only the words the program defines, toolchain encoding
  -d DEVICE     ROM size and reserved words of the target (NY8A050E1, NY8A051H1)
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ny8isa  # noqa: E402

DEVICES = {
    # rom words, reserved words (factory rolling code / reserved area)
    'NY8A050E1': (512, (0x00E, 0x00F, 0x1FE, 0x1FF)),
    'NY8A051H1': (1024, ()),
    'SIM': (1024, ()),
}

BUILTIN = {'A': 0, 'R': 1}


class AsmError(Exception):
    pass


def split_ops(s):
    s = s.strip()
    if not s:
        return []
    out, depth, cur, q = [], 0, '', None
    for ch in s:
        if q:
            cur += ch
            if ch == q:
                q = None
            continue
        if ch in '\'"':
            q = ch
        elif ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
            continue
        depth += ch == '('
        depth -= ch == ')'
        cur += ch
    out.append(cur.strip())
    return out


def strip_comment(line):
    q = None
    for i, ch in enumerate(line):
        if q:
            if ch == q:
                q = None
        elif ch in '\'"':
            q = ch
        elif ch == ';':
            return line[:i]
    return line


class Assembler:
    def __init__(self, incdirs=(), defines=None, rom_words=1024, reserved=()):
        self.incdirs = list(incdirs)
        self.rom_words = rom_words
        self.reserved = set(reserved)
        self.base_syms = dict(BUILTIN)
        for k, v in (defines or {}).items():
            self.base_syms[k.upper()] = v
        self.warnings = []
        self.list_inc = False

    # ---------------- source handling ----------------
    def load(self, path, depth=0):
        """Return list of (file, line number, text) with INCLUDE expanded."""
        if depth > 16:
            raise AsmError('INCLUDE nested too deep')
        out = []
        for ln, raw in enumerate(open(path).read().splitlines(), 1):
            m = re.match(r'^\s*(?:#|\.)?include\s+["<]([^">]+)[">]', raw, re.I)
            if m:
                out.append((path, ln, raw))
                out.extend(self.load(self.find_include(m.group(1), path, ln), depth + 1))
                out.append((path, ln, None))   # marker: back in the parent file
                continue
            out.append((path, ln, raw))
        return out

    def find_include(self, name, src, ln):
        for d in [os.path.dirname(os.path.abspath(src))] + self.incdirs:
            p = os.path.join(d, name)
            if os.path.exists(p):
                return p
        raise AsmError(f'{src}:{ln}: include file "{name}" not found')

    # ---------------- expressions ----------------
    def evaluate(self, expr, final, pc):
        env = {'low': lambda x: x & 0xFF, 'high': lambda x: (x >> 8) & 0xFF,
               '__pc': pc, '__builtins__': {}}
        expr = re.sub(r"'(.)'", lambda m: str(ord(m.group(1))), expr)
        expr = expr.replace('$', '__pc')

        def repl(m):
            w = m.group(0)
            if w in ('low', 'high', '__pc') or re.fullmatch(r'0[xXbBoO][0-9a-fA-F_]+|\d+', w):
                return w
            if w.lower() in ('and', 'or', 'not'):
                return f' {w.lower()} '
            k = w.upper()
            if k in self.syms:
                return f'({self.syms[k]})'
            if final:
                raise AsmError(f'undefined symbol {w}')
            return '0'

        e2 = re.sub(r'0[xXbBoO][0-9a-fA-F_]+|[A-Za-z_][A-Za-z0-9_]*|\d+', repl, expr)
        e2 = e2.replace('/', '//')
        try:
            return int(eval(e2, env))
        except AsmError:
            raise
        except Exception as e:
            raise AsmError(f'bad expression "{expr}": {e}')

    # ---------------- main ----------------
    def assemble(self, path):
        src = self.load(path)
        self.words = {}
        for final in (False, True):
            if not final:
                self.syms = dict(self.base_syms)
                defined = set()
            self.seen = set(self.base_syms)        # symbols defined so far in this pass (IFDEF)
            listing = []
            cond = []                              # [active, branch taken, parent active]
            pc = 0
            for fname, ln, raw in src:
                if raw is None:
                    continue
                try:
                    active = all(c[0] for c in cond)
                    m = re.match(r'^\s*\.?(IFDEF|IFNDEF|IF|ELSE|ENDIF)\b\s*(.*)$', strip_comment(raw).rstrip(), re.I)
                    if m:
                        self.conditional(m.group(1).upper(), m.group(2).strip(), cond, active, pc)
                        if final and (fname == path or self.list_inc):
                            listing.append(f'{"":13}{raw}')
                        continue
                    if not active:
                        if final and (fname == path or self.list_inc):
                            listing.append(f'{"":11}- {raw}')
                        continue
                    n_lst, n_words = len(listing), len(self.words)
                    pc, ended = self.line(raw, final, pc, listing, defined)
                    if final and fname != path and len(self.words) == n_words and not self.list_inc:
                        del listing[n_lst:]        # include files: list only lines with code
                except (AsmError, ny8isa.EncodeError) as e:
                    raise AsmError(f'{fname}:{ln}: {e}\n    {raw.strip()}')
                if ended:
                    break
            if cond:
                raise AsmError(f'{path}: missing ENDIF')
        self.listing = listing
        return self.words

    def conditional(self, kw, arg, cond, active, pc):
        if kw in ('IFDEF', 'IFNDEF', 'IF'):
            if not active:
                val = False
            elif kw == 'IF':
                val = self.evaluate(arg, True, pc) != 0
            else:
                val = (arg.upper() in self.seen) == (kw == 'IFDEF')
            cond.append([val, val, active])
        elif kw == 'ELSE':
            if not cond:
                raise AsmError('ELSE without IF')
            c = cond[-1]
            c[0] = c[2] and not c[1]
            c[1] = True
        else:
            if not cond:
                raise AsmError('ENDIF without IF')
            cond.pop()

    def line(self, raw, final, pc, listing, defined):
        line = strip_comment(raw).rstrip()
        lst = lambda addr, w: listing.append(
            (f'{addr:03X}  {w:04X}  ' if w is not None else (f'{addr:03X}{"":8}' if addr is not None else f'{"":13}')) + raw)
        if re.match(r'^\s*(?:#|\.)?include\s', line, re.I):
            if final:
                lst(None, None)
            return pc, False
        m = re.match(r'^\s*([A-Za-z_.][\w.]*)\s*:(.*)$', line)
        label = None
        if m and not re.match(r'^\s*[A-Za-z_]\w*\s+(EQU|=)', line, re.I):
            label = m.group(1).upper()
            if not final:
                if label in defined:
                    raise AsmError(f'duplicate label {label}')
                defined.add(label)
                self.syms[label] = pc
            elif self.syms.get(label) != pc:
                raise AsmError(f'label {label} moved between passes')
            self.seen.add(label)
            line = m.group(2)
        if not line.strip():
            if final:
                lst(pc if label else None, None)
            return pc, False

        m = re.match(r'^\s*([A-Za-z_]\w*)\s+(?:EQU|=)\s*(.*)$', line, re.I) or \
            re.match(r'^\s*([A-Za-z_]\w*)\s*=\s*(.*)$', line)
        if m:
            name = m.group(1).upper()
            if not final and name in defined:
                raise AsmError(f'symbol {name} defined twice')
            defined.add(name)
            self.seen.add(name)
            self.syms[name] = self.evaluate(m.group(2), final, pc)
            if final:
                lst(None, None)
            return pc, False

        toks = line.strip().split(None, 1)
        mn = toks[0].upper()
        rest = toks[1] if len(toks) > 1 else ''
        if mn == '.EQU':
            a, b = split_ops(rest)
            self.seen.add(a.upper())
            self.syms[a.upper()] = self.evaluate(b, final, pc)
            if final:
                lst(None, None)
            return pc, False
        if mn in ('ORG', '.ORG'):
            pc = self.evaluate(rest, final, pc)
            if final:
                lst(None, None)
            return pc, False
        if mn in ('ERROR', '.ERROR'):
            raise AsmError('ERROR ' + rest.strip().strip('"'))
        if mn in ('END', '.END'):
            if final:
                lst(None, None)
            return pc, True
        if mn in ('DW', '.DW'):
            first = True
            for e in split_ops(rest):
                if final:
                    w = self.evaluate(e, True, pc)
                    if not -0x2000 <= w <= 0x3FFF:
                        raise AsmError(f'DW value {w:#x} does not fit 14 bits')
                    self.put(pc, w & 0x3FFF)
                    if first:
                        lst(pc, w & 0x3FFF)
                    else:
                        listing.append(f'{pc:03X}  {w & 0x3FFF:04X}')
                    first = False
                pc += 1
            return pc, False
        if mn not in ny8isa.BY_NAME:
            raise AsmError(f'unknown mnemonic {toks[0]}')
        if final:
            ops = split_ops(rest)
            fmt = ny8isa.BY_NAME[mn][1]
            if fmt == 'rd' and len(ops) == 1:
                ops.append('1')
            vals = [self.evaluate(o, True, pc) for o in ops]
            if fmt in ('a8', 'a9', 'a10') and vals:
                self.check_target(mn, fmt, vals[0], pc)
            w = ny8isa.encode(mn, vals)
            self.put(pc, w)
            lst(pc, w)
        return pc + 1, False

    def check_target(self, mn, fmt, a, pc):
        if not 0 <= a < self.rom_words:
            raise AsmError(f'{mn} target {a:#x} outside ROM ({self.rom_words} words)')
        if fmt == 'a8' and a >= 0x100:
            self.warnings.append(f'{pc:03X}: CALL {a:#x}: bits 9:8 come from PCHBUF, use LCALL')
        if fmt == 'a9' and a >= 0x200:
            self.warnings.append(f'{pc:03X}: GOTO {a:#x}: bit 9 comes from PCHBUF, use LGOTO')

    def put(self, pc, w):
        if not 0 <= pc < self.rom_words:
            raise AsmError(f'address {pc:#x} outside ROM ({self.rom_words} words)')
        if pc in self.words:
            raise AsmError(f'address {pc:#x} used twice')
        if pc in self.reserved:
            raise AsmError(f'address {pc:#x} is reserved on this device (factory data)')
        self.words[pc] = w


def write_hex(words, path, size=1024):
    with open(path, 'w') as f:
        for a in range(size):
            f.write(f'{words.get(a, 0):04x}\n')


def write_header(words, path, src):
    with open(path, 'w') as f:
        f.write(f'// generated by nyasm.py from {os.path.basename(src)}\n')
        f.write('// {address, word} pairs in toolchain encoding (NY8_IMAGE_XOR applied when programming)\n')
        f.write('#include <stdint.h>\n\n')
        f.write('static const uint16_t ny8_image[][2] = {\n')
        for a in sorted(words):
            f.write(f'\t{{0x{a:03X}, 0x{words[a]:04X}}},\n')
        f.write('};\n')
        f.write(f'#define NY8_IMAGE_LEN {len(words)}\n')


def main():
    ap = argparse.ArgumentParser(description='NY8 assembler')
    ap.add_argument('src')
    ap.add_argument('hex', nargs='?', help='$readmemh output (same as -o)')
    ap.add_argument('lst', nargs='?', help='listing output (same as -l)')
    ap.add_argument('-o', dest='hex_opt')
    ap.add_argument('-l', dest='lst_opt')
    ap.add_argument('-H', dest='header')
    ap.add_argument('-I', dest='incdirs', action='append', default=[])
    ap.add_argument('-D', dest='defines', action='append', default=[], help='NAME=value')
    ap.add_argument('-d', dest='device', default='SIM', type=str.upper, choices=sorted(DEVICES))
    ap.add_argument('--rom-words', type=int, help='override the ROM size of the device')
    ap.add_argument('-L', dest='list_inc', action='store_true', help='list all lines of include files')
    ap.add_argument('-q', dest='quiet', action='store_true')
    a = ap.parse_args()

    defs = {}
    for d in a.defines:
        k, _, v = d.partition('=')
        defs[k] = int(v or '1', 0)
    rom, reserved = DEVICES[a.device]
    rom = a.rom_words or rom
    asm = Assembler(a.incdirs, defs, rom, reserved)
    asm.list_inc = a.list_inc
    try:
        words = asm.assemble(a.src)
    except AsmError as e:
        print(e, file=sys.stderr)
        sys.exit(1)
    for w in asm.warnings:
        print(f'{a.src}: warning: {w}', file=sys.stderr)
    hexf, lstf = a.hex_opt or a.hex, a.lst_opt or a.lst
    if hexf:
        write_hex(words, hexf)
    if lstf:
        with open(lstf, 'w') as f:
            f.write('\n'.join(asm.listing) + '\n\nSymbols\n')
            for k in sorted(asm.syms):
                if k not in BUILTIN:
                    f.write(f'  {k:24s} 0x{asm.syms[k]:X}\n')
    if a.header:
        write_header(words, a.header, a.src)
    if not a.quiet:
        print(f'{a.src}: {len(words)} words ({a.device}: {rom})')


if __name__ == '__main__':
    main()
