#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "circular_queue.h"
#include "uart.h"

// uart contexts
static uart_control_t uart0_ctx;
static uart_control_t uart1_ctx;

// forward declarations
void rx_handler(uart_control_t *ctx);
void tx_handler(uart_control_t *ctx);
void isr_uart0_wrapper(void);
void isr_uart1_wrapper(void);

// get uart instance by id
uart_control_t *get_uart_instance(int uart_id) {
    if(uart_id == 1)
        return &uart1_ctx;
    else
        return &uart0_ctx;
}

// setup uart with pins and baudrate
void uart_setup(int uart_id, int pin_tx, int pin_rx, int baud)
{
    uart_control_t *ctx = get_uart_instance(uart_id);

    // init context
    ctx->irq_num = (uart_id == 0) ? UART0_IRQ : UART1_IRQ;
    ctx->hw_inst = (uart_id == 0) ? uart0 : uart1;

    irq_set_enabled(ctx->irq_num, false);

    // create queues for rx and tx
    queue_create(&ctx->rx_queue, 256);
    queue_create(&ctx->tx_queue, 256);

    // init hardware uart
    uart_init(ctx->hw_inst, baud);
    gpio_set_function(pin_tx, GPIO_FUNC_UART);
    gpio_set_function(pin_rx, GPIO_FUNC_UART);

    // setup interrupt handler manually
    if(uart_id == 0) {
        irq_set_exclusive_handler(UART0_IRQ, isr_uart0_wrapper);
    } else {
        irq_set_exclusive_handler(UART1_IRQ, isr_uart1_wrapper);
    }

    // enable rx interrupt only
    uart_set_irq_enables(ctx->hw_inst, true, false);

    irq_set_enabled(ctx->irq_num, true);
}

// read data from uart
int uart_read(int uart_id, uint8_t *dest, int max_len)
{
    int cnt = 0;
    uart_control_t *ctx = get_uart_instance(uart_id);

    // read from queue
    while(cnt < max_len && !queue_is_empty(&ctx->rx_queue)) {
        *dest++ = queue_dequeue(&ctx->rx_queue);
        cnt++;
    }
    return cnt;
}

// write data to uart
int uart_write(int uart_id, const uint8_t *src, int len)
{
    int cnt = 0;
    uart_control_t *ctx = get_uart_instance(uart_id);

    // put data in tx queue
    while(cnt < len && !queue_is_full(&ctx->tx_queue)) {
        queue_enqueue(&ctx->tx_queue, *src++);
        cnt++;
    }

    // disable irq
    irq_set_enabled(ctx->irq_num, false);

    // check if tx interrupt is active
    if(!(uart_get_hw(ctx->hw_inst)->imsc & (1 << UART_UARTIMSC_TXIM_LSB))) {
        uart_set_irq_enables(ctx->hw_inst, true, true);
        tx_handler(ctx);
    }

    // enable irq again
    irq_set_enabled(ctx->irq_num, true);
    return cnt;
}

// send string helper
int uart_send(int uart_id, const char *text)
{
    return uart_write(uart_id, (const uint8_t *)text, strlen(text));
}

// handle rx interrupt
void rx_handler(uart_control_t *ctx)
{
    while(uart_is_readable(ctx->hw_inst)) {
        uint8_t c = uart_getc(ctx->hw_inst);
        queue_enqueue(&ctx->rx_queue, c);
    }
}

// handle tx interrupt
void tx_handler(uart_control_t *ctx)
{
    // send data from queue to hardware
    while(!queue_is_empty(&ctx->tx_queue) && uart_is_writable(ctx->hw_inst)) {
        uint8_t val = queue_dequeue(&ctx->tx_queue);
        uart_get_hw(ctx->hw_inst)->dr = val;
    }

    // disable tx interrupt if done
    if (queue_is_empty(&ctx->tx_queue)) {
        uart_set_irq_enables(ctx->hw_inst, true, false);
    }
}

// uart0 interrupt
void isr_uart0_wrapper(void)
{
    rx_handler(&uart0_ctx);
    tx_handler(&uart0_ctx);
}

// uart1 interrupt
void isr_uart1_wrapper(void)
{
    rx_handler(&uart1_ctx);
    tx_handler(&uart1_ctx);
}