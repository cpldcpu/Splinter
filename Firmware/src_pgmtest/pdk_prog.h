#ifndef PDK_PROG_H
#define PDK_PROG_H

#include <stdint.h>

#include "pdk_devices.h"

void pdk_io_init(void);

// Reads the 12-bit device ID for a specific scheme (no auto-detect).
// Returns 0 on failure.
uint16_t pdk_read_id12(const pdk_device_t *dev);

// Reads one code word (word address). Returns 1 on success, 0 on failure.
uint8_t pdk_read_word(const pdk_device_t *dev, uint16_t addr, uint16_t *out_word);

// Erases program memory (flash devices only). Returns 1 on success, 0 on failure/unsupported.
uint8_t pdk_erase(const pdk_device_t *dev);

// Writes program memory words. Returns 1 on success, 0 on failure/unsupported.
uint8_t pdk_write_words(const pdk_device_t *dev, uint16_t start_addr, const uint16_t *words, uint16_t count_words);

// Reads and prints program memory to the monitor.
// Dumps [start_word, start_word+count_words).
void pdk_dump_program(const pdk_device_t *dev, uint16_t start_word, uint16_t count_words);

void pdk_print_id(const pdk_device_t *dev);

#endif // PDK_PROG_H
