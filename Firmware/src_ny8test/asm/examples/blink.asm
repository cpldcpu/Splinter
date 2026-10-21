; blink.asm - LED on PB2, toggles every BLINK_MS milliseconds (software delay)
;
; Wiring: LED + resistor (1 kOhm) from PB2 to GND.
;   NY8A050E1S6 (SOT23-6): PB2 = pin 4, VDD = pin 5, GND = pin 2.
; FINST is the instruction clock in Hz (see clockout.asm), default 1 MHz.

        INCLUDE "ny8a050e1.inc"

        IFNDEF FINST
FINST       EQU 1000000
        ENDIF
        IFNDEF BLINK_MS
BLINK_MS    EQU 250                     ; 1..256
        ENDIF
LED         EQU PB2

        ORG     0x000
        LGOTO   start

        ORG     0x010
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~(1 << LED) & 0x3F      ; LED pin output
        IOST    IOSTB

loop:   MOVIA   1 << LED
        XORAR   PORTB, F                ; toggle LED
        MOVIA   BLINK_MS & 0xFF
        LCALL   delay_ms
        LGOTO   loop

        INCLUDE "delay.inc"
