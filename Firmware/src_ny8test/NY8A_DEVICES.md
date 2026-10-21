# NY8A device list

Merged from Nyquest's tool databases (Q-Writer 6.60: `otp`, `mask`, `NY8_FW/HW_Otp_Download_Config`;
NYC_NY8 2.30: `ny8devices.txt`, `lib/properties`) with `tools/ny8_devlist.py`.
None of this is confirmed on silicon yet.

## Columns

- **Die ID (chipid1)** / **Revisions (rev:chipid0)**: the two ID words the writer checks.
  chipid1 differs for every part number. chipid0 holds a size class in the high byte
  (0x0D: 512 words, 0x0E: 1K, 0x0F: 2K, 0x11: 1.5K) and the silicon revision in the low byte.
  The newest parts (NY8A050E1, NY8A053E1) share chipid1 0x001F and use the chipid0 high byte
  (0x00 / 0x01) to tell them apart.
- **Cmd ID**: `command_id` from the chip database. Q-Writer uses it for other writer families;
  the NY8 writer does not receive it. Its meaning for NY8 is not known.
- **Entry**: `WRITER_Enter_Mode` (programming-mode entry variant).
- **VPP code**: `Program_High_Voltage_setting`, probably tenths of a volt (0x46 = 7.0 V).
- **CP type**: `MTP_CP_Type`, layout of the factory (chip-probe) data in the config words.
- **Trim cmd bit**, **IHRC trim**: oscillator trim command variant and location of the trim
  bits in the 16-word config block.
- **LGOTO words**: `jump_size` from the SDCC device file (1 = single-word LGOTO/LCALL).
- **SDCC model**: memory model of the NYC_NY8 SDCC port.
- **Profile**: option/config-word layout family (`mask` database). Parts with the same
  profile share the config-word layout.
- **ICE image**: Q-Writer contains an emulator FPGA image for this part.

## Table

