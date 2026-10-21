// NY8A050E1 bring-up harness for Splinter R0.
//
// Step 1 (default, read only, VPP never above the idle level):
//   try the entry variants and read the start of ROM (incl. the factory rolling
//   code at 0x00E/0x00F) and the area behind the 512-word ROM.
// Step 2 (NY8TEST_PROGRAM=1): program one word with a VPP sweep and read it back.
// Step 3 (NY8TEST_WRITE_IMAGE=1): program ny8_image.h (generated from example/) and verify.

#include "ch32fun.h"
#include <stdio.h>

#include "ny8_prog.h"
#include "power.h"

#define NY8TEST_PROGRAM        0        // 1 = run the single-word VPP sweep (consumes one ROM word)
#define NY8TEST_WRITE_IMAGE    0        // 1 = program ny8_image.h (see example/) and verify
#define NY8TEST_VPP_PROG_MV    7000     // VPP for image programming: set from the sweep result
#define NY8_IMAGE_XOR          0x3F00   // toolchain word -> OTP word (see NY8_ISA.md)
#define NY8TEST_PROG_ADDR      0x1F0    // unused word near the end of the 512-word ROM
#define NY8TEST_PROG_VALUE     0x3FFE   // clears only bit 0
#define NY8TEST_VPP_START_MV   6000
#define NY8TEST_VPP_END_MV     9000     // MCU_Writer spec: VPP 6.0..10.0 V; Q-Writer setting 0x46
#define NY8TEST_VPP_STEP_MV    250      // note: power.c resolution is ~0.53 V per PWM step
#define NY8TEST_PULSE_US       90       // writer: 90 us SCK-high on the last data bit

#define VDD_MV 5000

#if NY8TEST_WRITE_IMAGE
#include "ny8_image.h"
#endif

static void print_words(const char *tag, uint16_t base, const uint16_t *raw, uint16_t n)
{
	for (uint16_t i = 0; i < n; i++)
	{
		if ((i & 7) == 0) printf("\n%s %03X:", tag, base + i);
		printf(" %04X(%04X)", ny8_frame_decode(raw[i]), raw[i]);
	}
	printf("\n");
}

static uint8_t looks_alive(const uint16_t *raw, uint16_t n)
{
	// 0xFFFF everywhere = SDO pulled up, nobody driving; 0x0000 = held low
	uint8_t ones = 1, zeros = 1;
	for (uint16_t i = 0; i < n; i++)
	{
		if (raw[i] != 0xFFFF) ones = 0;
		if (raw[i] != 0x0000) zeros = 0;
	}
	return !(ones || zeros);
}

static uint8_t try_read(const ny8_cfg_t *cfg, ny8_entry_t mode, uint16_t key_us, const char *name)
{
	uint16_t raw[24];
	printf("\n== entry %s, vpp_idle=%lumV, extra_zero=%u\n", name,
		(unsigned long)cfg->vpp_idle_mv, cfg->extra_zero_cmd);
	ny8_enter(cfg, mode, key_us);
	printf("rails: VDD=%lumV VPP=%lumV\n",
		(unsigned long)power_meas_vdd_mv(), (unsigned long)power_meas_vpp_mv());
	ny8_read_words(cfg, NY8_CMD_READ, 0x000, 24, 0, raw);
	ny8_leave();
	print_words("rom", 0x000, raw, 24);
	uint8_t ok = looks_alive(raw, 24);
	printf("-> %s\n", ok ? "data varies (candidate)" : "flat (no response)");
	return ok;
}

