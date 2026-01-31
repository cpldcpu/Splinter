# Programming Padauk PDK Devices (Notes From `Reference/`)

This document summarizes how the Padauk (PDK) in-circuit programming protocols work, based on the reference implementation vendored in this repo:

- Host tool: `Reference/easypdkprog.c` (and `Reference/fpdkcom.*`, `Reference/fpdkicdata.*`, ...)
- Programmer firmware (STM32F0): `Reference/Src_firmware/fpdk.c`

It is intended as an implementation guide for bringing up PDK programming on this project (CH32V003-based programmer).

## Terminology / Signals

Power rails (project-specific):
- `BOOST` : upstream boosted rail (in this project, regulated to a target when enabled)
- `VDD`   : target MCU VDD (derived from BOOST via linear stage)
- `VPP`   : target MCU VPP (derived from BOOST via linear stage)

Programming IO (target-facing):
- `ICPCK` : programming clock
- `ICPDA` : programming data (bidirectional for 2-wire protocols; input-only for 3-wire protocols)
- `MOSI`  : programming data output (only used by 3-wire protocols)

Current mapping in this repo (see `Firmware/src_pgmtest/main.c`):
- `PC5 -> ICPCK`
- `PC6 -> MOSI` (3-wire only)
- `PC7 -> ICPDA/MISO` (2-wire data, or 3-wire MISO)

## Device Families / Programming Schemes

The reference implementation groups devices into "types" (`FPDKICTYPE`). These types are the *programming scheme*, not just memory type:

- `FPDK_IC_FLASH_1` : Flash, 2-wire (clock + bidirectional data), ACK is 16 bits
- `FPDK_IC_FLASH_2` : Flash, 2-wire (clock + bidirectional data), ACK is 17 bits, different write/erase pulse timings
- `FPDK_IC_OTP1_2`  : OTP, 3-wire (clock + MOSI + MISO), command preamble `0xA5A5A5A0|cmd`
- `FPDK_IC_OTP2_1`  : OTP, 3-wire, command preamble `0x5A5A5A50|cmd`, different probe response layout
- `FPDK_IC_OTP2_2`  : OTP, 3-wire, command preamble `0x5A5A5A50|cmd`, different probe response layout
- `FPDK_IC_OTP3_1`  : OTP, special scheme that needs extra lines (`CLK2` + `CMT`) in the reference firmware; write is not implemented there

### Devices Listed In `Reference/fpdkicdata.c`

The current vendored device table contains (grouped by scheme):

- `FPDK_IC_FLASH_1`: PFS154, PFS173
- `FPDK_IC_FLASH_2`: PFS172
- `FPDK_IC_OTP1_2` : PMS150C (PMS15A), PMS154B (PMS154C), MCU390
- `FPDK_IC_OTP2_1` : PMS133 (PMS134)
- `FPDK_IC_OTP2_2` : PMS131 (PMC131), PMS132 (PMS132B), PMS152, PMS171B
- `FPDK_IC_OTP3_1` : PMC251, PMS271 (PMC271)

Note: in `Reference/fpdkicdata.c`, some devices only have read voltages filled in (write/erase parameters are missing). Also, in `Reference/Src_firmware/fpdk.c`, OTP3 write is marked `//TODO` in `_FPDK_WriteAddr()`.

### Per-Device Parameters (As Encoded In The Table)

The table below is copied from the values in `Reference/fpdkicdata.c` (with commented-out fields ignored). Blank cells mean the parameter is not provided for that device in the vendored table.

`Read cmd` voltages are the rails used to enter programming mode. `Read HV` / `Write HV` / `Erase HV` are the rails used during the operation step after the command has been accepted.

