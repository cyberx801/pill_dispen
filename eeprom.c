#include <stdio.h>
#include <string.h>
#include "hardware/i2c.h"
#include "eeprom.h"

// --- Configuration Constants ---
#define I2C_BUS         i2c0    // Main.c initializes i2c0
#define WRITE_DELAY_MS  5       // Required delay for EEPROM write cycle
#define BLOCK_SIZE      64      // Size of one log entry slot
#define MEMORY_CAPACITY 2048    // Total size of EEPROM in bytes
#define MAX_TEXT_LEN    61      // Max characters per log

// --- Internal Helper Prototypes ---
static uint16_t calc_checksum(const uint8_t *ptr, size_t count);
static int format_memory(void);
static void locate_next_slot(uint8_t *addr_buffer);

// -------------------------------------------------------------------------
// Core EEPROM Functions
// -------------------------------------------------------------------------

void eepromWrite(const uint8_t *src_data, size_t byte_count) {
    // Send data to the I2C bus (Address + Payload)
    i2c_write_blocking(I2C_BUS, MEMORY_ADDR, src_data, byte_count, false);
    // Wait for the physical write operation to complete
    sleep_ms(WRITE_DELAY_MS);
}

void eepromRead(const uint8_t *addr_ptr, uint8_t *dest_buf, size_t byte_count) {
    // Step 1: Write the memory address we want to read from
    i2c_write_blocking(I2C_BUS, MEMORY_ADDR, addr_ptr, 2, true);
    sleep_ms(WRITE_DELAY_MS);

    // Step 2: Read the actual data
    i2c_read_blocking(I2C_BUS, MEMORY_ADDR, dest_buf, byte_count, false);
}

// -------------------------------------------------------------------------
// Log Management Logic
// -------------------------------------------------------------------------

/**
 * Calculates a 16-bit CRC checksum to ensure data integrity.
 * Used to detect if a log entry is valid or corrupted.
 */
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

/**
 * Wipes the log section by writing 0x00 to the start of every block.
 * Returns 0 on success, 1 on failure.
 */
static int format_memory(void) {
    uint8_t cmd[3];
    cmd[2] = 0x00; // Marker for "Empty"

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

        if (check_val != 0) return 1; // Formatting failed
    }
    return 0; // Success
}

/**
 * Scans memory blocks to find the first empty slot (starting with 0).
 * If memory is full, it triggers a format.
 */
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

    // If no space, wipe and reset to 0
    if (!slot_found) {
        while(format_memory() != 0); // Retry until success
        addr_buffer[0] = 0x00;
        addr_buffer[1] = 0x00;
    }
}

/**
 * Main Public Function: Reads logs and prints them.
 */
void readEELog(void) {
    printf("\n--- Reading Device Logs ---\n");

    uint8_t entry_data[BLOCK_SIZE];
    uint8_t cursor[3];
    bool is_empty = true;

    for (int i = 0; i < MEMORY_CAPACITY; i += BLOCK_SIZE) {
        cursor[0] = (uint8_t)(i >> 8);
        cursor[1] = (uint8_t)(i & 0xFF);

        // Check if slot is occupied
        eepromRead(cursor, &cursor[2], 1);

        if (cursor[2] != 0) { // Not empty
            is_empty = false;

            // Read the entire block
            eepromRead(cursor, entry_data, BLOCK_SIZE);

            // Find the null terminator to determine string length
            for (int k = 0; k < (BLOCK_SIZE - 2); k++) {
                if (entry_data[k] == 0) {
                    // Validate checksum
                    // Checksum covers message + null terminator
                    if (calc_checksum(entry_data, k + 3) == 0) {
                        printf("[%d] %s\n", i, entry_data);
                    }
                    break;
                }
            }
        }
    }

    if (is_empty) {
        printf("Log is empty.\n");
    }
}

/**
 * Main Public Function: Writes a new log entry.
 */
void writeEELog(const char *message) {
    size_t len = strlen(message);

    // Truncate message if too long
    if (len > MAX_TEXT_LEN) {
        len = MAX_TEXT_LEN;
    }

    printf("Writing Log: %s\n", message);

    // Packet structure: [AddrH, AddrL, Data..., Null, CRC_H, CRC_L]
    size_t total_packet_size = len + 5;
    uint8_t tx_packet[total_packet_size];
    uint8_t crc_buffer[len + 1];

    // 1. Get Address (Fills index 0 and 1 of tx_packet)
    locate_next_slot(tx_packet);

    // 2. Prepare Data Payload
    memcpy(&tx_packet[2], message, len);
    tx_packet[len + 2] = '\0'; // Null terminator

    // 3. Create temp buffer for CRC calc
    memcpy(crc_buffer, message, len);
    crc_buffer[len] = '\0';

    // 4. Compute CRC
    uint16_t checksum = calc_checksum(crc_buffer, sizeof(crc_buffer));

    // 5. Append CRC to packet
    tx_packet[len + 3] = (uint8_t)(checksum >> 8);
    tx_packet[len + 4] = (uint8_t)(checksum & 0xFF);

    // 6. Write to EEPROM
    eepromWrite(tx_packet, total_packet_size);
}