| Body | ROM words | RAM B | Die ID (chipid1) | Revisions (rev:chipid0) | Cmd ID | Entry | VPP code | CP type | Trim cmd bit | IHRC trim | LGOTO words | SDCC model | Profile | Notes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| NY8A050D | 512 | 32 | 0x0010, 0x0011 | Z:0x0D00~0x0D05, A:0x0D00, B:0x0D01 | 0x8F01 | 3 | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small.19897 | ny8a050d | bug: NY8_DAA; ICE image |
| NY8A050E | 512 | 32 | 0x0017 | A:0x0D00, B:0x0D01 | 0x8F01 | 0 | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a050e | ICE image |
| NY8A050E1 | 512 | 32 | 0x001F | A:0x0000, B:0x0001 | 0x8F01 | 0 | 0x46 | 0x83 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a050e |  |
| NY8A051A | 1024 | 48 | 0x0000 | A:0x0E00, B:0x0E01 | 0x8001 | 4 | 0x45 | 0x81 | 2 | 0x0D[6:0] |  |  | ny8a051a | bug: NY8_DAA |
| NY8A051B | 1024 | 48 | 0x0007 | B:0x0E01, C:0x0E02, D:0x0C02, E:0x0E04, F:0x0E05 | 0x8001 | 0 | 0x45 | 0x81 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051b | bug: NY8_DAA |
| NY8A051C | 1024 | 48 | 0x0005 | A:0x0E00, C:0x0E02, D:0x0E03 | 0x8001 | 0 | 0x45 | 0x81 | 2 | 0x0D[6:0] |  |  | ny8a051b | bug: NY8_DAA |
| NY8A051D | 1024 | 48 | 0x0009 | A:0x0E00, B:0x0E01, C:0x0E03, D:0x0E06, E:0x0E04 | 0x8001 | 0 | 0x47 | 0x81 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051b | bug: NY8_DAA |
| NY8A051E | 1024 | 48 | 0x000D | B:0x0E01 | 0x8101 | 2 | 0x47 | 0x81 | 2 | 0x0D[6:0] |  |  | ny8a051e | bug: NY8_DAA |
| NY8A051F | 1024 | 48 | 0x000F | A:0x0E00, B:0x0E01, C:0x0E03 | 0x8F01 | 2 | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small.19897 | ny8a051f | bug: NY8_DAA |
| NY8A051G | 1024 | 48 | 0x0012 | A:0x0E00 | 0x8F01 | 2 | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small.19897 | ny8a051g | bug: NY8_DAA |
| NY8A051H | 1024 | 48 | 0x0016 | C:0x0E02, D:0x0E03 | 0x8F01 | 0 | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051h |  |
| NY8A051H1 | 1024 | 48 | 0x001B | C:0x0E01, D:0x0E02 | 0x8F01 | 0 | 0x46 | 0x82 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051h | ICE image |
| NY8A051J | 1024 | 48 | 0x001C, 0x021C | B:0x0E01, C:0x0E03, C:0x0E03 | 0x8F01 | 0 | 0x46 | 0x82 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051j | ICE image |
| NY8A051K | 1024 | 48 | 0x001A | B:0x0E01, C:0x0E03 | 0x8F01 | 0 | 0x46 | 0x82 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051k | ICE image |
| NY8A051L | 1024 | 48 | 0x001D | B:0x0E00, D:0x0E02 | 0x8F01 | 0 | 0x46 | 0x83 | 2 | 0x0D[6:0] | 1 | small.v2 | ny8a051l | ICE image |
| NY8A052E | 1536 | 96 | 0x0018 | B:0x1101, C:0x1102 | 0x8F01 | 0xD | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small_type5 | ny8a052e | ICE image |
| NY8A053A | 1024 | 64 | 0x0001 | B:0x0E01, C:0x0E02 | 0x8001 | 4 | 0x45 | 0x81 | 2 | 0x0D[6:0] |  |  | ny8a053a | bug: NY8_DAA |
| NY8A053B | 1024 | 64 | 0x0008 | A:0x0E00, B:0x0E01, C:0x0E02, D:0x0E03, E:0x0E06, F:0x0E07 | 0x8001 | 0 | 0x45 | 0x81 | 2 | 0x0D[6:0] | 1 | small | ny8a053d | bug: NY8_DAA |
| NY8A053D | 1024 | 64 | 0x000A | A:0x0E00, B:0x0E01, C:0x0E03 | 0x8001 | 0 | 0x47 | 0x81 | 2 | 0x0D[6:0] | 1 | small | ny8a053d | bug: NY8_DAA |
| NY8A053E | 1024 | 64 | 0x0015 | B:0x0E01, C:0x0E02, D:0x0E03 | 0x8F01 | 0xD | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small_type3 | ny8a053e | ICE image |
| NY8A053E1 | 1024 | 96 | 0x001F | A:0x0100, B:0x0101, C:0x0103 | 0x8F01 | 0xD | 0x46 | 0x83 | 2 | 0x0D[6:0] | 1 | small_type5 | ny8a053e1 | ICE image |
| NY8A054A | 2048 | 128 | 0x000B | A:0x0F00, B:0x0F01 | 0x8201 | 0xE | 0x47 | 0x81 | 2 | 0x0D[6:0] | 1 | small_8b | ny8a054a | bug: NY8_DAA |
| NY8A054D | 2048 | 128 | 0x000C | A:0x0F00 | 0x8301 | 0xE | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small_8b | ny8a054d | bug: NY8_DAA |
| NY8A054E | 2048 | 128 | 0x0014 | C:0x0F02 | 0x8F01 | 0xD | 0x47 | 0x82 | 2 | 0x0D[6:0] | 1 | small_8b | ny8a054d | ICE image |
| NY8A054E1 | 2048 | 128 | 0x001E | B:0x0F01, C:0x0F03 | 0x8F01 | 0xD | 0x46 | 0x83 | 2 | 0x0D[6:0] | 1 | small_8b | ny8a054e1 | ICE image |
| NY8A056A | 1024 | 96 | 0x0002 | A:0x0E01, B:0x0E02, D:0x0E03, F:0x0E04 | 0x8001 | 0 | 0x45 | 0x81 | 2 | 0x0D[6:0] | 1 | small | ny8a056a | bug: NY8_DAA |
| NY8AE51D | 1024 | 64 | 0x0303 | A:0x0E00, B:0x0E01 | 0x8F01 | 6 | 0x45 | 1 | 2 | 0x0D[6:0] |  |  | ny8ae51d | MTP; bug: NY8_DAA |
| NY8AE51F | 1024 | 64 | 0x030D | B:0x0E01, C:0x0E02 | 0x8F01 | 6 | 0x45 | 1 | 2 | 0x0D[6:0] | 1 | small_8b | ny8ae51d | MTP; ICE image |

