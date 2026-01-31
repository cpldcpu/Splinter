# Firmware

This directory contains the firmware build environment and sources for the Splinter project.

- `src_boostertest/` - application source (CH32V003 + ch32fun)
- `src_pgmtest/` - Padauk programmer test harness (CH32V003 + ch32fun)
- `ch32fun/` - ch32fun framework and tools

## Build/Flash

Build:

```sh
cd Firmware/src_pgmtest
make build
```

Flash:

```sh
cd Firmware/src_pgmtest
make flash
```
