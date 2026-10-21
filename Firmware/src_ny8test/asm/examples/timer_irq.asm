; timer_irq.asm - LED on PB2 toggled from the Timer0 overflow interrupt
;
; Timer0 counts FINST through the prescaler (1:16, 1:64 or 1:256, chosen by the
; assembler so that the overflow count fits 8 bits) and overflows every 256 counts.
; The interrupt routine counts overflows and toggles the LED every BLINK_MS
; milliseconds. The main loop toggles PB1 to show that it keeps running.
;
; Wiring: LED + resistor from PB2 to GND (SOT23-6 pin 4).
; Interrupt entry: 0x008. ACC and STATUS are saved by the routine
; (the core saves only the return address).

        INCLUDE "ny8a050e1.inc"

        IFNDEF FINST
FINST       EQU 1000000
        ENDIF
        IFNDEF BLINK_MS
BLINK_MS    EQU 500
        ENDIF
LED         EQU PB2
CYC_HALF    EQU BLINK_MS * (FINST / 1000)  ; cycles per LED half period
        IF CYC_HALF / 4096 < 255
PS0SEL_V    EQU 3                       ; prescaler 1:16
        ELSE
        IF CYC_HALF / 16384 < 255
PS0SEL_V    EQU 5                       ; 1:64
        ELSE
PS0SEL_V    EQU 7                       ; 1:256
        ENDIF
        ENDIF
TICK_CYC    EQU 256 << (PS0SEL_V + 1)   ; cycles per Timer0 overflow
TICKS       EQU (CYC_HALF + TICK_CYC / 2) / TICK_CYC
        IF TICKS < 1 or TICKS > 255
        ERROR "BLINK_MS gives a tick count outside 1..255"
        ENDIF

; RAM
acc_sv      EQU 0x10
st_sv       EQU 0x11
tick_cnt    EQU 0x12

        ORG     0x000
        LGOTO   start

        ORG     0x008                   ; hardware interrupt vector
        LGOTO   isr

        ORG     0x010
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~((1 << LED) | (1 << PB1)) & 0x3F
        IOST    IOSTB
        MOVIA   TICKS
        MOVAR   tick_cnt

        ; Timer0: clock FINST (T0CS = 0), prescaler on Timer0 (PS0WDT = 0)
        MOVIA   PS0SEL_V
        T0MD
        CLRR    TMR0
        MOVIA   ~(1 << T0IF) & 0xFF     ; clear a pending flag: write 0 to its bit
        MOVAR   INTF
        MOVIA   1 << T0IE
        MOVAR   INTE
        ENI

main:   MOVIA   1 << PB1                ; foreground work
        XORAR   PORTB, F
        LGOTO   main

; ---- interrupt routine ----
isr:    MOVAR   acc_sv                  ; save ACC
        SWAPR   STATUS, W               ; save STATUS without changing it
        MOVAR   st_sv

        MOVIA   ~(1 << T0IF) & 0xFF     ; acknowledge Timer0
        MOVAR   INTF
        DECRSZ  tick_cnt, F
        LGOTO   isr_end
        MOVIA   TICKS
        MOVAR   tick_cnt
        MOVIA   1 << LED
        XORAR   PORTB, F

isr_end:
        SWAPR   st_sv, W                ; restore STATUS
        MOVAR   STATUS
        SWAPR   acc_sv, F               ; restore ACC without touching Z
        SWAPR   acc_sv, W
        RETIE
