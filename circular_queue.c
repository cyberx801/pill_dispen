#include <stdlib.h>
#include "circular_queue.h"

// Internal helper to reset the queue pointers
static void reset_queue_pointers(circular_queue_t *q, uint8_t *mem, int cap) {
    q->read_idx = 0;
    q->write_idx = 0;
    q->capacity = cap;
    q->data_array = mem;
}

/**
 * Checks if the queue has no data.
 */
bool queue_is_empty(circular_queue_t *q)
{
    // If read and write indices are at the same spot, it's empty
    return (q->write_idx == q->read_idx);
}

/**
 * Checks if the queue has reached its capacity.
 */
bool queue_is_full(circular_queue_t *q)
{
    // Calculate where the write index would be next
    int next_write = (q->write_idx + 1) % q->capacity;

    // If it catches up to the read index, it's full
    return (next_write == q->read_idx);
}

/**
 * Adds a new byte to the circular buffer.
 * Returns true if successful, false if full.
 */
bool queue_enqueue(circular_queue_t *q, uint8_t byte_val)
{
    // Calculate next position using modulo arithmetic
    int next_pos = (q->write_idx + 1) % q->capacity;

    // Safety check: Prevent overflow
    if (next_pos == q->read_idx) {
        return false;
    }

    // Store the data and advance the write pointer
    q->data_array[q->write_idx] = byte_val;
    q->write_idx = next_pos;

    return true;
}

/**
 * Reads and removes the next byte from the buffer.
 */
uint8_t queue_dequeue(circular_queue_t *q)
{
    // Grab the data at the current read position
    uint8_t val = q->data_array[q->read_idx];

    // Advance the read pointer only if not empty
    if (q->write_idx != q->read_idx) {
        q->read_idx = (q->read_idx + 1) % q->capacity;
    }

    return val;
}

/**
 * Allocates dynamic memory for the queue structure.
 */
void queue_create(circular_queue_t *q, int total_size)
{
    // Use calloc to ensure memory is clean
    uint8_t *mem_block = calloc(total_size, sizeof(uint8_t));

    // Setup the struct members
    reset_queue_pointers(q, mem_block, total_size);
}

/**
 * Cleans up memory.
 */
void queue_destroy(circular_queue_t *q)
{
    if (q->data_array != NULL) {
        free(q->data_array);
        q->data_array = NULL;
    }
    // Safe reset
    q->write_idx = 0;
    q->read_idx = 0;
}