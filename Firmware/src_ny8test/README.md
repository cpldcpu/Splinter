# Nyquest NY8 Programmer Test Firmware (NY8A050E1)

Bring-up harness for programming Nyquest NY8A0xx OTP parts with Splinter R0.
It reuses `../src_pgmtest/power.c` (BOOST/VDD/VPP control) unchanged.

The protocol is reconstructed from the NY8-OTP_Writer (Ver. B) firmware shipped inside
Q-Writer 6.60 (8051 code, `ny8_otp_writer_b_firmware`). Nothing here has been verified on silicon yet.

## Socket mapping (Splinter R0, SOP8 socket)

| Socket pin | Splinter net | CH32V003 | NY8A050E1 SOP8 |
|---|---|---|---|
| 1 | DEV_VDD | – | VDD |
| 2 | PA7 | PD5 | PB5 (high-Z) |
| 3 | ICPDA_MISO | PC7 | PB4 (high-Z) |
| 4 | DEV_ICVPP | – | PB3 / VPP |
| 5 | ICPCK | PC5 | PB2 / SCK |
| 6 | MOSI | PC6 | PB1 / SDO (input) |
| 7 | PA0 | PD6 | PB0 / SDI |
| 8 | GND (1 Ω shunt) | – | VSS |

The NY8A050E1 SOP8 pinout matches the Padauk SOP8 socket on VDD/VPP/GND.

## Connecting the NY8A050E1S6 (SOT23-6)

The SOT23-6 package uses a different pin order than the SOP8 socket, so it needs an
adapter or six wires. Programming uses only PB0..PB3, VDD and VSS.

```
   NY8A050E1S6, top view                 Splinter R0 socket (SOP8 position)
   (dot = pin 1)                          and header pins
          +-------+
   SDI  1 |o      | 6  VPP               socket 1 = H1.5  DEV_VDD
   PB0    |       |    PB3               socket 4 = H1.8  DEV_ICVPP
   VSS  2 |       | 5  VDD               socket 5 = H2.8  ICPCK (PC5)
          |       |                      socket 6 = H2.7  MOSI  (PC6)
   SDO  3 |       | 4  SCK               socket 7 = H2.6  PA0   (PD6)
   PB1    +-------+    PB2               socket 8 = H2.5  GND (1 ohm shunt)
```

| SOT23-6 pin | Function | Splinter socket pin | Header pin | Net |
|---|---|---|---|---|
| 1 | PB0 / SDI | 7 | H2.6 (or H3.6) | PA0 (PD6), programmer drives |
| 2 | VSS | 8 | H2.5 (or H3.5) | GND via 1 Ω shunt |
| 3 | PB1 / SDO | 6 | H2.7 (or H3.7) | MOSI (PC6), programmer reads |
| 4 | PB2 / SCK | 5 | H2.8 (or H3.8) | ICPCK (PC5) |
| 5 | VDD | 1 | H1.5 | DEV_VDD |
| 6 | PB3 / VPP | 4 | H1.8 | DEV_ICVPP |
| – | – | 2, 3 | H1.6, H1.7 | leave open |

Options:
- A SOT23-6 breakout board (SOT23-6 to 2.54 mm pins) with six jumper wires to the headers
  listed above. Keep the wires short (< 10 cm); the writer manual allows at most 680 pF on
  SDI/SDO/SCK.
- An adapter from SOT23-6 to the SOP8 socket wired 1→7, 2→8, 3→6, 4→5, 5→1, 6→4. This is the
  same mapping as Microchip's PIC10F (SOT23-6) to PIC12F (8-pin) adapters.

**Do not use a Padauk SOT23-6 adapter.** It routes socket pin 4 (VPP) to SOT23-6 pin 4,
which is PB2/SCK on the NY8A050E1, and SCK would see the programming voltage.

Check before powering: VDD (pin 5) and VSS (pin 2) must not be swapped, and VPP must reach
pin 6 only. Run the read-only step first; it never raises VPP above VDD.

