//
// Created by fozma on 26.11.2025.
//

#ifndef BLINK_EEPROM_H
#define BLINK_EEPROM_H
#include "pico/stdlib.h"
#include <stddef.h>
#include <stdint.h>

// I2C Address for standard 24LCxx EEPROM
#define MEMORY_ADDR 0x50

// --- Public Functions called by main.c ---

/**
 * Writes a sequence of bytes to the EEPROM.
 * @param src_data Pointer to the data buffer (Address bytes + Data)
 * @param byte_count Total number of bytes to write
 */
void eepromWrite(const uint8_t *src_data, size_t byte_count);

/**
 * Reads a sequence of bytes from the EEPROM.
 * @param addr_ptr Pointer to the 2-byte address array
 * @param dest_buf Pointer to the buffer where read data will be stored
 * @param byte_count Number of bytes to read
 */
void eepromRead(const uint8_t *addr_ptr, uint8_t *dest_buf, size_t byte_count);

/**
 * Reads all valid logs from EEPROM and prints them to stdout.
 */
void readEELog(void);

/**
 * Writes a new log string to the EEPROM with CRC validation.
 * @param message Null-terminated string to save
 */
void writeEELog(const char *message);
#endif //BLINK_EEPROM_H