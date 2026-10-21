; button.asm - push button on PB0 toggles the LED on PB2, chip sleeps in between
;
; PB0 has the internal pull-up enabled; the button connects PB0 to GND.
; The chip halts with SLEEP (all clocks off) and wakes on a level change of PB0.
; With GIE = 0 the program continues after SLEEP instead of jumping to 0x008.
; A press is accepted when PB0 is still low after DEBOUNCE_MS.
;
; Wiring (NY8A050E1S6): button pin 1 (PB0) to GND, LED + resistor pin 4 (PB2) to GND.

        INCLUDE "ny8a050e1.inc"

        IFNDEF FINST
FINST       EQU 1000000
        ENDIF
        IFNDEF DEBOUNCE_MS
DEBOUNCE_MS EQU 20
        ENDIF
LED         EQU PB2
BTN         EQU PB0

        ORG     0x000
        LGOTO   start

        ORG     0x010
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~(1 << LED) & 0x3F      ; LED output, PB0 input
        IOST    IOSTB
        BCR     BPHCON, BTN             ; pull-up on PB0 (active low enable)
        MOVIA   1 << BTN                ; wake-up only from PB0
        MOVAR   BWUCON
        MOVIA   1 << PBIE               ; PB change flag may wake the core
        MOVAR   INTE
        DISI                            ; no interrupt routine: continue after SLEEP

idle:   MOVR    PORTB, W                ; read PORTB: reference for change detection
        MOVIA   ~(1 << PBIF) & 0xFF
        MOVAR   INTF
        SLEEP                           ; halt until PB0 changes
        NOP

        MOVIA   DEBOUNCE_MS
        LCALL   delay_ms
        BTRSC   PORTB, BTN              ; still pressed (low)?
        LGOTO   idle                    ; no: release or bounce
        MOVIA   1 << LED
        XORAR   PORTB, F                ; toggle LED

release:
        BTRSS   PORTB, BTN              ; wait until released
        LGOTO   release
        MOVIA   DEBOUNCE_MS
        LCALL   delay_ms
        LGOTO   idle

        INCLUDE "delay.inc"
