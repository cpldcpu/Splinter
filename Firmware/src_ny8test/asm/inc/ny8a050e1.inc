; NY8A050E1 register definitions for nyasm.py
; Names follow the Nyquest C header include/ny8a050e1.h (NYC_NY8) and the datasheet V1.1.
;
; Memory map
;   ROM  0x000..0x1FF (512 words)
;        0x000 reset vector, 0x001 software interrupt (INT), 0x008 hardware interrupt
;        0x00E/0x00F factory rolling code, 0x1FE/0x1FF reserved: do not place code there
;   RAM  0x10..0x2F (32 bytes), R-page SFRs 0x00..0x0F

ROM_SIZE    EQU 512
RAM_START   EQU 0x10
RAM_END     EQU 0x2F

; destination operand (Nyquest NYASM spelling; A/R also work)
W           EQU 0
F           EQU 1

; ---- R-page ------------------------------------------------------------
INDF        EQU 0x00
TMR0        EQU 0x01
PCL         EQU 0x02
STATUS      EQU 0x03
FSR         EQU 0x04
PORTB       EQU 0x06
PCON        EQU 0x08
BWUCON      EQU 0x09
PCHBUF      EQU 0x0A
BPLCON      EQU 0x0B
BPHCON      EQU 0x0C
INTE        EQU 0x0E
INTF        EQU 0x0F

; STATUS bits
C           EQU 0
DC          EQU 1
Z           EQU 2
PD          EQU 3               ; /PD
TO          EQU 4               ; /TO

; PORTB bits
PB0         EQU 0               ; INT, programming SDI
PB1         EQU 1               ; programming SDO
PB2         EQU 2               ; EX_CKI, programming SCK
PB3         EQU 3               ; RSTb, programming VPP
PB4         EQU 4               ; instruction clock output (config)
PB5         EQU 5

; PCON bits
LVREN       EQU 3
EIS         EQU 6               ; 1: PB0 is the external interrupt input
WDTEN       EQU 7               ; 1 after reset; WDT runs if the WDT configuration word enables it

; BPLCON: pull-down enable, active low, PB0..PB3 in bits 4..7
PLPB0       EQU 4
PLPB1       EQU 5
PLPB2       EQU 6
PLPB3       EQU 7

; INTE / INTF bits. Clear a flag with MOVIA ~(1 << flag) / MOVAR INTF (writing 1 is
; assumed to leave a flag unchanged; not stated in the datasheet)
T0IE        EQU 0
PBIE        EQU 1
INTIE       EQU 2
T0IF        EQU 0
PBIF        EQU 1
INTIF       EQU 2

; ---- F-page (IOST / IOSTR) ----------------------------------------------
IOSTB       EQU 0x06            ; 1 = input, 0 = output (reset: all inputs)
PS0CV       EQU 0x0A            ; prescaler 0 counter, read only
BODCON      EQU 0x0C            ; 1 = open drain
PCON1       EQU 0x0F
T0EN        EQU 0               ; PCON1 bit 0: Timer0 enable (1 after reset)
GIE         EQU 7               ; PCON1 bit 7: set by ENI, cleared by DISI, read by IOSTR

; ---- S-page (SFUN / SFUNR) -----------------------------------------------
TBHP        EQU 0x07            ; ROM address bit 8 for TABLEA / CALLA / GOTOA
TBHD        EQU 0x08            ; TABLEA: bits 13..8 of the table word
OSCCR       EQU 0x0F
SELHOSC     EQU 0               ; OSCCR bits
STPHOSC     EQU 1
OPMD0       EQU 2               ; OPMD[1:0]: 00 normal, 01 halt, 10 standby
OPMD1       EQU 3

; ---- T0MD (T0MD / T0MDR) ------------------------------------------------
PS0SEL0     EQU 0               ; prescaler rate, Timer0: 000 = 1:2 ... 111 = 1:256
PS0WDT      EQU 3               ; 1: prescaler assigned to the WDT
T0CE        EQU 4               ; external clock edge (1: falling)
T0CS        EQU 5               ; 0: FINST, 1: EX_CKI / I_LRC
INTEDG      EQU 6               ; external interrupt edge (1: rising)
LCKTM0      EQU 7               ; with T0CS=1: 1 = I_LRC instead of EX_CKI
