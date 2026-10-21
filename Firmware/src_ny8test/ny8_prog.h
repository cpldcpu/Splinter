#ifndef NY8_PROG_H
#define NY8_PROG_H

#include <stdint.h>

// Nyquest NY8 (first generation, 14-bit OTP) programming interface.
//
// Protocol as reconstructed from the NY8-OTP_Writer (Ver. B) firmware
// (C8051F34x, ny8_otp_writer_b_firmware in Q-Writer 6.60). Not verified on silicon yet.
//
// Lines (target view):  SCK = PB2 (clock in), SDI = PB0 (data in), SDO = PB1 (data out),
//                       VPP = PB3 (programming voltage)
// Commands are 8 bits MSB first; address/data travel in 16-bit frames
//     [d13..d7, 0, d6..d0, F]      F = 1 for program data frames
// The writer changes SDI while SCK is high and samples SDO after the falling edge.

typedef enum {
	NY8_ENTRY_PLAIN = 0,   // power up with SCK/SDI low, then SDO held low ~12 ms (writer routine 5db3)
	NY8_ENTRY_KEY   = 1,   // power-cycle VDD, short wait, key bytes 0x53 0xAD (writer routine 12aa)
} ny8_entry_t;

#define NY8_CMD_READ      0x60   // read burst
#define NY8_CMD_READ_ALT  0x40   // second read command (purpose not identified)
#define NY8_CMD_PROG      0x70   // program single word (address follows)
#define NY8_CMD_PROG_SEQ  0x50   // program with address auto-increment (bulk)

typedef struct {
	uint32_t vdd_mv;         // target VDD during programming mode (writer: 3.0..5.0 V)
	uint32_t vpp_idle_mv;    // VPP pin level while not programming (0 or VDD; unknown which the writer uses)
	uint8_t  extra_zero_cmd; // send 0x00 after each command (writer modes 1 and 3)
} ny8_cfg_t;

void     ny8_io_init(void);
void     ny8_io_release(void);                 // all target lines high-Z

void     ny8_power_off(void);
uint8_t  ny8_enter(const ny8_cfg_t *cfg, ny8_entry_t mode, uint16_t key_delay_us);
void     ny8_leave(void);

void     ny8_send_cmd(const ny8_cfg_t *cfg, uint8_t cmd);
void     ny8_send_frame(uint16_t value14, uint8_t flag, uint16_t pulse_us);
uint16_t ny8_read_frame_raw(uint8_t last);

static inline uint16_t ny8_frame_encode(uint16_t v, uint8_t flag)
{
	return (uint16_t)(((v & 0x3F80u) << 2) | ((v & 0x007Fu) << 1) | (flag & 1u));
}
static inline uint16_t ny8_frame_decode(uint16_t raw)
{
	return (uint16_t)(((raw >> 2) & 0x3F80u) | ((raw >> 1) & 0x007Fu));
}

// Burst read: command + address frame + n reads. raw_out may be NULL.
void     ny8_read_words(const ny8_cfg_t *cfg, uint8_t cmd, uint16_t addr, uint16_t n,
                        uint16_t *words_out, uint16_t *raw_out);

// Program one word (writer routine 6c96): CMD 0x70, address, VPP on, wait,
// data frame whose last SCK-high phase is the programming pulse, VPP off, wait.
void     ny8_program_word(const ny8_cfg_t *cfg, uint16_t addr, uint16_t value14,
                          uint32_t vpp_mv, uint16_t pulse_us);

#endif
