#ifndef BLINK_UART_H
#define BLINK_UART_H
#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "circular_queue.h"

// uart control structure
typedef struct {
    circular_queue_t tx_queue;
    circular_queue_t rx_queue;
    uart_inst_t *hw_inst;
    int irq_num;
} uart_control_t;

// function prototypes
void uart_setup(int uart_id, int pin_tx, int pin_rx, int baud);
uart_control_t *get_uart_instance(int uart_id);
int uart_read(int uart_id, uint8_t *dest, int max_len);
int uart_write(int uart_id, const uint8_t *src, int len);
int uart_send(int uart_id, const char *text);

#endif // UART_DRIVER_H
#endif //BLINK_UART_H