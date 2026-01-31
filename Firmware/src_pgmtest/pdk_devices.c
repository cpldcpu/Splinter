#include "pdk_devices.h"

const pdk_device_t PDK_DEV_PMS150C = {
	.name = "PMS150C",
	.scheme = PDK_SCHEME_OTP1_2,
	.id12_expected = 0xA16,
	.code_bits = 13,
	.addr_bits = 12,
	.code_words = 0x400,
	// CH32V003 GPIO levels track its VDD (~5V). Without level shifting, keep target VDD close to 5V.
	.vdd_cmd_mv = 5000,
	.vpp_cmd_mv = 8000,
	.vdd_erase_mv = 0,
	.vpp_erase_mv = 0,
	.erase_clocks = 0,
	.vdd_write_mv = 0,
	.vpp_write_mv = 0,
	.write_block_size = 0,
	.write_block_clock_groups = 0,
	.write_block_clocks_per_group = 0,
};

const pdk_device_t PDK_DEV_PFS154 = {
	.name = "PFS154",
	.scheme = PDK_SCHEME_FLASH_1,
	.id12_expected = 0xAA1,
	.code_bits = 14,
	.addr_bits = 13,
	.code_words = 0x800,
	// Keep target VDD close to 5V (see note above).
	.vdd_cmd_mv = 5000,
	.vpp_cmd_mv = 7500,
	.vdd_erase_mv = 5000,
	// PFS154 erase uses ~8V VPP in the sequence notes.
	.vpp_erase_mv = 8000,
	.erase_clocks = 2,
	.vdd_write_mv = 5800,
	// With VDD driven high (no level shifting), give a bit more VPP margin for reliable programming.
	.vpp_write_mv = 8000,
	.write_block_size = 4,
	.write_block_clock_groups = 1,
	.write_block_clocks_per_group = 8,
};

static const pdk_device_t *const _all[] = {
	&PDK_DEV_PMS150C,
	&PDK_DEV_PFS154,
};

const pdk_device_t *const *pdk_devices_all(uint32_t *count)
{
	if (count)
	{
		*count = (uint32_t)(sizeof(_all) / sizeof(_all[0]));
	}
	return _all;
}
