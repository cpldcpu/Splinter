#include "ny8_prog.h"

#include "ch32fun.h"
#include "power.h"

// ---------------------------------------------------------------------------
// Splinter R0 socket mapping for the NY8A050E1 SOP8 pinout
//   socket pin 1  DEV_VDD            -> VDD
//   socket pin 4  DEV_ICVPP          -> PB3 / VPP
//   socket pin 5  ICPCK      (PC5)   -> PB2 / SCK
//   socket pin 6  MOSI       (PC6)   -> PB1 / SDO   (input on the programmer side)
//   socket pin 7  PA0        (PD6)   -> PB0 / SDI
//   socket pin 3  ICPDA_MISO (PC7)   -> PB4 (unused, high-Z)
//   socket pin 2  PA7        (PD5)   -> PB5 (unused, high-Z)
//   socket pin 8  GND (1R shunt)     -> VSS
// SOT23-6 parts need an adapter that maps to this SOP8 pinout.
// A Padauk SOT23-6 adapter does NOT fit: it would put VPP on PB2/SCK.
// ---------------------------------------------------------------------------
#define SCK_PIN 5   // GPIOC
#define SDO_PIN 6   // GPIOC
#define SDI_PIN 6   // GPIOD
#define PB4_PIN 7   // GPIOC
#define PB5_PIN 5   // GPIOD

// Timing (writer firmware runs these phases in < 1 us; we stay conservative)
#define T_HALF_US        2
#define T_PWR_SETTLE_MS  5      // writer: 5 ms after VDD on
#define T_ENTRY_LOW_MS   11     // writer: SDO low for 1 + 11 ms
#define T_VPP_ON_MS      5      // writer: VPP on 5 ms before the data frame
#define T_VPP_OFF_MS     10     // writer: 10 ms after VPP off

static inline void pin_cfg(GPIO_TypeDef *port, uint8_t pin, uint32_t mode)
{
	port->CFGLR &= ~(0xFUL << (4 * pin));
	port->CFGLR |= (mode << (4 * pin));
}

static inline void sck(uint8_t v) { if (v) GPIOC->BSHR = 1u << SCK_PIN; else GPIOC->BCR = 1u << SCK_PIN; }
static inline void sdi(uint8_t v) { if (v) GPIOD->BSHR = 1u << SDI_PIN; else GPIOD->BCR = 1u << SDI_PIN; }
static inline uint8_t sdo(void)   { return (GPIOC->INDR >> SDO_PIN) & 1u; }