| Device | Type | ID12 | Addr bits | Code bits | Code words | Read cmd VDD/VPP (V) | Read HV VDD/VPP (V) | Write HV VDD/VPP (V) | Write blk | Erase HV VDD/VPP (V) | Erase clocks |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| PFS154 | FPDK_IC_FLASH_1 | 0xAA1 | 13 | 14 | 0x800 | 2.5/5.5 | 2.5/5.5 | 5.8/8.5 | 4 | 3.0/9.0 | 2 |
| PFS173 | FPDK_IC_FLASH_1 | 0xEA2 | 13 | 15 | 0xC00 | 2.5/5.5 | 2.5/5.5 | 5.8/9.0 | 4 | 3.0/9.0 | 4 |
| PFS172 | FPDK_IC_FLASH_2 | 0xCA6 | 13 | 14 | 0x800 | 2.5/5.5 | 2.5/0.0 | 5.3/0.0 | 4 | 5.3/0.0 | 14 |
| MCU390 | FPDK_IC_OTP1_2 | 0xC31 | 12 | 14 | 0x800 | 3.0/5.0 | 3.0/5.0 |  |  |  |  |
| PMS150C (PMS15A) | FPDK_IC_OTP1_2 | 0xA16 | 12 | 13 | 0x400 | 3.0/5.0 | 3.0/5.0 | 5.8/10.5 | 2 |  |  |
| PMS154B (PMS154C) | FPDK_IC_OTP1_2 | 0xE06 | 12 | 14 | 0x800 | 3.0/5.0 | 3.0/5.0 | 5.8/11.0 | 2 |  |  |
| PMS133 (PMS134) | FPDK_IC_OTP2_1 | 0xC19 | 12 | 15 | 0x1000 | 3.0/5.0 | 3.0/5.0 |  |  |  |  |
| PMS131 (PMC131) | FPDK_IC_OTP2_2 | 0xC83 | 12 | 14 | 0x600 | 3.0/5.0 | 3.0/5.0 |  |  |  |  |
| PMS132 (PMS132B) | FPDK_IC_OTP2_2 | 0x109 | 12 | 14 | 0x800 | 3.0/5.0 | 3.0/5.0 |  |  |  |  |
| PMS152 | FPDK_IC_OTP2_2 | 0xA27 | 12 | 14 | 0x500 | 3.0/5.0 | 3.0/5.0 | 5.8/10.5 | 2 |  |  |
| PMS171B | FPDK_IC_OTP2_2 | 0xD36 | 12 | 14 | 0x600 | 3.0/5.0 | 3.0/5.0 | 5.8/10.5 | 2 |  |  |
| PMC251 | FPDK_IC_OTP3_1 | 0x058 | 13 | 16 | 0x400 | 2.5/5.0 | 2.5/5.5 |  |  |  |  |
| PMS271 (PMC271) | FPDK_IC_OTP3_1 | 0xA58 | 13 | 16 | 0x400 | 2.5/5.0 | 2.5/5.5 |  |  |  |  |


## Power/Mode Sequencing (Conceptual)

In the STM32 reference firmware, VDD/VPP are controlled directly; in this project VDD/VPP are derived from BOOST, so the high-level sequencing we want is:

1. Force `VDD=0V` and `VPP=0V`
2. Enable/regulate `BOOST` to its target (this project currently uses 18V)
3. Apply the per-command `VDD_cmd`/`VPP_cmd` to enter programming mode
4. For each operation step, switch `VDD`/`VPP` to the required levels (read HV, write HV, erase HV, etc)
5. When done, return `VDD=0V` and `VPP=0V`, then disable BOOST if desired

The reference firmware does "enter programming mode" as (see `_FPDK_EnterProgramingmMode()` in `Reference/Src_firmware/fpdk.c`):

1. Configure clock pin as output.
2. Set `VPP` to `VPP_cmd` and wait ~100us (`FPDK_VPP_CMD_STABELIZE_DELAYUS`).
3. Set `VDD` to `VDD_cmd` and wait ~500us (`FPDK_VDD_CMD_STABELIZE_DELAYUS`).
4. Configure IO directions depending on scheme:
   - Flash: data pin is output initially (then switched to input to read ACK).
   - OTP (non-flash): data pin is input, MOSI pin is output.
   - OTP3: also needs extra pins (`PA0` and `PA7` in the reference).

Leaving programming mode (`_FPDK_LeaveProgramingMode()`):
- Set all related pins back to inputs.
- Disable VDD then VPP (0V).
- Wait ~10ms before the next command (`FPDK_LEAVE_PROG_MODE_DELAYUS`).

## Bit-Level Protocol Summary

All schemes are MSB-first shifting, with a clocked bit per cycle.

### Command Preambles / ACK

Implemented by `_FPDK_SendCommand()` in `Reference/Src_firmware/fpdk.c`:

Flash (`FPDK_IC_FLASH_1`, `FPDK_IC_FLASH_2`):
- Send 32 bits: `0xA5A5A5A0 | cmd` on the (bidirectional) data line.
- Switch data line to input and read ACK:
  - FLASH_1: 16 bits
  - FLASH_2: 17 bits
- Switch data line back to output and send 1 extra clock.
- The low 12 bits of the ACK are treated as the device `id12bit` by the higher-level code.

