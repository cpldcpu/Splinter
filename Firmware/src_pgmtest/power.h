#ifndef POWER_H
#define POWER_H

#include <stdint.h>

// Initialize PWM + ADC and calibrate ADC reference (VREFINT).
// Returns measured VDD (mV) derived from VREFINT, or 0 if calibration failed.
// vref_raw_out receives the averaged raw VREFINT ADC code (optional, may be NULL).
uint32_t power_init(uint16_t *vref_raw_out);

// Rails are derived from BOOST. Keep these at 0V when idle.
void power_rails_set_0v(void);

// Enable BOOST regulation (target is configured in power.c via POWER_BOOST_TARGET_MV).
void power_boost_enable(uint8_t enable);

// Sequencing helper: rails=0V, delay, then enable boost.
void power_pgm_begin(void);

// Open-loop setters for linear amplifier outputs.
void power_set_vdd_mv(uint32_t mv);
void power_set_vpp_mv(uint32_t mv);

// Optional monitoring helpers.
uint32_t power_meas_vdd_mv(void);
uint32_t power_meas_vpp_mv(void);
uint32_t power_meas_boost_mv(void);

#endif // POWER_H
