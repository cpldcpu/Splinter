#include "ch32fun.h"
#include <stdio.h>

#include "pdk_devices.h"
#include "pdk_prog.h"
#include "power.h"

// Safety: keep power rails off by default.
// Set to 1 to actually enable BOOST and attempt an ID read.
#define PGMTEST_ENABLE_POWER 1

// If 1, attempt a chip erase before dumping program memory (only if supported by the selected device).
#define PGMTEST_ERASE_BEFORE_READ 0

// If 1, write a small test pattern before dumping memory (only if supported by the selected device).
#define PGMTEST_WRITE_TEST 0

// Choose exactly one target device (no auto-detection for now).
// Uncomment one line:
// Default is PFS154.
#define PGMTEST_DEVICE_PFS154
// #define PGMTEST_DEVICE_PMS150C

#if (defined(PGMTEST_DEVICE_PFS154) + defined(PGMTEST_DEVICE_PMS150C)) != 1
#error "Select exactly one of PGMTEST_DEVICE_PFS154 or PGMTEST_DEVICE_PMS150C"
#endif

#if defined(PGMTEST_DEVICE_PFS154)
#define PGMTEST_TARGET_DEVICE (&PDK_DEV_PFS154)
#else
#define PGMTEST_TARGET_DEVICE (&PDK_DEV_PMS150C)
#endif

int main(void)
{
	SystemInit();

	printf("src_pgmtest: start\n");

	uint16_t vref_raw = 0;
	uint32_t vref_vdd_mv = power_init(&vref_raw);
	if (vref_vdd_mv)
	{
		printf("VREF calibration: VDD=%lu.%03luV (VREFINT raw avg=%u)\n",
			(unsigned long)(vref_vdd_mv / 1000),
			(unsigned long)(vref_vdd_mv % 1000),
			(unsigned)vref_raw);
	}
	else
	{
		printf("VREF calibration failed (raw avg=%u)\n", (unsigned)vref_raw);
	}
	pdk_io_init();

#if PGMTEST_ENABLE_POWER
	// VDD/VPP are derived from BOOST: BOOST is enabled first while VDD/VPP are 0V.
	power_pgm_begin();

	printf("Rails[mV] VDD=%lu VPP=%lu BOOST=%lu\n",
		(unsigned long)power_meas_vdd_mv(),
		(unsigned long)power_meas_vpp_mv(),
		(unsigned long)power_meas_boost_mv());

	const pdk_device_t *dev = PGMTEST_TARGET_DEVICE;
	printf("Target device: %s\n", dev->name);

	for (;;)
	{
		uint16_t id = pdk_read_id12(dev);
		if (id)
		{
			if (dev->id12_expected && id != dev->id12_expected)
			{
				printf("ID: FAIL got=0x%03X expected=0x%03X\n", id, dev->id12_expected);
			}
			else
			{
				printf("ID: OK 0x%03X\n", id);
				if (PGMTEST_ERASE_BEFORE_READ)
				{
					if (pdk_erase(dev))
					{
						printf("ERASE: OK\n");
					}
					else
					{
						printf("ERASE: SKIP/FAIL (unsupported or failed)\n");
					}
				}

				if (PGMTEST_WRITE_TEST)
				{
					uint16_t pat[16];
					for (uint16_t i = 0; i < 16; i++) pat[i] = i;

					printf("WRITE: addr=0x0100 words=16 ... ");
					if (pdk_write_words(dev, 0x0100, pat, 16))
					{
						printf("OK\n");

						uint8_t ok = 1;
						for (uint16_t i = 0; i < 16; i++)
						{
							uint16_t got = 0;
							if (!pdk_read_word(dev, (uint16_t)(0x0100 + i), &got) || got != pat[i])
							{
								ok = 0;
								break;
							}
						}
						printf("VERIFY: %s\n", ok ? "OK" : "FAIL");
					}
					else
					{
						printf("SKIP/FAIL (unsupported or failed)\n");
					}
				}

				// Dump program memory once after a successful ID read (and optional erase).
				pdk_dump_program(dev, 0, dev->code_words);
				for (;;) { Delay_Ms(1000); }
			}
		}
		else
		{
			printf("ID: FAIL (no response)\n");
		}
		Delay_Ms(1000);
	}
#else
	printf("Power is disabled (PGMTEST_ENABLE_POWER=0). Not reading target.\n");
	for (;;)
	{
		Delay_Ms(1000);
	}
#endif
}