## Protocol summary

- Commands: 8 bits MSB first (`0x60` read, `0x40` read variant, `0x70` program word,
  `0x50` program with auto-increment). Some writer modes append a `0x00` byte.
- Address/data frames: 16 bits `[d13..d7, 0, d6..d0, F]`, F=1 on program data.
- SDI changes while SCK is high; SDO is sampled after the falling edge.
- Read burst: command, address frame, n × 16-bit reads; on the last read SDI goes high
  during bit 15, followed by one extra clock.
- Program word: `0x70`, address frame, VPP on, 5 ms, data frame with the last SCK-high
  phase stretched to ~90 µs (programming pulse) plus one extra clock, VPP off, 10 ms.
- Entry (writer mode 0): VDD on with SCK/SDI low, 5 ms, SDO pulled low by the
  programmer for ~12 ms, release, 1 ms. Alternative entry (key): VDD on, 30–150 µs,
  key bytes `0x53 0xAD`.

## Chip data (Q-Writer databases)

- NY8A050E1: 512 words ROM, 32 B RAM, 16 config words, chipid1 0x001F, chipid0 0x0000 (rev A) / 0x0001 (rev B/C)
- `WRITER_Enter_Mode` 0, `Program_High_Voltage_setting` 0x46 (probably 7.0 V)
- 0x00E/0x00F: factory rolling code (non-blank on a new part); 0x1FE/0x1FF reserved
- Config words: 16 words directly behind the ROM, word addresses 0x200..0x20F (NYC_NY8 linker
  config). Within the block: IHRC trim 0x0D[6:0], ILRC trim 0x0E[2:0], read protect 0x0F[0],
  factory data in 0x05 (do not touch)

## Creating programs

Nyquest's C compiler package NYC_NY8 (2.30) contains an LLVM toolchain with an NY8 target
(`llvm/bin`: clang, ld.lld, llvm-objdump). Its CPU model `ny8a051j` matches the NY8A050E1
(512-word ROM, same register map for the blocks the chip has).

- `example/blink.c` - toggles PB2
- `example/ny8a050e1.ld` - linker script: code from word 0x010 (keeps the factory rolling
  code at 0x00E/0x00F intact), 0x1FE/0x1FF left free, RAM 0x10..0x2F
- `example/build.sh` - compile + link, then `tools/ny8_elf2h.py` writes `ny8_image.h`
  (address/word pairs; gaps stay unprogrammed). On Linux run with `RUN=wine NYC_LLVM=...`.
- `NY8_ISA.md` - instruction encoding table

`ny8_image.h` in this directory is the blink example. Programming applies `NY8_IMAGE_XOR`
(default 0x3F00, see NY8_ISA.md); the first programmed chip shows whether it is right.

The configuration words (behind the 512-word ROM) are not written yet, so the chip runs
with blank options and an untrimmed oscillator.

## Bring-up

1. `NY8TEST_PROGRAM 0` (default): tries the entry variants and dumps ROM 0x000–0x21F.
   VPP stays at 0 V or VDD. A working variant shows non-blank values at 0x00E/0x00F and
   0x3FFF elsewhere on a new part. Raw frames are printed in brackets next to the decoded words.
2. `NY8TEST_PROGRAM 1`: programs 0x3FFE into word 0x1F0 with a VPP sweep from 6.0 V and
   reads back after each step. Stop at the first success.
3. `NY8TEST_WRITE_IMAGE 1`: blank-checks, programs and verifies `ny8_image.h` at
   `NY8TEST_VPP_PROG_MV` (set from step 2).

Known limitations:
- VPP resolution of `power.c` is ~0.53 V per PWM step (1 MHz PWM, period 48). A finer
  sweep needs a lower PWM frequency or dithering.
- The VDD rail rises in some 100 µs, too slow for the timed key entry; plain entry is the
  documented mode for this part.
- The VPP pin level outside programming (0 V or VDD) is not known; both are tried.
