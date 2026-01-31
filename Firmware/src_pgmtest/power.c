#include "power.h"

#include "ch32fun.h"

// PWM period (48MHz / 48 = 1MHz)
#define PWM_PERIOD 47

// ADC channels
#define ADC_CH_VDD     0  // PA2
#define ADC_CH_VPP     4  // PD3
#define ADC_CH_BOOST   7  // PD4
#define ADC_CH_VREFINT 8  // Internal VREFINT (bandgap)

// Internal reference voltage (bandgap) in mV (typical)
#define VREFINT_MV 1200
#define VREF_SAMPLES 32

// Voltage divider ratios (multiplied by 1000 for integer math)
#define DIVIDER_VDD   2000
#define DIVIDER_BOOST 6100
#define DIVIDER_VPP   4922

// Calibrated linear amplifier transfer functions (initial assumption).
// VDD = duty * 0.219V  -> 219mV per LSB
// VPP = duty * 0.5275V -> 527.5mV per LSB
#define VDD_UV_PER_DUTY 219000UL
#define VPP_UV_PER_DUTY 527500UL

#ifndef POWER_BOOST_TARGET_MV
#define POWER_BOOST_TARGET_MV 18000
#endif

static uint32_t vref_mv = 5000;
static uint16_t vref_raw_last = 0;

static inline void set_boost_duty(uint8_t duty) { TIM1->CH1CVR = duty; }
static inline void set_vdd_duty(uint8_t duty)   { TIM1->CH2CVR = duty; }
static inline void set_vpp_duty(uint8_t duty)   { TIM1->CH3CVR = duty; }

static void power_enable_clocks(void)
{
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1 | RCC_APB2Periph_ADC1;
}

static void pwm_init(void)
{
	// Configure pins as alternate function push-pull
	// PD2 = TIM1_CH1
	GPIOD->CFGLR &= ~(0xF << (4 * 2));
	GPIOD->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (4 * 2);
	// PA1 = TIM1_CH2
	GPIOA->CFGLR &= ~(0xF << (4 * 1));
	GPIOA->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (4 * 1);
	// PC3 = TIM1_CH3
	GPIOC->CFGLR &= ~(0xF << (4 * 3));
	GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP_AF) << (4 * 3);

	// Reset TIM1
	RCC->APB2PRSTR |= RCC_APB2Periph_TIM1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_TIM1;

	TIM1->PSC = 0;            // No prescaler
	TIM1->ATRLR = PWM_PERIOD; // ~1MHz

	// PWM mode 1
	TIM1->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC2M_2 | TIM_OC2M_1;
	TIM1->CHCTLR2 = TIM_OC3M_2 | TIM_OC3M_1;

	// Enable CH1, CH2, CH3 outputs
	TIM1->CCER = TIM_CC1E | TIM_CC2E | TIM_CC3E;

	// Initialize duty to 0
	set_boost_duty(0);
	set_vdd_duty(0);
	set_vpp_duty(0);

	TIM1->SWEVGR |= TIM_UG;  // Load registers
	TIM1->BDTR |= TIM_MOE;   // Enable main output
	TIM1->CTLR1 |= TIM_CEN;  // Enable TIM1
}

static uint16_t adc_read(uint8_t channel)
{
	ADC1->RSQR3 = channel;
	ADC1->CTLR2 |= ADC_SWSTART;
	while (!(ADC1->STATR & ADC_EOC)) {}
	return ADC1->RDATAR;
}

static uint32_t adc_to_mv(uint16_t adc_val, uint16_t divider_mult)
{
	uint32_t sensed_mv = (uint32_t)adc_val * vref_mv / 1023;
	return sensed_mv * divider_mult / 1000;
}

static void adc_init(void)
{
	// ADCCLK = 24 MHz => RCC_ADCPRE = 0: divide by 2
	RCC->CFGR0 &= ~(0x1F << 11);

	// Configure analog inputs (CNF=00, MODE=00)
	GPIOA->CFGLR &= ~(0xF << (4 * 2));  // PA2
	GPIOD->CFGLR &= ~(0xF << (4 * 3));  // PD3
	GPIOD->CFGLR &= ~(0xF << (4 * 4));  // PD4

	// Reset ADC
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;

	// Set sampling time to 241 cycles for all channels we use
	ADC1->SAMPTR2 = (7 << (3 * ADC_CH_VDD)) | (7 << (3 * ADC_CH_VPP)) |
		(7 << (3 * ADC_CH_BOOST)) | (7 << (3 * ADC_CH_VREFINT));

	// Turn on ADC, enable internal VREF/temperature, set software trigger
	ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL | ADC_TSVREFE;

	// Calibrate
	ADC1->CTLR2 |= ADC_RSTCAL;
	while (ADC1->CTLR2 & ADC_RSTCAL) {}
	ADC1->CTLR2 |= ADC_CAL;
	while (ADC1->CTLR2 & ADC_CAL) {}
}

