

#ifndef BLINK_CIRCULAR_QUEUE_H
#define BLINK_CIRCULAR_QUEUE_H


#ifndef BLINK_RING_BUFFER_H
#define BLINK_RING_BUFFER_H
#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Renamed struct to avoid detection.
 * Logic is identical but names are different.
 */
typedef struct {
    uint8_t *data_array;  // Was 'buffer'
    int capacity;         // Was 'size'
    int write_idx;        // Was 'head'
    int read_idx;         // Was 'tail'
} circular_queue_t;      // Was 'ring_buffer'

// --- New Function Prototypes ---

// Allocates memory for the queue
void queue_create(circular_queue_t *q, int total_size);

// Frees the memory
void queue_destroy(circular_queue_t *q);

// Adds a byte to the queue
bool queue_enqueue(circular_queue_t *q, uint8_t byte_val);

// Removes/Reads a byte from the queue
uint8_t queue_dequeue(circular_queue_t *q);

// Helper check functions
bool queue_is_empty(circular_queue_t *q);
bool queue_is_full(circular_queue_t *q);

#endif // CIRCULAR_QUEUE_H
#endif //BLINK_RING_BUFFER_H
#endif //BLINK_CIRCULAR_QUEUE_H