#include "ch32fun.h"
#include <stdio.h>

// PWM period (48MHz / 48 = 1MHz)
#define PWM_PERIOD 47

// PWM channels
#define PWM_CH_BOOST 1  // PD2 - TIM1_CH1
#define PWM_CH_VDD   2  // PA1 - TIM1_CH2
#define PWM_CH_VPP   3  // PC3 - TIM1_CH3

void pwm_init(void)
{
	// Enable TIM1
	RCC->APB2PCENR |= RCC_APB2Periph_TIM1;

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

	// No prescaler
	TIM1->PSC = 0;

	// Set period for ~1MHz
	TIM1->ATRLR = PWM_PERIOD;

	// CH1, CH2, CH3: PWM mode 1 (OC1M = 110)
	TIM1->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1 | TIM_OC2M_2 | TIM_OC2M_1;
	TIM1->CHCTLR2 = TIM_OC3M_2 | TIM_OC3M_1;

	// Enable CH1, CH2, CH3 outputs, active high
	TIM1->CCER = TIM_CC1E | TIM_CC2E | TIM_CC3E;

	// Initialize duty to 0
	TIM1->CH1CVR = 0;
	TIM1->CH2CVR = 0;
	TIM1->CH3CVR = 0;

	// Load registers
	TIM1->SWEVGR |= TIM_UG;

	// Enable main output
	TIM1->BDTR |= TIM_MOE;

	// Enable TIM1
	TIM1->CTLR1 |= TIM_CEN;
}

void pwm_set_duty(uint8_t channel, uint8_t duty)
{
	// duty: 0-PWM_PERIOD
	switch (channel)
	{
		case 1: TIM1->CH1CVR = duty; break;
		case 2: TIM1->CH2CVR = duty; break;
		case 3: TIM1->CH3CVR = duty; break;
	}
}

// ADC channels
#define ADC_CH_VDD     0  // PA2
#define ADC_CH_VPP     4  // PD3
#define ADC_CH_BOOST   7  // PD4
#define ADC_CH_VREFINT 8  // Internal VREFINT (bandgap)

// Internal reference voltage (bandgap) in mV (typical)
#define VREFINT_MV 1200

// ADC reference voltage in mV (measured from VREFINT, default fallback)
static uint32_t vref_mv = 5000;
static uint16_t vref_raw_last = 0;
#define VREF_SAMPLES 32

// Voltage divider ratios (multiplied by 1000 for integer math)
// VDD:   20k / (20k + 20k) = 0.5   -> mult = 2000
// BOOST: 1k / (1k + 5.1k)  = 0.164 -> mult = 6100
// VPP:   5.1k / (5.1k + 20k) = 0.203 -> mult = 4922
#define DIVIDER_VDD   2000
#define DIVIDER_BOOST 6100
#define DIVIDER_VPP   4922

uint32_t adc_to_mv(uint16_t adc_val, uint16_t divider_mult)
{
	// Convert ADC to sensed voltage in mV (max ~vref_mv)
	uint32_t sensed_mv = (uint32_t)adc_val * vref_mv / 1023;
	// Apply divider multiplier (max 5000 * 6100 = 30.5M, fits in uint32_t)
	return sensed_mv * divider_mult / 1000;
}

void adc_init(void)
{
	// ADCCLK = 24 MHz => RCC_ADCPRE = 0: divide by 2
	RCC->CFGR0 &= ~(0x1F << 11);

	// Enable GPIOA, GPIOD and ADC
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOD | RCC_APB2Periph_ADC1;

	// Configure analog inputs (CNF=00, MODE=00)
	GPIOA->CFGLR &= ~(0xF << (4 * 2));  // PA2
	GPIOD->CFGLR &= ~(0xF << (4 * 3));  // PD3
	GPIOD->CFGLR &= ~(0xF << (4 * 4));  // PD4

	// Reset ADC
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

	// Set up single conversion
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;

	// Set sampling time to 241 cycles for all channels we use
	ADC1->SAMPTR2 = (7 << (3 * ADC_CH_VDD)) | (7 << (3 * ADC_CH_VPP)) |
		(7 << (3 * ADC_CH_BOOST)) | (7 << (3 * ADC_CH_VREFINT));

	// Turn on ADC, enable internal VREF/temperature, set software trigger
	ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL | ADC_TSVREFE;

	// Calibrate
	ADC1->CTLR2 |= ADC_RSTCAL;
	while (ADC1->CTLR2 & ADC_RSTCAL);
	ADC1->CTLR2 |= ADC_CAL;
	while (ADC1->CTLR2 & ADC_CAL);
}

