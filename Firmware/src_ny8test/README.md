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
**SOT23-6 (NY8A050E1S6) needs its own adapter.** A Padauk SOT23-6 adapter puts VPP on PB2/SCK.
Required SOT23-6 → SOP8 mapping: 1→7, 2→8, 3→6, 4→5, 5→1, 6→4.

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
- Config words: IHRC trim 0x0D[6:0], ILRC trim 0x0E[2:0], read protect 0x0F[0],
  factory data in 0x05 (do not touch)

## Bring-up

1. `NY8TEST_PROGRAM 0` (default): tries the entry variants and dumps ROM 0x000–0x21F.
   VPP stays at 0 V or VDD. A working variant shows non-blank values at 0x00E/0x00F and
   0x3FFF elsewhere on a new part. Raw frames are printed in brackets next to the decoded words.
2. `NY8TEST_PROGRAM 1`: programs 0x3FFE into word 0x1F0 with a VPP sweep from 6.0 V and
   reads back after each step. Stop at the first success.

Known limitations:
- VPP resolution of `power.c` is ~0.53 V per PWM step (1 MHz PWM, period 48). A finer
  sweep needs a lower PWM frequency or dithering.
- The VDD rail rises in some 100 µs, too slow for the timed key entry; plain entry is the
  documented mode for this part.
- The VPP pin level outside programming (0 V or VDD) is not known; both are tried.
