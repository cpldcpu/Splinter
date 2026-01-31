#ifndef PDK_DEVICES_H
#define PDK_DEVICES_H

#include <stdint.h>

typedef enum pdk_scheme
{
	PDK_SCHEME_FLASH_1 = 1, // 2-wire: ICPCK + bidirectional ICPDA
	PDK_SCHEME_OTP1_2  = 2, // 3-wire: ICPCK + MOSI + ICPDA/MISO
} pdk_scheme_t;

typedef struct pdk_device
{
	const char *name;
	pdk_scheme_t scheme;

	// Expected 12-bit device ID (as used by the reference tool). 0 if unknown.
	uint16_t id12_expected;

	// Code word width in bits. Used for ID probe bit-length on OTP schemes.
	// (Flash schemes ignore this.)
	uint8_t code_bits;

	// Program memory geometry (word addressed).
	uint8_t addr_bits;
	uint16_t code_words;

	// Voltages used to enter programming mode (command stage).
	uint32_t vdd_cmd_mv;
	uint32_t vpp_cmd_mv;

	// Optional erase parameters (flash devices only). If erase_clocks==0, erase is unsupported.
	uint32_t vdd_erase_mv;
	uint32_t vpp_erase_mv;
	uint8_t erase_clocks;

	// Optional write parameters. If write_block_size==0, write is unsupported.
	uint32_t vdd_write_mv;
	uint32_t vpp_write_mv;
	uint8_t write_block_size;
	uint8_t write_block_clock_groups;
	uint8_t write_block_clocks_per_group;
} pdk_device_t;

extern const pdk_device_t PDK_DEV_PMS150C;
extern const pdk_device_t PDK_DEV_PFS154;

const pdk_device_t *const *pdk_devices_all(uint32_t *count);

#endif // PDK_DEVICES_H
