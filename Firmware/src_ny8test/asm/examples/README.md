# NY8A050E1 assembler examples

Assembly programs for the NY8A050E1 (512 words OTP, 32 B RAM), assembled with
`../tools/nyasm.py`. They also run on the other NY8A05x parts with the same register map.

| Program | What it does | Shows |
|---|---|---|
| `clockout.asm` | Square wave on PB2 at FINST / 6 | measuring the instruction clock of a new chip |
| `blink.asm` | LED on PB2 toggles every 250 ms | port setup, `LCALL`, `delay.inc` |
| `timer_irq.asm` | LED on PB2 toggles every 500 ms from the Timer0 interrupt, main loop toggles PB1 | `T0MD`, interrupt vector 0x008, saving ACC/STATUS, clearing INTF |
| `button.asm` | Button on PB0 toggles the LED on PB2; chip halts between presses | pull-up, wake-up on pin change with `SLEEP`, debouncing |
| `fade.asm` | LED on PB2 fades up and down | software PWM, `SUBAR` carry |
| `morse.asm` | Sends "SOS NY8" in Morse code on PB2 | `TABLEA` table in ROM, `SFUN TBHP` |

## Wiring (NY8A050E1S6, SOT23-6)

| Pin | Function | Used for |
|---|---|---|
| 1 | PB0 / SDI | button to GND (`button.asm`) |
| 2 | VSS | GND |
| 3 | PB1 / SDO | second output (`timer_irq.asm`) |
| 4 | PB2 / SCK | LED with 1 kΩ to GND |
| 5 | VDD | supply, 1.6–5.5 V depending on the clock (datasheet) |
| 6 | PB3 / VPP | not used (programming voltage) |

For the SOP8 package: PB0 = pin 7, PB1 = pin 6, PB2 = pin 5, PB3 = pin 4, VDD = pin 1, VSS = pin 8.

## Build

```
make                          # build/<name>.hex  (simulator), .lst (listing), .h (programmer image)
make FINST=4000000            # rebuild for a different instruction clock
make image PROG=blink IMAGE=../../ny8_image.h
make dis-blink                # disassemble the image
make sim                      # simulate on the Verilog core and check the pin activity
```

`build/<name>.h` is the `{address, word}` table that `src_ny8test` (Splinter) programs
when `NY8TEST_WRITE_IMAGE` is 1. It lists only the words the program uses, so the
factory words at 0x00E/0x00F stay untouched.

## Instruction clock

Delays are computed from `FINST`, the instruction clock in Hz (default 1 MHz). FINST is set
by configuration words (I_HRC 1–20 MHz, divided by 2 or 4), and the programmer does not
write configuration words yet, so the clock of a freshly programmed part is not known.
Program `clockout.asm` first, measure the frequency on PB2 and build the others with
`make FINST=<6 × measured frequency>`.

All programs clear `PCON.WDTEN` at start, so a watchdog enabled by the configuration
words does not reset them.

## Program layout

```
        INCLUDE "ny8a050e1.inc"
        ORG     0x000           ; reset vector (0x001 is the INT vector)
        LGOTO   start
        ORG     0x008           ; hardware interrupt vector
        LGOTO   isr
        ORG     0x010           ; 0x00E/0x00F hold the factory rolling code
start:  ...
```

Use `LGOTO`/`LCALL`: they reach the whole ROM. `GOTO` covers 0x000–0x1FF on this part,
`CALL` only 0x000–0x0FF (bit 8 comes from `PCHBUF`). Tables read with `TABLEA` and
computed jumps through `PCL` must not cross a 256-word boundary.

## Simulation

`make sim` assembles each program with FINST = 100 kHz and shortened delays (see the
`SIMDEF_*` lines in the Makefile), runs it on the Verilog core (`../sim/tb_example.v`) and
checks the recorded pin edges with `../sim/check_example.py`:

| Program | Check |
|---|---|
| clockout | PB2 toggles every 3 cycles |
| blink | PB2 half period = BLINK_MS × FINST / 1000 within 3 % |
| timer_irq | PB2 half period = TICKS × Timer0 overflow period within 4 cycles; PB1 toggles |
| button | two LED toggles for the stimulus in `button.stim` (one press with contact bounce, one clean press, one glitch shorter than the debounce time); core halted more than half of the time |
| fade | PWM duty per frame runs from 0 to above 0.9 and back |
| morse | PB2 decodes to "SOS NY8 SOS ..." |

The simulation model is the NY8A051H1 core; for these programs it behaves like the
NY8A050E1 (same registers, the programs use RAM 0x10–0x2F only).
