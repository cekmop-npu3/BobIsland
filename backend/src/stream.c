#include "stream.h"

static int valid_scalar(uint32_t scalar)
{
    return scalar <= 0x10ffffu && !(scalar >= 0xd800u && scalar <= 0xdfffu);
}

int is_text(uint32_t scalar)
{
    return valid_scalar(scalar) && (scalar == '\n' ||
           (scalar >= 0x20u && !(scalar >= 0x7fu && scalar <= 0x9fu)));
}

size_t utf8_encode(uint32_t scalar, unsigned char bytes[4])
{
    if (!bytes || !valid_scalar(scalar)) return 0;
    if (scalar <= 0x7fu) { bytes[0] = (unsigned char)scalar; return 1; }
    if (scalar <= 0x7ffu) {
        bytes[0] = (unsigned char)(0xc0u | (scalar >> 6));
        bytes[1] = (unsigned char)(0x80u | (scalar & 63u));
        return 2;
    }
    if (scalar <= 0xffffu) {
        bytes[0] = (unsigned char)(0xe0u | (scalar >> 12));
        bytes[1] = (unsigned char)(0x80u | ((scalar >> 6) & 63u));
        bytes[2] = (unsigned char)(0x80u | (scalar & 63u));
        return 3;
    }
    bytes[0] = (unsigned char)(0xf0u | (scalar >> 18));
    bytes[1] = (unsigned char)(0x80u | ((scalar >> 12) & 63u));
    bytes[2] = (unsigned char)(0x80u | ((scalar >> 6) & 63u));
    bytes[3] = (unsigned char)(0x80u | (scalar & 63u));
    return 4;
}

int utf8_feed(decoder_t *decoder, unsigned char byte, uint32_t *scalar)
{
    if (!decoder || !scalar) return -1;
    if (!decoder->remaining) {
        if (byte <= 0x7fu) { *scalar = byte; return 1; }
        if (byte >= 0xc2u && byte <= 0xdfu) {
            decoder->value = byte & 31u; decoder->minimum = 0x80u; decoder->remaining = 1;
        } else if (byte >= 0xe0u && byte <= 0xefu) {
            decoder->value = byte & 15u; decoder->minimum = 0x800u; decoder->remaining = 2;
        } else if (byte >= 0xf0u && byte <= 0xf4u) {
            decoder->value = byte & 7u; decoder->minimum = 0x10000u; decoder->remaining = 3;
        } else return -1;
        return 0;
    }
    if ((byte & 0xc0u) != 0x80u) { decoder->remaining = 0; return -1; }
    decoder->value = (decoder->value << 6) | (byte & 63u);
    if (--decoder->remaining) return 0;
    if (decoder->value < decoder->minimum || !valid_scalar(decoder->value)) return -1;
    *scalar = decoder->value;
    return 1;
}

int write_character(uint32_t scalar, write_byte_fn write_byte,
                        void *context, uint64_t *completed)
{
    unsigned char bytes[4];
    size_t i, length;
    if (!is_text(scalar) || !write_byte || !completed) return 0;
    length = utf8_encode(scalar, bytes);
    for (i = 0; i < length; ++i) if (!write_byte(context, bytes[i])) return 0;
    ++*completed;
    return 1;
}

int queue_push(queue_t *queue, uint32_t scalar)
{
    if (!queue || !is_text(scalar) || queue->count == QUEUE_CAPACITY) return 0;
    queue->values[(queue->head + queue->count) % QUEUE_CAPACITY] = scalar;
    ++queue->count;
    return 1;
}

int queue_pop(queue_t *queue, uint32_t *scalar)
{
    if (!queue || !scalar || !queue->count) return 0;
    *scalar = queue->values[queue->head];
    queue->head = (queue->head + 1) % QUEUE_CAPACITY;
    --queue->count;
    return 1;
}

int valid_config(unsigned port, unsigned stop_bits)
{
    return port >= 1 && port <= 65535 && (stop_bits == 1 || stop_bits == 2);
}
