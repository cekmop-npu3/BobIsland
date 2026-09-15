#ifndef BACKEND_H
#define BACKEND_H
#include <stddef.h>
#include <stdint.h>

#if defined(BACKEND_BUILD)
# define BACKEND_API __declspec(dllexport)
#else
# define BACKEND_API __declspec(dllimport)
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct serial_t serial_t;
/* Called by the receive thread; return zero if delivery fails. Never call close here. */
typedef int (*receive_fn)(void *context, uint32_t scalar);
typedef struct serial_status {
    uint64_t transmitted;
    size_t pending;
    unsigned long error;
    int running;
} serial_status;

/* Query actual DOS COM device names. Returns count, or SIZE_MAX on failure.
 * Pass NULL/zero first to obtain capacity. No ports are opened by enumeration. */
BACKEND_API size_t serial_ports(unsigned *ports, size_t capacity, unsigned long *error);
/* A successful open fixes the configuration for this object's entire lifetime. */
BACKEND_API serial_t *serial_open(unsigned port, unsigned stop_bits,
    receive_fn receive, void *context, unsigned long *error);
/* UI-thread calls; enqueue one scalar, without blocking on the device. */
BACKEND_API int serial_send(serial_t *serial, uint32_t scalar, unsigned long *error);
BACKEND_API serial_status serial_get_status(serial_t *serial);
/* Stops/cancels both workers, joins them, then releases storage. Pending input is discarded. */
BACKEND_API void serial_close(serial_t *serial);

#ifdef __cplusplus
}
#endif
#endif