static uint32_t adc_read_vref_mv_avg(uint8_t samples)
{
	uint32_t sum = 0;
	for (uint8_t i = 0; i < samples; i++)
	{
		sum += adc_read(ADC_CH_VREFINT);
	}
	vref_raw_last = (uint16_t)((sum + (samples / 2)) / samples);
	if (vref_raw_last == 0)
	{
		return 0;
	}
	// VDD = VREFINT * 1023 / raw
	return (uint32_t)VREFINT_MV * 1023 / vref_raw_last;
}

static uint8_t duty_from_uv(uint64_t target_uv, uint32_t uv_per_duty)
{
	uint64_t duty = (target_uv + (uv_per_duty / 2)) / uv_per_duty; // rounded
	if (duty > PWM_PERIOD) duty = PWM_PERIOD;
	return (uint8_t)duty;
}

void power_set_vdd_mv(uint32_t mv)
{
	uint64_t uv = (uint64_t)mv * 1000ULL;
	set_vdd_duty(duty_from_uv(uv, VDD_UV_PER_DUTY));
}

void power_set_vpp_mv(uint32_t mv)
{
	uint64_t uv = (uint64_t)mv * 1000ULL;
	set_vpp_duty(duty_from_uv(uv, VPP_UV_PER_DUTY));
}

void power_rails_set_0v(void)
{
	set_vdd_duty(0);
	set_vpp_duty(0);
}

static uint8_t boost_set_target_mv(uint32_t target_mv, uint8_t min_duty, uint8_t max_duty)
{
	uint8_t duty = (uint8_t)TIM1->CH1CVR;
	while (duty < min_duty)
	{
		duty++;
		set_boost_duty(duty);
		Delay_Ms(20);
	}
	while (duty > max_duty)
	{
		duty--;
		set_boost_duty(duty);
		Delay_Ms(20);
	}

	// Objective: highest value below target_mv.
	while (duty > min_duty)
	{
		uint32_t boost_mv = adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
		if (boost_mv < target_mv) break;
		duty--;
		set_boost_duty(duty);
		Delay_Ms(20);
	}

	while (duty < max_duty)
	{
		uint32_t boost_mv = adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
		if (boost_mv >= target_mv)
		{
			if (duty > min_duty)
			{
				duty--;
				set_boost_duty(duty);
				Delay_Ms(20);
			}
			break;
		}
		duty++;
		set_boost_duty(duty);
		Delay_Ms(20);
	}

	return duty;
}

void power_boost_enable(uint8_t enable)
{
	if (!enable)
	{
		set_boost_duty(0);
		return;
	}
	(void)boost_set_target_mv(POWER_BOOST_TARGET_MV, 0, PWM_PERIOD);
}

void power_pgm_begin(void)
{
	power_rails_set_0v();
	Delay_Ms(50);
	power_boost_enable(1);
}

uint32_t power_meas_vdd_mv(void)
{
	return adc_to_mv(adc_read(ADC_CH_VDD), DIVIDER_VDD);
}

uint32_t power_meas_vpp_mv(void)
{
	return adc_to_mv(adc_read(ADC_CH_VPP), DIVIDER_VPP);
}

uint32_t power_meas_boost_mv(void)
{
	return adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
}

uint32_t power_init(uint16_t *vref_raw_out)
{
	power_enable_clocks();

	pwm_init();
	adc_init();

	uint32_t vdd_mv = adc_read_vref_mv_avg(VREF_SAMPLES);
	if (vdd_mv != 0)
	{
		vref_mv = vdd_mv;
	}

	if (vref_raw_out)
	{
		*vref_raw_out = vref_raw_last;
	}

	// Default safe state.
	power_rails_set_0v();
	set_boost_duty(0);

	return vdd_mv;
}
