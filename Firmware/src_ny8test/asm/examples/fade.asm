; fade.asm - LED on PB2 fades up and down (software PWM, 256 steps)
;
; One PWM frame has 256 steps of 9 cycles (2304 cycles, about 430 Hz at FINST = 1 MHz).
; The LED is on while step < duty. After each frame duty moves by DSTEP; a full
; up/down cycle takes 2 * 256 / DSTEP frames (about 1 s at 1 MHz, DSTEP = 1).
;
; Wiring: LED + resistor from PB2 to GND (SOT23-6 pin 4).

        INCLUDE "ny8a050e1.inc"

        IFNDEF DSTEP
DSTEP       EQU 1
        ENDIF
LED         EQU PB2

; RAM
duty        EQU 0x10
step        EQU 0x11
flags       EQU 0x12                    ; bit 0: 1 = fading down

        ORG     0x000
        LGOTO   start

        ORG     0x010
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~(1 << LED) & 0x3F
        IOST    IOSTB
        CLRR    duty
        CLRR    flags

frame:  CLRR    step
pwm:    MOVR    duty, W
        SUBAR   step, W                 ; step - duty, C = 1 if step >= duty
        BTRSS   STATUS, C
        BSR     PORTB, LED              ; step < duty: on
        BTRSC   STATUS, C
        BCR     PORTB, LED              ; step >= duty: off
        INCRSZ  step, F
        LGOTO   pwm                     ; 9 cycles per step

        BTRSC   flags, 0
        LGOTO   down
up:     MOVIA   DSTEP
        ADDAR   duty, F                 ; C = 1 on overflow
        BTRSS   STATUS, C
        LGOTO   frame
        MOVIA   0xFF                    ; clamp, change direction
        MOVAR   duty
        BSR     flags, 0
        LGOTO   frame
down:   MOVIA   DSTEP
        SUBAR   duty, F                 ; duty - DSTEP, C = 0 on borrow
        BTRSC   STATUS, C
        LGOTO   frame
        CLRR    duty
        BCR     flags, 0
        LGOTO   frame
