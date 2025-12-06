

#ifndef BLINK_LORA_H
#define BLINK_LORA_H
#ifndef LORAWAN_H
#define LORAWAN_H

#include <stdbool.h>
#include <stddef.h>

// --- Public API (Required by main.c) ---

/**
 * Initializes the LoRaWAN module.
 * Sets up UART, configures keys, mode, class, and attempts to JOIN.
 * @return true if joined successfully, false otherwise.
 */
bool loraInit(void);

/**
 * Sends a message string via LoRaWAN.
 * @param msg: The message content.
 * @param len: Length of the message.
 * @return true if sent successfully, false otherwise.
 */
bool loraMsg(const char *msg, size_t len);

#endif // LORAWAN_H

#endif //BLINK_LORA_H



