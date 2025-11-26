

#ifndef BLINK_UART_H
#define BLINK_UART_H
#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "circular_queue.h" // Requires the circular_queue_t definition

/*
 * Structure to manage UART instance data.
 * Contains buffers for transmission and reception.
 */
typedef struct {
    circular_queue_t tx_queue;  // Queue for outgoing data
    circular_queue_t rx_queue;  // Queue for incoming data
    uart_inst_t *hw_inst;       // Hardware ID (uart0 or uart1)
    int irq_num;                // Interrupt ID
    irq_handler_t irq_fn;       // Pointer to ISR function
} uart_control_t;

// --- Function Prototypes ---

// Initialize the UART with specific pins and baud rate
void uart_setup(int uart_id, int pin_tx, int pin_rx, int baud);

// Get internal handle (helper function)
uart_control_t *get_uart_instance(int uart_id);

// Read data from the internal buffer into a user array
int uart_read(int uart_id, uint8_t *dest, int max_len);

// Write data from a user array into the internal buffer
int uart_write(int uart_id, const uint8_t *src, int len);

// Send a null-terminated string
int uart_send(int uart_id, const char *text);

#endif // UART_DRIVER_H

#endif //BLINK_UART_H