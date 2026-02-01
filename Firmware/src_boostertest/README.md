# Booster Test Firmware

This firmware sets up the boost converter and voltage regulation PWM, sets the booster to a target voltage, and sweeps VPP/VDD.
Voltages are read back via ADC and sent to the monitor output.

Bring-up captures (plots and scope screenshots) collected with this firmware live in `Design/README.md`.

Key files:
- `main.c` - PWM/ADC control and measurement logic
- `Makefile` - build/flash entry point
- `funconfig.h` - ch32fun configuration
