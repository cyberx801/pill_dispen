#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "circular_queue.h"
#include "uart.h"

// --- Internal Function Declarations ---
void process_rx_irq(uart_control_t *ctx);
void process_tx_irq(uart_control_t *ctx);
void isr_uart0_wrapper(void);
void isr_uart1_wrapper(void);

// --- Static Instances for UART0 and UART1 ---
static uart_control_t ctx0 = { .hw_inst = uart0, .irq_num = UART0_IRQ, .irq_fn = isr_uart0_wrapper };
static uart_control_t ctx1 = { .hw_inst = uart1, .irq_num = UART1_IRQ, .irq_fn = isr_uart1_wrapper };

// Helper to select the correct context based on ID (0 or 1)
uart_control_t *get_uart_instance(int uart_id) {
    return uart_id ? &ctx1 : &ctx0;
}

// --- Initialization ---
void uart_setup(int uart_id, int pin_tx, int pin_rx, int baud)
{
    uart_control_t *ctx = get_uart_instance(uart_id);

    // Disable interrupts during configuration
    irq_set_enabled(ctx->irq_num, false);

    // Initialize the circular queues (Allocate memory)
    // Size is set to 256 bytes for both RX and TX
    queue_create(&ctx->rx_queue, 256);
    queue_create(&ctx->tx_queue, 256);

    // Initialize Hardware UART
    uart_init(ctx->hw_inst, baud);
    gpio_set_function(pin_tx, GPIO_FUNC_UART);
    gpio_set_function(pin_rx, GPIO_FUNC_UART);

    // Register the Interrupt Service Routine
    irq_set_exclusive_handler(ctx->irq_num, ctx->irq_fn);

    // Enable RX interrupt. TX is disabled until we have data to send.
    uart_set_irq_enables(ctx->hw_inst, true, false);

    // Enable interrupt on the processor
    irq_set_enabled(ctx->irq_num, true);
}

// --- Read Function ---
int uart_read(int uart_id, uint8_t *dest, int max_len)
{
    int count = 0;
    uart_control_t *ctx = get_uart_instance(uart_id);

    // Pull data from the RX queue until it is empty or max_len is reached
    while(count < max_len && !queue_is_empty(&ctx->rx_queue)) {
        *dest++ = queue_dequeue(&ctx->rx_queue);
        ++count;
    }
    return count;
}

// --- Write Function ---
int uart_write(int uart_id, const uint8_t *src, int len)
{
    int count = 0;
    uart_control_t *ctx = get_uart_instance(uart_id); // typo fix: get_uart_instance

    // Push data to the TX queue
    while(count < len && !queue_is_full(&ctx->tx_queue)) {
        queue_enqueue(&ctx->tx_queue, *src++);
        ++count;
    }

    // Temporarily disable IRQ to safely modify hardware state
    irq_set_enabled(ctx->irq_num, false);

    // If the transmission interrupt is not active, we need to kickstart it
    if(!(uart_get_hw(ctx->hw_inst)->imsc & (1 << UART_UARTIMSC_TXIM_LSB))) {
        // Enable TX interrupt
        uart_set_irq_enables(ctx->hw_inst, true, true);
        // Force the first execution to load the hardware FIFO
        process_tx_irq(ctx);
    }

    // Re-enable interrupts
    irq_set_enabled(ctx->irq_num, true);
    return count;
}

// --- Send String Helper ---
int uart_send(int uart_id, const char *text)
{
    return uart_write(uart_id, (const uint8_t *)text, strlen(text));
}

// --- Interrupt Logic ---

// Handles incoming data
void process_rx_irq(uart_control_t *ctx)
{
    while(uart_is_readable(ctx->hw_inst)) {
        uint8_t c = uart_getc(ctx->hw_inst);
        // Store received byte in software buffer
        queue_enqueue(&ctx->rx_queue, c);
    }
}

// Handles outgoing data
void process_tx_irq(uart_control_t *ctx)
{
    // Fill hardware FIFO from software buffer
    while(!queue_is_empty(&ctx->tx_queue) && uart_is_writable(ctx->hw_inst)) {
        uint8_t val = queue_dequeue(&ctx->tx_queue);
        uart_get_hw(ctx->hw_inst)->dr = val;
    }

    // If no more data to send, disable the TX interrupt to stop CPU load
    if (queue_is_empty(&ctx->tx_queue)) {
        uart_set_irq_enables(ctx->hw_inst, true, false);
    }
}

// --- Interrupt Wrappers ---

void isr_uart0_wrapper(void)
{
    process_rx_irq(&ctx0);
    process_tx_irq(&ctx0);
}

void isr_uart1_wrapper(void)
{
    process_rx_irq(&ctx1);
    process_tx_irq(&ctx1);
}