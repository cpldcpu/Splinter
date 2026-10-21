; Software delay for nyasm.py, INCLUDE after the main code.
;
;   MOVIA  n
;   LCALL  delay_ms         ; waits n milliseconds (n = 0 -> 256 ms), destroys ACC
;
; Needs FINST (instruction clock in Hz). Uses RAM DLY_RAM..DLY_RAM+2
; (default 0x2D..0x2F, the top of the NY8A050E1 RAM).
;
; Cycles per millisecond: DLY_HI * (3 * DLY_LO + 4) + 4 (inner loop: DECRSZ 1 cycle,
; 2 on the last pass, + LGOTO 2 cycles). Call overhead: 6 cycles.
; Rounding error: below 1 % for FINST >= 400 kHz, about 2 % at 100 kHz.

        IFNDEF DLY_RAM
DLY_RAM     EQU 0x2D
        ENDIF
d_ms        EQU DLY_RAM
d_hi        EQU DLY_RAM + 1
d_lo        EQU DLY_RAM + 2

DLY_CYC     EQU FINST / 1000                    ; cycles per millisecond
DLY_HI      EQU (DLY_CYC + 760) / 761           ; outer count, inner loop <= 253
DLY_LO      EQU ((DLY_CYC - 4) / DLY_HI - 4) / 3   ; inner count
        IF DLY_LO < 1
        ERROR "FINST too low for delay_ms"
        ENDIF

delay_ms:
        MOVAR   d_ms
dly_1:  MOVIA   DLY_HI                          ; 1 ms
        MOVAR   d_hi
dly_2:  MOVIA   DLY_LO
        MOVAR   d_lo
dly_3:  DECRSZ  d_lo, F
        LGOTO   dly_3
        DECRSZ  d_hi, F
        LGOTO   dly_2
        DECRSZ  d_ms, F
        LGOTO   dly_1
        RET
