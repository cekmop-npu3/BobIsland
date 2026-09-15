# Backend

`backend.dll` owns all COM-port communication. Its public API is in
[`include/backend.h`](include/backend.h); applications use `serial_open`,
`serial_send`, `serial_get_status`, and `serial_close`.

`serial_open` opens `\\.\COM<number>` exclusively, configures 9600 baud, 8 data
bits, no parity, and the selected one or two stop bits. It then starts a reader
thread and a writer thread.

The writer takes Unicode characters from a bounded queue, encodes each one as
raw UTF-8, and writes its bytes one at a time. The transmitted count advances
only after all bytes of one character are written. The reader receives bytes,
reconstructs UTF-8 characters, and delivers them through the callback supplied
to `serial_open`.

`stream.c` implements UTF-8 encoding/decoding and the queue. `config_win32.c`
creates the WinAPI DCB and timeout settings. `serial_win32.c` contains port
enumeration, overlapped I/O, events, worker threads, and shutdown handling.

`backend_core` is an internal static library shared by the DLL and tests. Its
object files become part of `backend.dll`; client projects only need the exported
`com_app::backend` target.

After installation, consume the DLL with:

```cmake
find_package(backend CONFIG REQUIRED)
target_link_libraries(client PRIVATE com_app::backend)
```
