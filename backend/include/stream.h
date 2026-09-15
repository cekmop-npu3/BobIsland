#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* No framing, delimiters, or length fields: each scalar is encoded as raw UTF-8. */
typedef struct decoder_t {
    uint32_t value;
    uint32_t minimum;
    unsigned remaining;
} decoder_t;

enum { QUEUE_CAPACITY = 8192 };
typedef struct queue_t {
    uint32_t values[QUEUE_CAPACITY];
    size_t head;
    size_t count;
} queue_t;

typedef int (*write_byte_fn)(void *context, unsigned char byte);
int is_text(uint32_t scalar);
size_t utf8_encode(uint32_t scalar, unsigned char bytes[4]);
/* Returns 1 for a complete scalar, 0 for incomplete, -1 for malformed UTF-8. */
int utf8_feed(decoder_t *decoder, unsigned char byte, uint32_t *scalar);
/* Count only after every byte belonging to the character was written. */
int write_character(uint32_t scalar, write_byte_fn write_byte,
                        void *context, uint64_t *completed);
int queue_push(queue_t *queue, uint32_t scalar);
int queue_pop(queue_t *queue, uint32_t *scalar);
int valid_config(unsigned port, unsigned stop_bits);

#ifdef __cplusplus
}
#endif
#endif
