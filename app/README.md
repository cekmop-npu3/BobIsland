# Application

`com_app.exe` is the executable entry point. It links `backend.dll` and
`graphics.dll`, then connects their callback APIs in [`src/main.c`](src/main.c).

When the user selects both configuration values, the application opens the
chosen COM port through `serial_open`. Characters entered in the GUI are sent by
`serial_send`. Received characters are posted back to the GUI output pane.

The status callback reads the backend counter and error state every 250 ms. On
window close, the application calls `serial_close`, which stops both communication
threads and releases the COM-port handle.

The manifest provides Windows application metadata. Build this directory through
the root CMake project; it is not intended to be configured separately.
