#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "uart.h"
#include "lora.h"

// --- Configuration ---
#define UART_ID         1       // UART1
#define TX_PIN          4       // GP4
#define RX_PIN          5       // GP5
#define BAUD_RATE       9600

#define RECV_BUFF_SIZE  256
#define SHORT_DELAY     1000    // 1 second
#define JOIN_DELAY      20000   // 20 seconds for Join
#define SEND_DELAY      3000    // 3 seconds for Message

// --- Command Structure ---
typedef struct {
    const char *cmd_str;
    const char *expect_str;
    uint32_t timeout;
} lora_cmd_t;

// --- Command Sequence ---
// Note: We use the "Mong" Key found in your reference code.
static const lora_cmd_t init_sequence[] = {
    {"AT\r\n",                          "+AT: OK",      SHORT_DELAY},
    {"AT+MODE=LWOTAA\r\n",              "+MODE: LWOTAA", SHORT_DELAY},
    {"AT+KEY=APPKEY,\"f20ae78e29e379bdc2c166d268a603a6\"\r\n", "+KEY: APPKEY", SHORT_DELAY},
    {"AT+CLASS=A\r\n",                  "+CLASS: A",    SHORT_DELAY},
    {"AT+PORT=8\r\n",                   "+PORT: 8",     SHORT_DELAY},
    {"AT+JOIN\r\n",                     "joined",       JOIN_DELAY}
};

// --- Internal Helper ---

/**
 * Sends a command and checks if the response contains the expected string.
 */
static bool send_cmd_wait_resp(const char *cmd, const char *expected, uint32_t wait_ms) {
    char rx_buffer[RECV_BUFF_SIZE] = {0};

    // Clear buffer (flush old data)
    while(uart_read(UART_ID, (uint8_t*)rx_buffer, RECV_BUFF_SIZE));

    // Send Command via UART (using our uart.c driver)
    uart_send(UART_ID, cmd);

    // Wait for module to process
    sleep_ms(wait_ms);

    // Read Response
    int bytes_read = uart_read(UART_ID, (uint8_t *)rx_buffer, RECV_BUFF_SIZE - 1);

    if (bytes_read > 0) {
        rx_buffer[bytes_read] = '\0'; // Null terminate

        // Debug print (Optional)
        // printf("CMD: %s -> RX: %s\n", cmd, rx_buffer);

        // Check if response contains expected substring
        if (strstr(rx_buffer, expected) != NULL) {
            return true;
        }
    }

    return false;
}

// --- Public Functions ---

bool loraInit(void) {
    printf("LoRaWAN: Initializing...\n");

    // Initialize UART Driver
    uart_setup(UART_ID, TX_PIN, RX_PIN, BAUD_RATE);

    int total_cmds = sizeof(init_sequence) / sizeof(init_sequence[0]);

    // Iterate through configuration commands
    for (int i = 0; i < total_cmds; i++) {
        if (!send_cmd_wait_resp(init_sequence[i].cmd_str,
                                init_sequence[i].expect_str,
                                init_sequence[i].timeout)) {

            // If Join fails, we return false so main.c can retry
            if (strstr(init_sequence[i].cmd_str, "JOIN") != NULL) {
                printf("LoRaWAN: Join Failed.\n");
            } else {
                printf("LoRaWAN: Config Failed at step %d\n", i);
            }
            return false;
        }
    }

    printf("LoRaWAN: Connected/Joined Successfully.\n");
    return true;
}

bool loraMsg(const char *msg, size_t msg_size) {
    char packet_buffer[RECV_BUFF_SIZE];

    // Safety check for length
    if (msg_size > 200) return false;

    // Format string: AT+MSG="Hello"
    // snprintf is safer than strcpy/strcat
    snprintf(packet_buffer, sizeof(packet_buffer), "AT+MSG=\"%s\"\r\n", msg);

    // Send and expect a confirmation (usually "+MSG: Done" or similar)
    // The reference code expects a response, so we wait.
    if (send_cmd_wait_resp(packet_buffer, "Done", SEND_DELAY)) {
        return true;
    }

    // Sometimes it returns OK immediately but Done later,
    // basic check passed if we are here.
    return false;
}