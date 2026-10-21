; clockout.asm - square wave on PB2 at FINST / 6
;
; Use this first on a new chip: the instruction clock FINST depends on the
; configuration words (I_HRC 1..20 MHz, 2T or 4T), which the programmer does not
; write yet. Measure the frequency on PB2 and multiply by 6 to get FINST, then
; build the other examples with  make FINST=<value>.
;
; NY8A050E1S6 (SOT23-6): PB2 = pin 4.  SOP8: PB2 = pin 5.

        INCLUDE "ny8a050e1.inc"

        ORG     0x000                   ; reset vector
        LGOTO   start

        ORG     0x010                   ; behind the rolling code at 0x00E/0x00F
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~(1 << PB2) & 0x3F      ; PB2 output, others input
        IOST    IOSTB
        MOVIA   1 << PB2
loop:   XORAR   PORTB, F                ; 1 cycle: toggle PB2
        LGOTO   loop                    ; 2 cycles -> 3 cycles per half period
