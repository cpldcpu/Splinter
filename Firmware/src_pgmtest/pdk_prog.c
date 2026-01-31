#include "pdk_prog.h"

#include "ch32fun.h"
#include "power.h"

#include <stdio.h>

// Target IO mapping (see repo README / schematic).
#define PDK_PIN_ICPCK 5 // PC5
#define PDK_PIN_MOSI  6 // PC6
#define PDK_PIN_DATA  7 // PC7 (ICPDA/MISO, or 2-wire ICPDA)

#define PDK_VPP_CMD_STABILIZE_US   400
#define PDK_VDD_CMD_STABILIZE_US   600
#define PDK_LEAVE_PROG_MODE_US   10000
#define PDK_VPP_EW_STABILIZE_US   5000
#define PDK_VDD_EW_STABILIZE_US  10000

// Conservative bit-bang timing (us).
#define PDK_CLK_HIGH_US 2
#define PDK_CLK_LOW_US  2

// Forward declarations (keeps file readable while allowing helpers above/below).
static void pdk_send_bits32_mosi(uint32_t data, uint8_t bits);
static void pdk_send_bits32_data(uint32_t data, uint8_t bits);
static uint64_t pdk_recv_bits_data(uint8_t bits);
static void pdk_enter_prog_mode(const pdk_device_t *dev);
static void pdk_leave_prog_mode(void);
static void pdk_leave_prog_mode_extrawait(uint32_t extra_wait_us);

static inline void gpioc_pin_cfg(uint8_t pin, uint8_t mode)
{
	GPIOC->CFGLR &= ~(0xFUL << (4 * pin));
	GPIOC->CFGLR |= ((uint32_t)mode) << (4 * pin);
}

static inline void pdk_set_clk(uint8_t level)
{
	if (level) GPIOC->BSHR = (1U << PDK_PIN_ICPCK);
	else GPIOC->BCR = (1U << PDK_PIN_ICPCK);
}

static inline void pdk_set_mosi(uint8_t level)
{
	if (level) GPIOC->BSHR = (1U << PDK_PIN_MOSI);
	else GPIOC->BCR = (1U << PDK_PIN_MOSI);
}

static inline void pdk_set_data(uint8_t level)
{
	if (level) GPIOC->BSHR = (1U << PDK_PIN_DATA);
	else GPIOC->BCR = (1U << PDK_PIN_DATA);
}

static inline uint8_t pdk_read_data(void)
{
	return (GPIOC->INDR >> PDK_PIN_DATA) & 1U;
}

