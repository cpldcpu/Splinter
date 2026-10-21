# NY8 assembler

| Path | What |
|---|---|
| `tools/nyasm.py` | Two-pass assembler: Nyquest syntax and encoding, `INCLUDE`, `IF`/`IFDEF`, expressions; outputs hex, listing and `ny8_image.h` |
| `tools/ny8isa.py` | Instruction table, encoder, decoder |
| `tools/ny8dis.py` | Disassembler for hex files, `ny8_image.h` and ROM dumps (`--xor 0x3F00` for raw OTP words) |
| `tools/llvm_xcheck.py` | Compares the encoder with the Nyquest LLVM assembler (NYC_NY8 + Wine) |
| `inc/ny8a050e1.inc` | NY8A050E1 register and bit names |
| `inc/delay.inc` | `delay_ms` routine, loop counts computed from `FINST` |
| `examples/` | Example programs with Makefile, see `examples/README.md` |

```
python3 tools/nyasm.py -I inc -d NY8A050E1 prog.asm -l prog.lst -H ../ny8_image.h
```

These files come from the ny8cpu project (Verilog model of the NY8 core with reference
model and tests). `make sim` in `examples/` runs the programs on that Verilog model when
`NY8CPU=<path to ny8cpu>` is given; it is not part of this repository.