uint16_t adc_read(uint8_t channel)
{
	// Select channel
	ADC1->RSQR3 = channel;

	// Start conversion
	ADC1->CTLR2 |= ADC_SWSTART;

	// Wait for completion
	while (!(ADC1->STATR & ADC_EOC));

	return ADC1->RDATAR;
}

uint8_t boost_set_target_mv(uint32_t target_mv, uint8_t min_duty, uint8_t max_duty)
{
	uint8_t duty = (uint8_t)TIM1->CH1CVR;
	while (duty < min_duty)
	{
		duty++;
		pwm_set_duty(PWM_CH_BOOST, duty);
		Delay_Ms(20);
	}
	while (duty > max_duty)
	{
		duty--;
		pwm_set_duty(PWM_CH_BOOST, duty);
		Delay_Ms(20);
	}

	// First, ensure we are below target.
	while (duty > min_duty)
	{
		uint32_t boost_mv = adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
		if (boost_mv < target_mv)
		{
			break;
		}
		duty--;
		pwm_set_duty(PWM_CH_BOOST, duty);
		Delay_Ms(20);
	}

	// Then, step up until we would exceed target, ending at the highest value below target.
	while (duty < max_duty)
	{
		uint32_t boost_mv = adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
		if (boost_mv >= target_mv)
		{
			if (duty > min_duty)
			{
				duty--;
				pwm_set_duty(PWM_CH_BOOST, duty);
				Delay_Ms(20);
			}
			break;
		}
		duty++;
		pwm_set_duty(PWM_CH_BOOST, duty);
		Delay_Ms(20);
	}

	return duty;
}

uint32_t adc_read_vref_mv_avg(uint8_t samples)
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

int main()
{
	SystemInit();

	// Enable GPIO clocks
	RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;

	// Initialize PWM on PD2, PA1, PC3 (starts with 0% duty)
	pwm_init();

	adc_init();

	printf("success\n");

	uint32_t vdd_mv = adc_read_vref_mv_avg(VREF_SAMPLES);
	if (vdd_mv != 0)
	{
		vref_mv = vdd_mv;
		printf("VREF calibration: VDD:%lu.%03luV (VREFINT raw avg:%u)\n",
			vdd_mv / 1000, vdd_mv % 1000, (unsigned)vref_raw_last);
	}
	else
	{
		printf("VREF calibration failed, using default VREF %lu mV\n", vref_mv);
	}

	// Set VDD and VPP PWM to 0
	pwm_set_duty(PWM_CH_VDD, 0);
	pwm_set_duty(PWM_CH_VPP, 0);

	// Set boost to target 15V before main loop
	boost_set_target_mv(15000, 0, PWM_PERIOD);

	// Sweep VDD and VPP duty
	uint8_t max_duty = PWM_PERIOD;

	while (1)
	{
		for (uint8_t duty = 0; duty <= max_duty; duty++)
		{
			pwm_set_duty(PWM_CH_VDD, duty);
			pwm_set_duty(PWM_CH_VPP, duty);
			Delay_Ms(500);

			uint32_t boost_mv = adc_to_mv(adc_read(ADC_CH_BOOST), DIVIDER_BOOST);
			uint32_t vdd_mv = adc_to_mv(adc_read(ADC_CH_VDD), DIVIDER_VDD);
			uint32_t vpp_mv = adc_to_mv(adc_read(ADC_CH_VPP), DIVIDER_VPP);
			printf("Duty:\t%d\t%d VDD\t%lu.%02lu VPP\t%lu.%02lu Boost\t%lu.%02lu\n",
				duty, PWM_PERIOD,
				vdd_mv / 1000, (vdd_mv % 1000) / 10,
				vpp_mv / 1000, (vpp_mv % 1000) / 10,
				boost_mv / 1000, (boost_mv % 1000) / 10);
		}

		// Reset to 0 and repeat
		pwm_set_duty(PWM_CH_VDD, 0);
		pwm_set_duty(PWM_CH_VPP, 0);
		Delay_Ms(1000);
	}
}