OTP1 (`FPDK_IC_OTP1_2`):
- Send 32 bits: `0xA5A5A5A0 | cmd` on MOSI.
- No explicit ACK read at command time; probe/read/write flows read additional bits after the command.

OTP2 (`FPDK_IC_OTP2_1`, `FPDK_IC_OTP2_2`):
- Send 32 bits: `0x5A5A5A50 | cmd` on MOSI.
- No explicit ACK read at command time; probe/read/write flows read additional bits after the command.

OTP3 (`FPDK_IC_OTP3_1`):
- Send 32 bits `0x5A5A5A5A` then 27 zeros.
- Uses a second clock and a "commit" pin in the reference (`_FPDK_Clock2()`, `_FPDK_Commit2()`).
- Read is implemented; write is not implemented in the reference.

### Read Flow

Implemented by `FPDK_ReadIC()` + `_FPDK_ReadAddr()`:

1. Enter programming mode at `VDD_cmd`/`VPP_cmd`.
2. Send READ command:
   - FLASH_2 uses command `0xC`
   - all other types use command `0x6`
3. If required, switch rails to `VDD_read_hv`/`VPP_read_hv`.
4. For each word address:
   - Flash: send address on data line, switch to input, then clock out `codebits` bits.
   - OTP: send address on MOSI, then clock in `codebits` bits on `ICPDA/MISO`.
5. Leave programming mode.

### Write Flow

Implemented by `FPDK_WriteIC()` + `_FPDK_WriteAddr()`:

1. Enter programming mode at `VDD_cmd`/`VPP_cmd`.
2. Send WRITE command `0x7`.
3. Switch rails to `VDD_write_hv`/`VPP_write_hv` and wait longer (10ms in the reference: `FPDK_VDD_EW_STABELIZE_DELAYUS`, `FPDK_VPP_EW_STABELIZE_DELAYUS`).
4. Write is performed in blocks (see per-IC `write_block_size` etc in `Reference/fpdkicdata.c`):
   - Flash_1: send data words, then address; switch data to input; generate program pulses by clocking with ~15us high/low.
   - Flash_2: send address, then data; switch data to input; program pulses are ~40us high/low plus extra clocks.
   - OTP (non-flash): send data words then address; then keep clock high and pulse MOSI high/low with ~30us timing; repeat groups.
   - OTP3: not implemented in the reference.
5. Leave programming mode and wait longer (100ms in the reference after write: `_FPDK_LeaveProgramingMode(..., 100000)`).

### Erase Flow (Flash Only)

Implemented by `FPDK_EraseIC()`:

- Flash erase command:
  - FLASH_1: `0x3`
  - FLASH_2: `0x5`
- Switch to erase HV rails.
- Generate `erase_clocks` erase pulses:
  - FLASH_1 holds clock high ~5ms per pulse
  - FLASH_2 holds clock high ~40ms per pulse
- Leave programming mode and wait longer (~100ms).

## Memory Layout Notes (Fuses / Protected Areas)

The host tool (`Reference/easypdkprog.c`) treats:
- Code memory as an array of `codewords` words, each `codebits` wide (stored in 16-bit containers).
- Fuse word as the last word: `fuseaddr = icdata->codewords - 1`.

Some devices have factory/OTP regions that should not be overwritten or verified byte-for-byte. The device table provides:
- `exclude_code_first_instr`
- `exclude_code_start` / `exclude_code_end`

Example from the table:
- PFS154: excludes `0x7E0..0x7F0` (OTP area / factory values) from normal write/verify.

## Implementation Notes For This Repo

This repo’s programmer hardware intentionally supports both:
- 2-wire: `ICPCK` + bidirectional `ICPDA` (Flash-style)
- 3-wire: `ICPCK` + `MOSI` + `ICPDA/MISO` (OTP-style)

### I/O Level Note (CH32V003)

The CH32V003 GPIO logic levels track its supply voltage. If the CH32V003 side is running at ~5V and there is no level shifting, programming at low target VDD (e.g. 2.5V or 3.0V schemes) can be unreliable because the target may not tolerate/recognize the programmer’s logic-high level.

Practical implication: for initial bring-up, keep target `VDD` close to 5V (and adjust the programming sequence/voltages accordingly), or add proper level shifting for full coverage of low-VDD programming modes.

If you need to support `FPDK_IC_OTP3_1` devices, the reference implementation indicates you also need:
- a second clock (`CLK2`)
- a "commit" pin (`CMT`)

Currently, `Firmware/src_pgmtest/main.c` only allocates PC5/PC6/PC7 for target IO, so OTP3 would require assigning extra pins (or a different strategy).
