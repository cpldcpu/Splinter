; morse.asm - sends a message in Morse code on the LED (PB2), message stored in ROM
;
; Shows TABLEA: the table word at {TBHP, ACC} is read into ACC (bits 7..0)
; and TBHD (bits 13..8). Each table word holds one letter: the Morse symbols
; left aligned (1 = dash, 0 = dot) followed by a 1 as end marker,
; e.g. S = ... = 000 1 0000 = 0x10. 0x00 = word gap, 0xFF = end of message.
;
; Timing: dot = UNIT_MS, dash = 3 units, gap between symbols 1 unit,
; between letters 3 units, between words 7 units.

        INCLUDE "ny8a050e1.inc"

        IFNDEF FINST
FINST       EQU 1000000
        ENDIF
        IFNDEF UNIT_MS
UNIT_MS     EQU 100
        ENDIF
LED         EQU PB2

; letter codes
M_E EQU 0x40
M_T EQU 0xC0
M_S EQU 0x10
M_O EQU 0xF0
M_N EQU 0xA0
M_Y EQU 0xB8
M_8 EQU 0xE4                            ; ---.. 1 -> 11100 1 00
WORD EQU 0x00
STOP EQU 0xFF

; RAM
idx         EQU 0x10
code        EQU 0x11
units       EQU 0x12

        ORG     0x000
        LGOTO   start

        ORG     0x010
start:  BCR     PCON, WDTEN             ; watchdog off
        CLRR    PORTB
        MOVIA   ~(1 << LED) & 0x3F
        IOST    IOSTB

restart:
        CLRR    idx
next:   MOVIA   high(message)           ; TBHP = ROM address bit 8
        SFUN    TBHP
        MOVR    idx, W
        ADDIA   low(message)            ; message must not cross a 256-word page
        TABLEA                          ; ACC = low byte of the table word
        MOVAR   code
        INCR    idx, F
        XORIA   STOP
        BTRSC   STATUS, Z
        LGOTO   restart
        MOVR    code, W
        BTRSC   STATUS, Z
        LGOTO   word_gap

symbol: MOVR    code, W
        XORIA   0x80                    ; only the end marker left?
        BTRSC   STATUS, Z
        LGOTO   letter_end
        BSR     PORTB, LED
        MOVIA   1                       ; dot: 1 unit
        BTRSC   code, 7
        MOVIA   3                       ; dash: 3 units
        LCALL   wait_units
        BCR     PORTB, LED
        MOVIA   1                       ; gap between symbols
        LCALL   wait_units
        BCR     STATUS, C
        RLR     code, F                 ; next symbol
        LGOTO   symbol

letter_end:
        MOVIA   2                       ; 1 + 2 = 3 units between letters
        LCALL   wait_units
        LGOTO   next
word_gap:
        MOVIA   4                       ; 3 + 4 = 7 units between words
        LCALL   wait_units
        LGOTO   next

; wait ACC units
wait_units:
        MOVAR   units
wu_1:   MOVIA   UNIT_MS
        LCALL   delay_ms
        DECRSZ  units, F
        LGOTO   wu_1
        RET

        INCLUDE "delay.inc"

        ORG     0x100
message:
        DW      M_S, M_O, M_S, WORD
        DW      M_N, M_Y, M_8, WORD
        DW      STOP
