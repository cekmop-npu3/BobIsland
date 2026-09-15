# Graphics

`graphics.dll` provides the WinAPI graphical interface and has no dependency on
the COM backend. Its public API is in [`include/gui.h`](include/gui.h).

The application supplies `gui_options`: captions, two selector choice lists, and
callbacks. `gui_run` creates the main window and its control, input, output, and
status areas. Once configuration succeeds, the two selectors are locked.

Typing and pasted text are converted to Unicode scalars and passed to the
`on_character` callback. `gui_post_character` safely posts received characters
from another thread to the output pane. `gui_set_status` updates the status text
from the UI thread.

`src/gui_win32.c` contains the window procedure, native controls, Unicode input
handling, and layout code. It uses only WinAPI GUI libraries.

After installation, consume the DLL with:

```cmake
find_package(graphics CONFIG REQUIRED)
target_link_libraries(client PRIVATE com_app::graphics)
```
