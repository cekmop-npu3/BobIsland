# Tests

Tests use GoogleTest, downloaded through CMake `FetchContent` when the `debug`
preset is configured. Test source files are C++; production source remains C.

| File | Coverage |
| --- | --- |
| `stream_test.cpp` | UTF-8, raw byte transmission, queue bounds, and counters. |
| `config_test.cpp` | Fixed WinAPI DCB and timeout settings. |
| `serial_test.cpp` | Backend API validation and optional connected-port exchange. |
| `gui_test.cpp` | GUI configuration, Unicode input, output, and close lifecycle. |

Configure, build, and run the suite from an x64 Native Tools Command Prompt:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The connected-port test is skipped unless `TEST_PORT_A` and `TEST_PORT_B` contain
the numeric ends of a connected physical or virtual COM-port pair. GUI tests need
a Windows desktop session.

`create_virtual_ports.bat COM10 COM11` creates a virtual pair for this test when
the com0com driver is available.
