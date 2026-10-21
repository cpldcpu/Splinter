# NY8 (14-bit) instruction encoding

Obtained by assembling every instruction with the NY8 target of Nyquest's LLVM toolchain
(NYC_NY8 2.30, `clang --target=ny8 -mcpu=ny8a051j`) and reading back the object code.
The table applies to the first-generation 14-bit NY8 parts: NY8A050D/E/E1 (all packages,
including NY8A050E1S6) and NY8A051B..L/H1. NYC_NY8's SDCC device table and linker configs list
these with the same core and differing only in ROM/RAM size. The "16-bit ROM" series
(e.g. NY8BM84A, NY8F2481) uses a different encoding (separate decoder table in the LLVM target).

R = 6-bit register address (bit 6 is 0), d = destination (0: ACC, 1: R), b = bit number,
i = 8-bit immediate, f = 4-bit F/S-page address, a = program address.

| Encoding (hex) | Bits 13..0 | Instruction |
|---|---|---|
| 0000 | `00 0000 0000 0000` | NOP |
| 0001 | `00 0000 0000 0001` | SLEEP |
| 0002 | `00 0000 0000 0010` | CLRWDT |
| 0003 | `00 0000 0000 0011` | T0MD |
| 0004 | `00 0000 0000 0100` | ENI |
| 0000+f | `00 0000 0000 ffff` | IOST f (f = 5..F) |
| 0010 | `00 0000 0001 0000` | RET |
| 0011 | `00 0000 0001 0001` | RETIE |
| 0012 | `00 0000 0001 0010` | DAA |
| 0013 | `00 0000 0001 0011` | DISI |
| 0014 | `00 0000 0001 0100` | T0MDR |
| 0010+f | `00 0000 0001 ffff` | IOSTR f (f = 5..F) |
| 0020+f | `00 0000 0010 ffff` | SFUN f |
| 0030+f | `00 0000 0011 ffff` | SFUNR f |
| 0080+R | `00 0000 10RR RRRR` | MOVAR R |
| 0100 | `00 0001 d0RR RRRR` | MOVR R,d |
| 0200 | `00 0010 0000 0000` | CLRA |
| 0210 | `00 0010 0001 0000` | INT |
| 0211 | `00 0010 0001 0001` | TABLEA |
| 0212 | `00 0010 0001 0010` | CALLA |
| 0213 | `00 0010 0001 0011` | GOTOA |
| 0280+R | `00 0010 10RR RRRR` | CLRR R |
| 0300 | `00 0011 d0RR RRRR` | ADDAR R,d |
| 0400 | `00 0100 d0RR RRRR` | SUBAR R,d |
| 0500 | `00 0101 d0RR RRRR` | INCR R,d |
| 0600 | `00 0110 d0RR RRRR` | DECR R,d |
| 0700 | `00 0111 d0RR RRRR` | COMR R,d |
| 0800 | `00 10bb b0RR RRRR` | BTRSS R,b |
| 0C00 | `00 11bb b0RR RRRR` | BTRSC R,b |
| 1000 | `01 0000 d0RR RRRR` | ANDAR R,d |
| 1100 | `01 0001 d0RR RRRR` | IORAR R,d |
| 1200 | `01 0010 d0RR RRRR` | XORAR R,d |
| 1300 | `01 0011 d0RR RRRR` | RRR R,d |
| 1400 | `01 0100 d0RR RRRR` | RLR R,d |
| 1500 | `01 0101 d0RR RRRR` | SWAPR R,d |
| 1600 | `01 0110 d0RR RRRR` | INCRSZ R,d |
| 1700 | `01 0111 d0RR RRRR` | DECRSZ R,d |
| 1800 | `01 10bb b0RR RRRR` | BSR R,b |
| 1C00 | `01 11bb b0RR RRRR` | BCR R,b |
| 2000 | `10 0000 iiii iiii` | RETIA i |
| 2100 | `10 0001 iiii iiii` | MOVIA i |
| 2200 | `10 0010 iiii iiii` | ANDIA i |
| 2300 | `10 0011 iiii iiii` | IORIA i |
| 2400 | `10 0100 iiii iiii` | XORIA i |
| 2500 | `10 0101 iiii iiii` | ADDIA i |
| 2600 | `10 0110 iiii iiii` | ADCIA i |
| 2700 | `10 0111 iiii iiii` | SUBIA i |
| 2800 | `10 10aa aaaa aaaa` | LCALL a (10 bit) |
| 3000 | `11 0000 iiii iiii` | SBCIA i |
| 3100 | `11 0001 aaaa aaaa` | CALL a (8 bit, PC[9:8] from PCHBUF) |
| 3200 | `11 001a aaaa aaaa` | GOTO a (9 bit, PC[9] from PCHBUF) |
| 3400 | `11 0100 d0RR RRRR` | ADCAR R,d |
| 3500 | `11 0101 d0RR RRRR` | SBCAR R,d |
| 3680+R | `11 0110 10RR RRRR` | CMPAR R |
| 3800 | `11 10aa aaaa aaaa` | LGOTO a (10 bit) |

Notes:
- The LLVM assembler truncates numeric GOTO/CALL operands to 8 bits, and the linker relocation
  for GOTO/CALL to labels inserted a byte address in a test. The compiler emits LGOTO/LCALL,
  which link correctly. The 9-bit GOTO field is inferred from the opcode map (0x3200..0x33FF)
  and the NY8A050E1 datasheet ("For GOTO instruction, PC[8:0] is from instruction word").
- `asm/tools/ny8isa.py` contains this table in machine-readable form; `asm/tools/llvm_xcheck.py`
  compares it with the LLVM assembler (6,709 instruction forms, no differences).
- Words not in the table (e.g. 0x0040..0x007F, 0x2C00..0x2FFF, 0x3C00..0x3FFF) are not
  produced by the toolchain for `ny8a051j`; the 2K-word parts probably use 0x2C00/0x3C00 for
  11-bit LCALL/LGOTO.
- **OTP word format (unconfirmed):** Q-Writer identifies MOVIA as `0x1E00 | i` and RETIA as
  `0x1F00 | i` in the OTP buffer it sends to the writer. Relative to this table, bits 13..8
  are inverted (XOR 0x3F00) and the immediate byte is unchanged. `src_ny8test` applies
  `NY8_IMAGE_XOR` (default 0x3F00) when programming. Only two data points support this
  (both literal instructions), so the first programmed chip decides: if blink does not run,
  try `NY8_IMAGE_XOR 0` on a fresh chip.