NY8A053E1: the chip database gives 2k bytes (1024 words), the SDCC device file 1536 words.

## Observations

**Dies.** Every part number has its own chipid1, including pairs that look like minor
updates (NY8A051H 0x0016 / NY8A051H1 0x001B, NY8A050E 0x0017 / NY8A050E1 0x001F). The
factory config words contain a `VIRTUAL_BODY` field, though, so part of the ID can be
programmed at wafer test and one die could be sold under several part numbers. Distinct
IDs are therefore not proof of distinct dies; die photos or a read of the ID words settle it.
The "Profile" column and Q-Writer's compatibility list show the derivative relations:

| Part | Accepts programs built for | Shares profile with |
|---|---|---|
| NY8A050E1 | NY8A050E | NY8A050E |
| NY8A051H1 | NY8A051H | NY8A051H |
| NY8A051L | NY8A051G | – |
| NY8A053E1 | NY8A052E, NY8A053E | – |
| NY8A051C | – | NY8A051B |
| NY8A053B | – | NY8A053D |
| NY8A054E | – | NY8A054D |
| NY8AE51F | – | NY8AE51D |

**Instruction set.** All NY8A parts are 14-bit, single-word LGOTO/LCALL (`jump_size 1`),
one core family in the SDCC port. Differences:
- Older parts carry the bug flag `NY8_DAA` (DAA instruction defect); the compiler works around it.
- RAM: 32–128 B. The 2K-word NY8A054x use the FSR[7:6] bank bits (128 B). For some parts the
  two sources disagree (NY8A056A: 96 B in the chip database, 48 B in the SDCC device file).
- The 2K-word NY8A054x need 11-bit LGOTO/LCALL addresses. In the opcode map
  (NY8_ISA.md) the unused ranges 0x2C00–0x2FFF and 0x3C00–0x3FFF fit an 11-bit field exactly;
  this is inferred, not tested.
- Nyquest's LLVM target has only `ny8a051j` from this family; SDCC covers all.

**Programming generations.** The writer parameters group the parts into generations:

| Group | Parts | Cmd ID | CP type | VPP code | Entry |
|---|---|---|---|---|---|
| early | NY8A051A–D, NY8A053A/B/D, NY8A056A | 0x8001 | 0x81 | 0x45 (051D, 053D: 0x47) | 0 (051A, 053A: 4) |
| early, variants | NY8A051E / NY8A054A / NY8A054D | 0x8101 / 0x8201 / 0x8301 | 0x81 / 0x81 / 0x82 | 0x47 | 2 / 0xE / 0xE |
| middle | NY8A050D/E, NY8A051F/G/H, NY8A052E, NY8A053E, NY8A054E | 0x8F01 | 0x82 | 0x47 | 0, 2, 3, 0xD |
| late | NY8A050E1, NY8A051H1/J/K/L, NY8A053E1, NY8A054E1 | 0x8F01 | 0x82/0x83 | 0x46 | 0 or 0xD |
| MTP | NY8AE51D/F | 0x8F01 | 1 | 0x45 | 6 |

All NY8A OTP parts use the same NY8-OTP_Writer firmware except NY8A054E/E1 (own firmware
variant) and the MTP parts. The trim command bit (2) and the IHRC trim location
(config word 0x0D[6:0]) are the same for all. Entry modes 1 and 3 add a 0x00 byte after each
command in the writer firmware; the meaning of 0xD/0xE and 2/4/6 is not decoded yet.
