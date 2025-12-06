#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"
#include "eeprom.h"

// --- Configuration Constants ---
#define I2C_BUS         i2c0
#define WRITE_DELAY_MS  5
#define BLOCK_SIZE      64      // Size of one log entry
#define MEMORY_CAPACITY 1800    // Use only first 1800 bytes for logs (leaves 248 bytes for states)
#define MAX_TEXT_LEN    61      // Max characters per log

// --- Internal Helper Prototypes ---
static uint16_t calc_checksum(const uint8_t *ptr, size_t count);
static int format_memory(void);
static void locate_next_slot(uint8_t *addr_buffer);

// -------------------------------------------------------------------------
// Core EEPROM Functions
// -------------------------------------------------------------------------

void eepromWrite(const uint8_t *src_data, size_t byte_count) {
    i2c_write_blocking(I2C_BUS, MEMORY_ADDR, src_data, byte_count, false);
    sleep_ms(WRITE_DELAY_MS);
}

void eepromRead(const uint8_t *addr_ptr, uint8_t *dest_buf, size_t byte_count) {
    i2c_write_blocking(I2C_BUS, MEMORY_ADDR, addr_ptr, 2, true);
    sleep_ms(WRITE_DELAY_MS);
    i2c_read_blocking(I2C_BUS, MEMORY_ADDR, dest_buf, byte_count, false);
}

// -------------------------------------------------------------------------
// Log Management Logic
// -------------------------------------------------------------------------

static uint16_t calc_checksum(const uint8_t *ptr, size_t count) {
    uint16_t crc = 0xFFFF;
    uint8_t temp;

    while (count > 0) {
        temp = (crc >> 8) ^ *ptr++;
        temp ^= (temp >> 4);

        uint16_t term1 = (temp << 12);
        uint16_t term2 = (temp << 5);

        crc = (crc << 8) ^ term1 ^ term2 ^ ((uint16_t)temp);
        count--;
    }
    return crc;
}

static int format_memory(void) {
    uint8_t cmd[3];
    cmd[2] = 0x00;

    printf("Formatting EEPROM log area...\n");

    // Erase loop
    for (int offset = 0; offset < MEMORY_CAPACITY; offset += BLOCK_SIZE) {
        cmd[0] = (uint8_t)(offset >> 8);
        cmd[1] = (uint8_t)(offset & 0xFF);
        eepromWrite(cmd, 3);
    }

    // Verification loop
    for (int offset = 0; offset < MEMORY_CAPACITY; offset += BLOCK_SIZE) {
        cmd[0] = (uint8_t)(offset >> 8);
        cmd[1] = (uint8_t)(offset & 0xFF);

        uint8_t check_val;
        eepromRead(cmd, &check_val, 1);

        if (check_val != 0) {
            printf("Format verification failed at %d\n", offset);
            return 1;
        }
    }
    printf("Format complete!\n");
    return 0;
}

static void locate_next_slot(uint8_t *addr_buffer) {
    bool slot_found = false;
    uint8_t header_byte;
    int current_addr = 0;

    // Search for available space
    while (current_addr < MEMORY_CAPACITY) {
        addr_buffer[0] = (uint8_t)(current_addr >> 8);
        addr_buffer[1] = (uint8_t)(current_addr & 0xFF);

        eepromRead(addr_buffer, &header_byte, 1);

        if (header_byte == 0) {
            slot_found = true;
            break;
        }
        current_addr += BLOCK_SIZE;
    }

    // If no space, wipe and reset
    if (!slot_found) {
        printf("Log memory full, formatting...\n");
        while(format_memory() != 0);
        addr_buffer[0] = 0x00;
        addr_buffer[1] = 0x00;
    }
}

void readEELog(void) {
    printf("\n=== Device Log History ===\n");

    uint8_t entry_data[BLOCK_SIZE];
    uint8_t cursor[3];
    bool is_empty = true;
    int log_count = 0;

    for (int i = 0; i < MEMORY_CAPACITY; i += BLOCK_SIZE) {
        cursor[0] = (uint8_t)(i >> 8);
        cursor[1] = (uint8_t)(i & 0xFF);

        // Check if slot is occupied
        eepromRead(cursor, &cursor[2], 1);

        if (cursor[2] != 0) {
            is_empty = false;

            // Read the entire block
            eepromRead(cursor, entry_data, BLOCK_SIZE);

            // Find null terminator
            for (int k = 0; k < (BLOCK_SIZE - 2); k++) {
                if (entry_data[k] == 0) {
                    // Validate checksum
                    if (calc_checksum(entry_data, k + 3) == 0) {
                        printf("[%d] %s\n", log_count, entry_data);
                        log_count++;
                    } else {
                        printf("[%d] <corrupted>\n", log_count);
                        log_count++;
                    }
                    break;
                }
            }
        }
    }

    if (is_empty) {
        printf("No logs found.\n");
    }
    printf("=========================\n\n");
}

void writeEELog(const char *message) {
    size_t len = strlen(message);

    // Truncate if too long
    if (len > MAX_TEXT_LEN) {
        len = MAX_TEXT_LEN;
    }

    printf("LOG: %s\n", message);

    // Packet: [AddrH, AddrL, Data..., Null, CRC_H, CRC_L]
    size_t total_packet_size = len + 5;
    uint8_t tx_packet[total_packet_size];
    uint8_t crc_buffer[len + 1];

    // Get address
    locate_next_slot(tx_packet);

    // Prepare data
    memcpy(&tx_packet[2], message, len);
    tx_packet[len + 2] = '\0';

    // Compute CRC
    memcpy(crc_buffer, message, len);
    crc_buffer[len] = '\0';
    uint16_t checksum = calc_checksum(crc_buffer, sizeof(crc_buffer));

    // Append CRC
    tx_packet[len + 3] = (uint8_t)(checksum >> 8);
    tx_packet[len + 4] = (uint8_t)(checksum & 0xFF);

    // Write to EEPROM
    eepromWrite(tx_packet, total_packet_size);
}