static inline void pdk_data_input(void)
{
	// Match the reference implementation behavior (STM32 uses a pulldown on DAT).
	// This helps avoid reading a "ghost" pattern from crosstalk when the target is not driving.
	gpioc_pin_cfg(PDK_PIN_DATA, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	GPIOC->OUTDR &= ~(1U << PDK_PIN_DATA); // pulldown
}

static inline void pdk_data_output(void)
{
	gpioc_pin_cfg(PDK_PIN_DATA, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static inline void pdk_mosi_output(void)
{
	gpioc_pin_cfg(PDK_PIN_MOSI, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static inline void pdk_clock_output(void)
{
	gpioc_pin_cfg(PDK_PIN_ICPCK, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static inline void pdk_io_idle(void)
{
	// Idle in 3-wire-safe mode: clock + MOSI as outputs low, DATA as input.
	pdk_clock_output();
	pdk_mosi_output();
	pdk_data_input();
	GPIOC->BCR = (1U << PDK_PIN_ICPCK) | (1U << PDK_PIN_MOSI);
}

static inline void pdk_clock_pulse(void)
{
	pdk_set_clk(1);
	Delay_Us(PDK_CLK_HIGH_US);
	pdk_set_clk(0);
	Delay_Us(PDK_CLK_LOW_US);
}

static inline void pdk_send_bit_mosi(uint8_t bit)
{
	pdk_set_mosi(bit);
	pdk_clock_pulse();
}

static inline void pdk_send_bit_data(uint8_t bit)
{
	pdk_set_data(bit);
	pdk_clock_pulse();
}

static inline uint8_t pdk_recv_bit_data(void)
{
	pdk_set_clk(1);
	Delay_Us(PDK_CLK_HIGH_US);
	uint8_t bit = pdk_read_data();
	pdk_set_clk(0);
	Delay_Us(PDK_CLK_LOW_US);
	return bit;
}

static uint8_t pdk_read_begin(const pdk_device_t *dev)
{
	if (!dev)
	{
		return 0;
	}

	pdk_enter_prog_mode(dev);

	switch (dev->scheme)
	{
		case PDK_SCHEME_FLASH_1:
		{
			// Send READ command (0x6) and check we got some response.
			pdk_send_bits32_data(0xA5A5A5A0U | 0x6U, 32);

			pdk_data_input();
			uint32_t ack = (uint32_t)pdk_recv_bits_data(16);
			pdk_data_output();
			pdk_clock_pulse(); // extra clock

			// Reject an all-zero ACK (likely no target / not in programming mode).
			return ack != 0;
		}
		case PDK_SCHEME_OTP1_2:
			// Send READ command (0x6). No explicit ACK.
			pdk_send_bits32_mosi(0xA5A5A5A0U | 0x6U, 32);
			return 1;
		default:
			return 0;
	}
}

static void pdk_read_end(void)
{
	pdk_leave_prog_mode();
}

static uint8_t pdk_write_begin(const pdk_device_t *dev)
{
	if (!dev)
	{
		return 0;
	}

	pdk_enter_prog_mode(dev);

	switch (dev->scheme)
	{
		case PDK_SCHEME_FLASH_1:
		{
			// WRITE command is 0x7.
			pdk_send_bits32_data(0xA5A5A5A0U | 0x7U, 32);

			pdk_data_input();
			uint32_t ack = (uint32_t)pdk_recv_bits_data(16);
			pdk_data_output();
			pdk_clock_pulse(); // extra clock

			uint16_t id12 = (uint16_t)(ack & 0x0FFFU);
			if (dev->id12_expected && id12 != dev->id12_expected)
			{
				return 0;
			}

			return 1;
		}
		default:
			return 0;
	}
}

static void pdk_write_end(void)
{
	pdk_leave_prog_mode_extrawait(100000);
}

uint8_t pdk_erase(const pdk_device_t *dev)
{
	if (!dev || dev->erase_clocks == 0)
	{
		return 0;
	}

	// Only supported for FLASH_1 right now (PFS154).
	if (dev->scheme != PDK_SCHEME_FLASH_1)
	{
		return 0;
	}

	// Make sure rails are off before entering programming mode.
	power_set_vdd_mv(0);
	power_set_vpp_mv(0);
	Delay_Ms(5);

	pdk_enter_prog_mode(dev);

	// ERASE command is 0x3.
	pdk_send_bits32_data(0xA5A5A5A0U | 0x3U, 32);

	// Read 16-bit ACK; low 12 bits contain device ID.
	pdk_data_input();
	uint32_t ack = (uint32_t)pdk_recv_bits_data(16);
	pdk_data_output();
	pdk_clock_pulse(); // extra clock

	uint16_t id12 = (uint16_t)(ack & 0x0FFFU);
	if (dev->id12_expected && id12 != dev->id12_expected)
	{
		pdk_leave_prog_mode();
		return 0;
	}

	// For PFS154: after command, ramp VPP first, then VDD (see sequence notes).
	power_set_vpp_mv(dev->vpp_erase_mv);
	Delay_Us(PDK_VPP_EW_STABILIZE_US);
	power_set_vdd_mv(dev->vdd_erase_mv);
	Delay_Us(PDK_VDD_EW_STABILIZE_US);

	for (uint8_t e = 0; e < dev->erase_clocks; e++)
	{
		// Matches reference for FLASH_1: one long high pulse (~5ms) with a few short toggles.
		pdk_set_clk(1);
		Delay_Us(5000);
		pdk_set_clk(0);
		Delay_Us(1);
		pdk_set_clk(1);
		Delay_Us(1);
		pdk_set_clk(0);
		Delay_Us(4);
	}

	pdk_clock_pulse(); // 1 extra clock
	pdk_leave_prog_mode_extrawait(100000);
	return 1;
}

uint8_t pdk_write_words(const pdk_device_t *dev, uint16_t start_addr, const uint16_t *words, uint16_t count_words)
{
	if (!dev || !words || count_words == 0 || dev->write_block_size == 0)
	{
		return 0;
	}

	// Only supported for FLASH_1 right now (PFS154).
	if (dev->scheme != PDK_SCHEME_FLASH_1)
	{
		return 0;
	}

	if (dev->code_bits == 0 || dev->code_bits > 16)
	{
		return 0;
	}

	uint16_t blank_value = (dev->code_bits == 16) ? 0xFFFF : (uint16_t)((1U << dev->code_bits) - 1U);

	// Make sure rails are off before entering programming mode.
	power_set_vdd_mv(0);
	power_set_vpp_mv(0);
	Delay_Ms(5);

	if (!pdk_write_begin(dev))
	{
		pdk_leave_prog_mode();
		return 0;
	}

	// For PFS154: after sending the command, ramp VPP first, then VDD (see sequence notes).
	power_set_vpp_mv(dev->vpp_write_mv);
	Delay_Us(PDK_VPP_EW_STABILIZE_US);
	power_set_vdd_mv(dev->vdd_write_mv);
	Delay_Us(PDK_VDD_EW_STABILIZE_US);

	const uint8_t bs = dev->write_block_size;

	for (uint16_t i = 0; i < count_words; )
	{
		uint16_t addr = (uint16_t)(start_addr + i);
		uint16_t addr_aligned = (uint16_t)((addr / bs) * bs);

		// Build one aligned block buffer (fill with blank, then overlay our payload).
		uint16_t block[8];
		for (uint8_t k = 0; k < 8; k++) block[k] = blank_value;

		uint8_t offset = (uint8_t)(addr - addr_aligned);
		uint16_t remaining = (uint16_t)(count_words - i);
		uint8_t write_count = (remaining > (uint16_t)(bs - offset)) ? (uint8_t)(bs - offset) : (uint8_t)remaining;

		for (uint8_t w = 0; w < write_count; w++)
		{
			block[offset + w] = (uint16_t)(words[i + w] & blank_value);
		}

		// Send data words then address (reference order for FLASH_1).
		for (uint8_t w = 0; w < bs; w++)
		{
			pdk_send_bits32_data(block[w], dev->code_bits);
		}

		pdk_send_bits32_data(addr_aligned, dev->addr_bits);
		pdk_data_input();
		Delay_Us(4);

		for (uint8_t g = 0; g < dev->write_block_clock_groups; g++)
		{
			for (uint8_t p = 0; p < dev->write_block_clocks_per_group; p++)
			{
				pdk_set_clk(1);
				Delay_Us(15);
				pdk_set_clk(0);
				Delay_Us(15);
			}
			pdk_clock_pulse(); // 1 extra clock
			Delay_Us(4);
		}

		pdk_data_output();
		Delay_Us(25);
		Delay_Us(100); // small settle (matches reference intent)

		i += write_count;
	}

	pdk_write_end();
	return 1;
}

uint8_t pdk_read_word(const pdk_device_t *dev, uint16_t addr, uint16_t *out_word)
{
	if (!dev || !out_word)
	{
		return 0;
	}

	uint16_t data = 0;

	// One-shot read: enter mode, issue read command, read addr, leave mode.
	// This is slower but keeps sequencing explicit while bringing up the protocol.
	if (!pdk_read_begin(dev))
	{
		pdk_read_end();
		return 0;
	}

	switch (dev->scheme)
	{
		case PDK_SCHEME_FLASH_1:
		{
			pdk_data_output();
			pdk_send_bits32_data(addr, dev->addr_bits);

			// Reference holds CLK high briefly before sampling the first data bit.
			pdk_data_input();
			pdk_set_clk(1);
			Delay_Us(5);

			uint64_t bits = pdk_recv_bits_data(dev->code_bits);
			data = (uint16_t)(bits & 0xFFFFU);

			pdk_data_output();
			pdk_clock_pulse(); // extra clock
			break;
		}
		case PDK_SCHEME_OTP1_2:
		{
			pdk_send_bits32_mosi(addr, dev->addr_bits);
			uint64_t bits = pdk_recv_bits_data(dev->code_bits);
			data = (uint16_t)(bits & 0xFFFFU);
			break;
		}
		default:
			pdk_read_end();
			return 0;
	}

	pdk_read_end();

	if (dev->code_bits && dev->code_bits < 16)
	{
		data &= (uint16_t)((1U << dev->code_bits) - 1U);
	}

	*out_word = data;
	return 1;
}

void pdk_dump_program(const pdk_device_t *dev, uint16_t start_word, uint16_t count_words)
{
	if (!dev)
	{
		printf("PDK: dump: (null device)\n");
		return;
	}

	printf("PDK: dump: %s start=0x%X count=0x%X\n", dev->name, start_word, count_words);

	for (uint16_t i = 0; i < count_words; i++)
	{
		uint16_t addr = (uint16_t)(start_word + i);

		if ((i % 16) == 0)
		{
			printf("0x%04X:", (unsigned)addr);
		}

		uint16_t w = 0;
		if (!pdk_read_word(dev, addr, &w))
		{
			printf(" ----");
		}
		else
		{
			printf(" %04X", (unsigned)w);
		}

		if ((i % 16) == 15 || (i + 1) == count_words)
		{
			printf("\n");
		}
	}
}

static void pdk_send_bits32_mosi(uint32_t data, uint8_t bits)
{
	uint32_t v = data << (32 - bits);
	for (uint8_t i = 0; i < bits; i++)
	{
		pdk_send_bit_mosi((v & 0x80000000U) != 0);
		v <<= 1;
	}
	pdk_set_mosi(0);
}

static void pdk_send_bits32_data(uint32_t data, uint8_t bits)
{
	uint32_t v = data << (32 - bits);
	for (uint8_t i = 0; i < bits; i++)
	{
		pdk_send_bit_data((v & 0x80000000U) != 0);
		v <<= 1;
	}
	pdk_set_data(0);
}

static uint64_t pdk_recv_bits_data(uint8_t bits)
{
	uint64_t v = 0;
	for (uint8_t i = 0; i < bits; i++)
	{
		v = (v << 1) | (uint64_t)(pdk_recv_bit_data() ? 1U : 0U);
	}
	return v;
}

static void pdk_enter_prog_mode(const pdk_device_t *dev)
{
	// Rails: VPP first, then VDD (per reference implementation).
	power_set_vpp_mv(dev->vpp_cmd_mv);
	Delay_Us(PDK_VPP_CMD_STABILIZE_US);

	power_set_vdd_mv(dev->vdd_cmd_mv);
	Delay_Us(PDK_VDD_CMD_STABILIZE_US);

	// IO direction depends on scheme.
	pdk_clock_output();
	if (dev->scheme == PDK_SCHEME_FLASH_1)
	{
		// 2-wire: DATA is driven for command, then released for ACK.
		pdk_data_output();
	}
	else
	{
		// 3-wire: MOSI driven, DATA read.
		pdk_mosi_output();
		pdk_data_input();
	}
}

static void pdk_leave_prog_mode(void)
{
	pdk_io_idle();
	power_set_vdd_mv(0);
	power_set_vpp_mv(0);
	Delay_Us(PDK_LEAVE_PROG_MODE_US);
}

static void pdk_leave_prog_mode_extrawait(uint32_t extra_wait_us)
{
	pdk_io_idle();
	power_set_vdd_mv(0);
	power_set_vpp_mv(0);
	Delay_Us(PDK_LEAVE_PROG_MODE_US);
	if (extra_wait_us)
	{
		Delay_Us(extra_wait_us);
	}
}

uint16_t pdk_read_id12(const pdk_device_t *dev)
{
	if (!dev)
	{
		return 0;
	}

	// Make sure rails are off before entering programming mode.
	power_set_vdd_mv(0);
	power_set_vpp_mv(0);
	Delay_Ms(5);

	pdk_enter_prog_mode(dev);

	uint16_t id12 = 0;
	switch (dev->scheme)
	{
		case PDK_SCHEME_FLASH_1:
		{
			// Command 0x6 (READ) used for probing in reference.
			pdk_send_bits32_data(0xA5A5A5A0U | 0x6U, 32);

			// Read 16-bit ACK; low 12 bits contain device ID.
			pdk_data_input();
			uint32_t ack = (uint32_t)pdk_recv_bits_data(16);

			pdk_data_output();
			pdk_clock_pulse(); // extra clock (matches reference)
			id12 = (uint16_t)(ack & 0x0FFFU);
			break;
		}
		case PDK_SCHEME_OTP1_2:
		{
			// Command 0x7 (WRITE) used for probing in reference.
			pdk_send_bits32_mosi(0xA5A5A5A0U | 0x7U, 32);

			// Reference reads (codebits + codebits + 12) bits and masks the low 12 bits.
			// For PMS150C, that's 13+13+12 = 38 bits.
			uint8_t databits = dev->code_bits ? dev->code_bits : 16;
			uint64_t resp = pdk_recv_bits_data((uint8_t)(databits + databits + 12));
			id12 = (uint16_t)(resp & 0x0FFFU);
			break;
		}
		default:
			break;
	}

	pdk_leave_prog_mode();
	return id12;
}

void pdk_print_id(const pdk_device_t *dev)
{
	uint16_t id = pdk_read_id12(dev);
	if (!dev)
	{
		printf("PDK: (null device)\n");
		return;
	}

	if (id == 0)
	{
		printf("PDK: %s ID read failed\n", dev->name);
		return;
	}

	if (dev->id12_expected)
	{
		printf("PDK: %s ID=0x%03X (expected 0x%03X)\n", dev->name, id, dev->id12_expected);
	}
	else
	{
		printf("PDK: %s ID=0x%03X\n", dev->name, id);
	}
}

void pdk_io_init(void)
{
	// Ensure GPIOC clock is enabled (main may already do this).
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
	pdk_io_idle();
}
