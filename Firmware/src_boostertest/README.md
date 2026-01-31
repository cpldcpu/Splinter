# Booster Test Firmware

This firmwasre will set up booster and voltage regulation PWM, set the booster to a target voltage and then sweep VPP and VDD.
Voltages are read back via ADC and sent to the monitor output.

Key files:
- `main.c` - PWM/ADC control and measurement logic
- `Makefile` - build/flash entry point
- `funconfig.h` - ch32fun configuration