static void sdo_drive_low(void)
{
	GPIOC->BCR = 1u << SDO_PIN;
	pin_cfg(GPIOC, SDO_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
}

static void sdo_release(void)
{
	// input with pull-up (the writer's 8051 port is open drain with weak pull-up)
	pin_cfg(GPIOC, SDO_PIN, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	GPIOC->BSHR = 1u << SDO_PIN;
}

void ny8_io_init(void)
{
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;
	ny8_io_release();
}

void ny8_io_release(void)
{
	// everything high-Z while the target is unpowered
	pin_cfg(GPIOC, SCK_PIN, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	pin_cfg(GPIOC, SDO_PIN, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	pin_cfg(GPIOD, SDI_PIN, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	pin_cfg(GPIOC, PB4_PIN, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
	pin_cfg(GPIOD, PB5_PIN, GPIO_Speed_In | GPIO_CNF_IN_FLOATING);
}

static void io_active(void)
{
	GPIOC->BCR = 1u << SCK_PIN;
	GPIOD->BCR = 1u << SDI_PIN;
	pin_cfg(GPIOC, SCK_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
	pin_cfg(GPIOD, SDI_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
	sdo_release();
}

void ny8_power_off(void)
{
	power_set_vpp_mv(0);
	power_set_vdd_mv(0);
	ny8_io_release();
}

// 8-bit transfer used for commands (writer routine 7367): SCK high, set SDI, SCK low.
static void send_bits(uint32_t data, uint8_t nbits)
{
	sdi(0);
	for (int8_t i = nbits - 1; i >= 0; i--)
	{
		sck(1);
		sdi((data >> i) & 1u);
		Delay_Us(T_HALF_US);
		sck(0);
		Delay_Us(T_HALF_US);
	}
	sdi(0);
}

void ny8_send_cmd(const ny8_cfg_t *cfg, uint8_t cmd)
{
	send_bits(cmd, 8);
	if (cfg->extra_zero_cmd) send_bits(0, 8);
}

// 16-bit frame (writer routines 708a / 60dd). pulse_us > 0 stretches the SCK-high
// phase of the last bit: that is the programming pulse. A frame with flag=1 is
// followed by one extra clock with SDI high (writer: X616d).
void ny8_send_frame(uint16_t value14, uint8_t flag, uint16_t pulse_us)
{
	uint16_t f = ny8_frame_encode(value14, flag);
	sdi(0);
	for (int8_t i = 15; i >= 0; i--)
	{
		sck(1);
		sdi((f >> i) & 1u);
		if (i == 0 && pulse_us) Delay_Us(pulse_us);
		else Delay_Us(T_HALF_US);
		sck(0);
		Delay_Us(T_HALF_US);
	}
	if (flag)
	{
		sck(1);
		Delay_Us(T_HALF_US);
		sck(0);
		Delay_Us(T_HALF_US);
	}
	sdi(0);
}

// 16-bit read (writer routine 5b5c): SCK high, SCK low, sample SDO.
// On the last read of a burst SDI goes high during bit 15, followed by one
// extra clock, then SDI low.
uint16_t ny8_read_frame_raw(uint8_t last)
{
	uint16_t raw = 0;
	sdi(0);
	for (uint8_t i = 0; i < 16; i++)
	{
		sck(1);
		if (last && i == 15) sdi(1);
		Delay_Us(T_HALF_US);
		sck(0);
		Delay_Us(T_HALF_US);
		raw = (uint16_t)((raw << 1) | sdo());
	}
	if (last)
	{
		sck(1);
		Delay_Us(T_HALF_US);
		sck(0);
		Delay_Us(T_HALF_US);
		sdi(0);
	}
	return raw;
}

uint8_t ny8_enter(const ny8_cfg_t *cfg, ny8_entry_t mode, uint16_t key_delay_us)
{
	ny8_power_off();
	Delay_Ms(20);

	if (mode == NY8_ENTRY_PLAIN)
	{
		// writer routine 5db3: SCK/SDI low and SDO released before power-up (idle state X1d77)
		io_active();
		power_set_vpp_mv(cfg->vpp_idle_mv);
		power_set_vdd_mv(cfg->vdd_mv);
		Delay_Ms(T_PWR_SETTLE_MS);
		sck(0);
		Delay_Ms(1);
		sdo_drive_low();
		Delay_Ms(1);
		sdi(0);
		Delay_Ms(T_ENTRY_LOW_MS);
		sdo_release();
		Delay_Ms(1);
		return 1;
	}

	// writer routines 6df3 + 12aa: VDD on, short wait, key 0x53 0xAD, 10 ms wait.
	// Splinter's VDD rail is an LM358 behind an RC filter, so the rise takes
	// some 100 us; the writer's switch is much faster. Treat this mode as experimental.
	io_active();
	power_set_vpp_mv(cfg->vpp_idle_mv);
	power_set_vdd_mv(cfg->vdd_mv);
	Delay_Us(key_delay_us);
	send_bits(0x53, 8);
	send_bits(0xAD, 8);
	Delay_Ms(10);
	return 1;
}

void ny8_leave(void)
{
	ny8_power_off();
	Delay_Ms(10);
}

void ny8_read_words(const ny8_cfg_t *cfg, uint8_t cmd, uint16_t addr, uint16_t n,
                    uint16_t *words_out, uint16_t *raw_out)
{
	ny8_send_cmd(cfg, cmd);
	ny8_send_frame(addr, 0, 0);
	for (uint16_t i = 0; i < n; i++)
	{
		uint16_t raw = ny8_read_frame_raw(i == (uint16_t)(n - 1));
		if (raw_out) raw_out[i] = raw;
		if (words_out) words_out[i] = ny8_frame_decode(raw);
	}
}

void ny8_program_word(const ny8_cfg_t *cfg, uint16_t addr, uint16_t value14,
                      uint32_t vpp_mv, uint16_t pulse_us)
{
	ny8_send_cmd(cfg, NY8_CMD_PROG);
	ny8_send_frame(addr, 0, 0);
	power_set_vpp_mv(vpp_mv);
	Delay_Ms(T_VPP_ON_MS);
	ny8_send_frame(value14, 1, pulse_us);
	power_set_vpp_mv(cfg->vpp_idle_mv);
	Delay_Ms(T_VPP_OFF_MS);
}