int main(void)
{
	SystemInit();
	printf("src_ny8test: start\n");

	uint16_t vref_raw = 0;
	uint32_t vref_vdd = power_init(&vref_raw);
	printf("VREF: VDD=%lumV (raw %u)\n", (unsigned long)vref_vdd, vref_raw);
	ny8_io_init();
	power_pgm_begin();
	printf("BOOST=%lumV\n", (unsigned long)power_meas_boost_mv());

	ny8_cfg_t cfg = { .vdd_mv = VDD_MV, .vpp_idle_mv = 0, .extra_zero_cmd = 0 };

	// ---- step 1: find a working entry variant (read only) ----
	uint8_t found = 0;
	static const uint32_t idle_levels[2] = { 0, VDD_MV };
	for (uint8_t z = 0; z < 2 && !found; z++)
	{
		cfg.extra_zero_cmd = z;
		for (uint8_t v = 0; v < 2 && !found; v++)
		{
			cfg.vpp_idle_mv = idle_levels[v];
			found = try_read(&cfg, NY8_ENTRY_PLAIN, 0, "PLAIN");
		}
	}
	if (!found)
	{
		static const uint16_t key_delays[] = { 90, 60, 50, 40, 30, 120, 150, 500, 1000 };
		cfg.extra_zero_cmd = 0;
		cfg.vpp_idle_mv = 0;
		for (uint8_t k = 0; k < sizeof(key_delays) / sizeof(key_delays[0]) && !found; k++)
		{
			printf("(key delay %u us)", key_delays[k]);
			found = try_read(&cfg, NY8_ENTRY_KEY, key_delays[k], "KEY");
		}
	}
	if (!found)
	{
		printf("\nNo response. Check wiring/adapter, then look at SCK/SDI/SDO with a scope.\n");
		for (;;) Delay_Ms(1000);
	}

	// ---- full dump with the working variant ----
	{
		static uint16_t raw[64];
		printf("\n== dump with working variant\n");
		ny8_enter(&cfg, NY8_ENTRY_PLAIN, 0);
		for (uint16_t base = 0; base < 0x220; base += 64)
		{
			ny8_read_words(&cfg, NY8_CMD_READ, base, 64, 0, raw);
			print_words("rom", base, raw, 64);
		}
		// same start address with the second read command, for comparison
		ny8_read_words(&cfg, NY8_CMD_READ_ALT, 0x000, 16, 0, raw);
		print_words("alt", 0x000, raw, 16);
		ny8_read_words(&cfg, NY8_CMD_READ_ALT, 0x200, 16, 0, raw);
		print_words("alt", 0x200, raw, 16);
		ny8_leave();
	}

#if NY8TEST_PROGRAM
	// ---- step 2: single-word VPP sweep ----
	printf("\n== program 0x%03X <- 0x%04X, pulse %u us\n", NY8TEST_PROG_ADDR, NY8TEST_PROG_VALUE, NY8TEST_PULSE_US);
	for (uint32_t vpp = NY8TEST_VPP_START_MV; vpp <= NY8TEST_VPP_END_MV; vpp += NY8TEST_VPP_STEP_MV)
	{
		uint16_t before, after, raw;
		ny8_enter(&cfg, NY8_ENTRY_PLAIN, 0);
		ny8_read_words(&cfg, NY8_CMD_READ, NY8TEST_PROG_ADDR, 1, &before, &raw);
		ny8_program_word(&cfg, NY8TEST_PROG_ADDR, NY8TEST_PROG_VALUE, vpp, NY8TEST_PULSE_US);
		ny8_read_words(&cfg, NY8_CMD_READ, NY8TEST_PROG_ADDR, 1, &after, &raw);
		ny8_leave();
		printf("VPP set %lumV: before %04X after %04X (raw %04X)\n",
			(unsigned long)vpp, before, after, raw);
		if (after == NY8TEST_PROG_VALUE)
		{
			printf("-> programmed at VPP %lumV\n", (unsigned long)vpp);
			break;
		}
	}
#endif

#if NY8TEST_WRITE_IMAGE
	// ---- step 3: program the image from ny8_image.h ----
	{
		printf("\n== image: %u words, VPP %lumV, XOR %04X\n", NY8_IMAGE_LEN,
			(unsigned long)NY8TEST_VPP_PROG_MV, NY8_IMAGE_XOR);
		uint16_t cur, raw, bad = 0;
		ny8_enter(&cfg, NY8_ENTRY_PLAIN, 0);
		// blank check: every target word must be 0x3FFF or already hold its value
		for (uint16_t k = 0; k < NY8_IMAGE_LEN; k++)
		{
			uint16_t a = ny8_image[k][0], v = ny8_image[k][1] ^ NY8_IMAGE_XOR;
			ny8_read_words(&cfg, NY8_CMD_READ, a, 1, &cur, &raw);
			if (cur != 0x3FFF && cur != v) { printf("not blank: %03X = %04X\n", a, cur); bad++; }
		}
		if (bad)
		{
			printf("-> abort, %u words not blank\n", bad);
		}
		else
		{
			for (uint16_t k = 0; k < NY8_IMAGE_LEN; k++)
			{
				uint16_t a = ny8_image[k][0], v = ny8_image[k][1] ^ NY8_IMAGE_XOR;
				if (v != 0x3FFF) ny8_program_word(&cfg, a, v, NY8TEST_VPP_PROG_MV, NY8TEST_PULSE_US);
			}
			for (uint16_t k = 0; k < NY8_IMAGE_LEN; k++)
			{
				uint16_t a = ny8_image[k][0], v = ny8_image[k][1] ^ NY8_IMAGE_XOR;
				ny8_read_words(&cfg, NY8_CMD_READ, a, 1, &cur, &raw);
				if (cur != v) { printf("verify %03X: want %04X got %04X\n", a, v, cur); bad++; }
			}
			printf("-> %s (%u errors)\n", bad ? "VERIFY FAIL" : "OK", bad);
		}
		ny8_leave();
	}
#endif

	printf("\ndone\n");
	for (;;) Delay_Ms(1000);
}
