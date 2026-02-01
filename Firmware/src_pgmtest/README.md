# Padauk Programmer Test Firmware

This firmware is a test harness for programming an attached Padauk microcontroller.

It reuses the same power-control blocks (PWM + ADC measurement + VREF calibration) used by `src_boostertest/` so VDD/VPP/Boost voltages can be controlled and monitored.

Bring-up captures (terminal/scope screenshots) collected with this firmware live in `Design/README.md`.

Power model used in this test firmware:
- BOOST: regulated to a target when enabled (closed-loop using ADC)
- VDD/VPP: set directly from PWM using linear-amplifier calibration constants (open-loop)

Sequencing note: VDD/VPP are derived from BOOST. Keep VDD/VPP at 0V while enabling BOOST, then cycle VDD/VPP between the required levels for each programming step.

## Target Pin Mapping

- PC5 -> ICPCK (clock)
- PC6 -> MOSI (3-wire only)
- PC7 -> ICPDA / MISO (2-wire data or 3-wire MISO)

Some targets use a 3-wire protocol (separate MOSI/MISO). Others use a 2-wire protocol (single bidirectional data line on PC7).

## Programming Notes

See `PDK_PROGRAMMING.md` for a protocol summary and per-family notes extracted from the reference implementation in `Reference/`.

## ID Read Bringup

Source layout (for maintainability):
- `main.c` - top-level harness / printf output
- `power.c` / `power.h` - PWM + ADC + BOOST/VDD/VPP control
- `pdk_devices.c` / `pdk_devices.h` - device table (extend here)
- `pdk_prog.c` / `pdk_prog.h` - bit-banged PDK protocol helpers (ID/read/dump/erase/write)

To actually attempt an ID read, set `PGMTEST_ENABLE_POWER` to `1` in `main.c`.
Select the target device in `Firmware/src_pgmtest/main.c` by defining exactly one of:
- `PGMTEST_DEVICE_PFS154`
- `PGMTEST_DEVICE_PMS150C`

## Current Capabilities

- PMS150C: ID read + program dump (read-only for now)
- PFS154: ID read + program dump + erase + write

## Test Defines (main.c)

These are intentionally compile-time toggles for bringup:
- `PGMTEST_ENABLE_POWER` - actually energize BOOST/VDD/VPP and talk to the DUT
- `PGMTEST_ERASE_BEFORE_READ` - run erase (only if device supports it) before dumping
- `PGMTEST_WRITE_TEST` - write test pattern then verify then dump

Write test pattern:
- writes 16 words `0..15` starting at word address `0x0100`
- reads back and prints `VERIFY: OK/FAIL`

Dump format:
- program dump prints 16 words per line

## Electrical Note

The CH32V003 GPIO logic-high level tracks its own VDD. If there is no level shifting, low-target-VDD programming modes can be unreliable. For bring-up we keep target VDD close to ~5V and adjust VPP accordingly.
