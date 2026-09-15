#include "backend.h"
#include "stream.h"
#include "config_win32.h"
#include <stdlib.h>
#include <wchar.h>

struct serial_t {
    HANDLE port, stop, ready, reader, writer;
    OVERLAPPED read_io, write_io;
    CRITICAL_SECTION lock;
    queue_t queue;
    uint64_t transmitted;
    DWORD error;
    receive_fn receive;
    void *context;
};

static void set_error(unsigned long *output, DWORD error)
{
    if (output) *output = error;
}

static int compare_ports(const void *a, const void *b)
{
    unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
    return (x > y) - (x < y);
}

size_t serial_ports(unsigned *ports, size_t capacity, unsigned long *error)
{
    DWORD size = 4096, result;
    wchar_t *names, *name;
    size_t count = 0;
    if (!ports && capacity) { set_error(error, ERROR_INVALID_PARAMETER); return SIZE_MAX; }
    for (;;) {
        names = (wchar_t *)malloc(size * sizeof(*names));
        if (!names) { set_error(error, ERROR_NOT_ENOUGH_MEMORY); return SIZE_MAX; }
        result = QueryDosDeviceW(NULL, names, size);
        if (result) break;
        { DWORD code = GetLastError();
          free(names);
          if (code != ERROR_INSUFFICIENT_BUFFER || size >= 1048576) {
              set_error(error, code); return SIZE_MAX;
          }
        }
        size *= 2;
    }
    for (name = names; *name; name += wcslen(name) + 1) {
        wchar_t *end;
        unsigned long number;
        if (wcsncmp(name, L"COM", 3) || name[3] < L'1' || name[3] > L'9') continue;
        number = wcstoul(name + 3, &end, 10);
        if (*end || number > 65535) continue;
        if (count < capacity) ports[count] = (unsigned)number;
        ++count;
    }
    free(names);
    if (ports && count > capacity) {
        set_error(error, ERROR_INSUFFICIENT_BUFFER); return SIZE_MAX;
    }
    if (ports && count) qsort(ports, count, sizeof(*ports), compare_ports);
    set_error(error, ERROR_SUCCESS);
    return count;
}

static void fail(serial_t *serial, DWORD error)
{
    EnterCriticalSection(&serial->lock);
    if (!serial->error) serial->error = error ? error : ERROR_GEN_FAILURE;
    LeaveCriticalSection(&serial->lock);
    SetEvent(serial->stop);
}

/* OVERLAPPED and its byte buffer remain alive until cancellation completes. */
static int transfer(serial_t *serial, int writing, unsigned char *byte, DWORD *count)
{
    OVERLAPPED *io = writing ? &serial->write_io : &serial->read_io;
    HANDLE events[2] = { serial->stop, io->hEvent };
    BOOL ok;
    DWORD wait_result, code;
    if (WaitForSingleObject(serial->stop, 0) != WAIT_TIMEOUT) {
        SetLastError(ERROR_OPERATION_ABORTED); return 0;
    }
    ResetEvent(io->hEvent);
    ok = writing ? WriteFile(serial->port, byte, 1, count, io)
                 : ReadFile(serial->port, byte, 1, count, io);
    if (ok) return 1;
    if (GetLastError() != ERROR_IO_PENDING) return 0;
    wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    if (wait_result == WAIT_OBJECT_0 + 1)
        return GetOverlappedResult(serial->port, io, count, FALSE) != 0;
    code = wait_result == WAIT_FAILED ? GetLastError() : ERROR_OPERATION_ABORTED;
    CancelIoEx(serial->port, io);
    /* Joining the request is required even if CancelIoEx reports ERROR_NOT_FOUND. */
    GetOverlappedResult(serial->port, io, count, TRUE);
    SetLastError(code);
    return 0;
}

static int write_byte(void *context, unsigned char byte)
{
    serial_t *serial = (serial_t *)context;
    DWORD count = 0;
    if (!transfer(serial, 1, &byte, &count)) return 0;
    if (count != 1) { SetLastError(ERROR_TIMEOUT); return 0; }
    return 1;
}

static DWORD WINAPI writer_main(void *context)
{
    serial_t *serial = (serial_t *)context;
    HANDLE events[2] = { serial->stop, serial->ready };
    uint64_t completed = 0;
    for (;;) {
        uint32_t scalar;
        int available;
        DWORD result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (result == WAIT_OBJECT_0) break;
        if (result != WAIT_OBJECT_0 + 1) { fail(serial, GetLastError()); break; }
        EnterCriticalSection(&serial->lock);
        available = queue_pop(&serial->queue, &scalar);
        if (!serial->queue.count) ResetEvent(serial->ready);
        LeaveCriticalSection(&serial->lock);
        if (!available) continue;
        if (!write_character(scalar, write_byte, serial, &completed)) {
            if (WaitForSingleObject(serial->stop, 0) == WAIT_TIMEOUT) fail(serial, GetLastError());
            break;
        }
        EnterCriticalSection(&serial->lock);
        serial->transmitted = completed;
        LeaveCriticalSection(&serial->lock);
    }
    return 0;
}

static DWORD WINAPI reader_main(void *context)
{
    serial_t *serial = (serial_t *)context;
    decoder_t decoder = {0};
    while (WaitForSingleObject(serial->stop, 0) == WAIT_TIMEOUT) {
        unsigned char byte = 0;
        DWORD count = 0, errors = 0;
        uint32_t scalar;
        int decoded;
        if (!transfer(serial, 0, &byte, &count)) {
            if (WaitForSingleObject(serial->stop, 0) == WAIT_TIMEOUT) fail(serial, GetLastError());
            break;
        }
        if (!ClearCommError(serial->port, &errors, NULL)) { fail(serial, GetLastError()); break; }
        if (errors) { fail(serial, ERROR_IO_DEVICE); break; }
        if (!count) continue;
        decoded = utf8_feed(&decoder, byte, &scalar);
        if (decoded < 0 || (decoded > 0 && !is_text(scalar))) {
            fail(serial, ERROR_NO_UNICODE_TRANSLATION); break;
        }
        if (decoded > 0 && !serial->receive(serial->context, scalar)) {
            fail(serial, ERROR_NOT_ENOUGH_QUOTA); break;
        }
    }
    return 0;
}

serial_t *serial_open(unsigned port, unsigned stop_bits,
    receive_fn receive, void *context, unsigned long *error)
{
    serial_t *serial;
    wchar_t path[32];
    DCB dcb;
    COMMTIMEOUTS timeouts;
    DWORD code;
    if (!valid_config(port, stop_bits) || !receive) {
        set_error(error, ERROR_INVALID_PARAMETER); return NULL;
    }
    serial = (serial_t *)calloc(1, sizeof(*serial));
    if (!serial) { set_error(error, ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    serial->port = INVALID_HANDLE_VALUE;
    if (!InitializeCriticalSectionEx(&serial->lock, 0, 0)) {
        code = GetLastError(); free(serial); set_error(error, code); return NULL;
    }
    serial->receive = receive;
    serial->context = context;
    swprintf(path, 32, L"\\\\.\\COM%u", port);
    serial->port = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (serial->port == INVALID_HANDLE_VALUE) goto failure;
    make_dcb(stop_bits, &dcb);
    make_timeouts(&timeouts);
    if (!SetupComm(serial->port, 4096, 4096) || !SetCommState(serial->port, &dcb) ||
        !SetCommTimeouts(serial->port, &timeouts)) goto failure;
    serial->stop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!serial->stop) goto failure;
    serial->ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!serial->ready) goto failure;
    serial->read_io.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!serial->read_io.hEvent) goto failure;
    serial->write_io.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!serial->write_io.hEvent) goto failure;
    serial->reader = CreateThread(NULL, 0, reader_main, serial, 0, NULL);
    if (!serial->reader) goto failure;
    serial->writer = CreateThread(NULL, 0, writer_main, serial, 0, NULL);
    if (!serial->writer) goto failure;
    set_error(error, ERROR_SUCCESS);
    return serial;
failure:
    code = GetLastError();
    serial_close(serial);
    set_error(error, code);
    return NULL;
}

int serial_send(serial_t *serial, uint32_t scalar, unsigned long *error)
{
    DWORD code = ERROR_SUCCESS;
    if (!serial || !is_text(scalar)) { set_error(error, ERROR_INVALID_PARAMETER); return 0; }
    EnterCriticalSection(&serial->lock);
    if (WaitForSingleObject(serial->stop, 0) != WAIT_TIMEOUT)
        code = serial->error ? serial->error : ERROR_OPERATION_ABORTED;
    else if (!queue_push(&serial->queue, scalar)) code = ERROR_NOT_ENOUGH_QUOTA;
    else SetEvent(serial->ready);
    LeaveCriticalSection(&serial->lock);
    set_error(error, code);
    return code == ERROR_SUCCESS;
}

serial_status serial_get_status(serial_t *serial)
{
    serial_status status = {0};
    if (!serial) return status;
    EnterCriticalSection(&serial->lock);
    status.transmitted = serial->transmitted;
    status.pending = serial->queue.count;
    status.error = serial->error;
    status.running = WaitForSingleObject(serial->stop, 0) == WAIT_TIMEOUT;
    LeaveCriticalSection(&serial->lock);
    return status;
}

void serial_close(serial_t *serial)
{
    if (!serial) return;
    if (serial->stop) SetEvent(serial->stop);
    if (serial->reader) { WaitForSingleObject(serial->reader, INFINITE); CloseHandle(serial->reader); }
    if (serial->writer) { WaitForSingleObject(serial->writer, INFINITE); CloseHandle(serial->writer); }
    if (serial->port != INVALID_HANDLE_VALUE) CloseHandle(serial->port);
    if (serial->read_io.hEvent) CloseHandle(serial->read_io.hEvent);
    if (serial->write_io.hEvent) CloseHandle(serial->write_io.hEvent);
    if (serial->ready) CloseHandle(serial->ready);
    if (serial->stop) CloseHandle(serial->stop);
    DeleteCriticalSection(&serial->lock);
    free(serial);
}
