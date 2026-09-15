# COM communication application

A Windows desktop application for laboratory variant 3. Two computers exchange
text through connected COM ports. The user selects a COM port and either one or
two stop bits once; the application uses fixed 9600 baud, 8 data bits, and no
parity. Every typed printable character and Enter are sent immediately.

The application is written in C and uses WinAPI for both the graphical interface
and serial communication. GoogleTest-based tests use C++17.

## Requirements

Use Windows with Visual Studio's Desktop development with C++ workload, the
Windows SDK, CMake 3.24 or newer, and Ninja. Run CMake from an x64 Native Tools
Command Prompt so `cl.exe`, `rc.exe`, and the Windows SDK are available.

## Configure and build

On a new Windows computer, run `setup.bat` from an elevated Command Prompt. It
uses winget for MSVC Build Tools, CMake, Ninja, and LLVM/clangd. If winget is
unavailable or an installation fails, it downloads the official installer or ZIP
with curl. It activates the MSVC environment for that session, configures the
Debug preset, builds the project, downloads GoogleTest, and creates the
compilation database.

`build` creates Release application binaries only:

```sh
cmake --preset build
cmake --build --preset build
```

`debug` creates a Debug build, downloads GoogleTest, enables tests, and writes
`build/debug/compile_commands.json` for clangd and other IDE tools:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The application executable is `com_app.exe`. Keep it with `backend.dll` and
`graphics.dll` when copying it to another computer.

## Layout

| Directory | Purpose |
| --- | --- |
| `backend/` | Serial-port API, UTF-8 byte stream, queue, and worker threads. |
| `graphics/` | Independent WinAPI GUI DLL. |
| `app/` | Executable that connects the GUI callbacks to the backend. |
| `tests/` | GoogleTest targets for stream, configuration, serial API, and GUI. |

`project_options` is an interface library. It shares C11/C++17 requirements,
Windows definitions, and MSVC warnings without producing a binary.

`backend_core` is a private static support library. Its object code is linked
into `backend.dll`. The executable links `backend.dll` and `graphics.dll`.

## Installed libraries

The backend and graphics DLLs export independent CMake packages. After running
`cmake --install build/build --prefix <install-prefix>`, another project can use:

```cmake
find_package(backend CONFIG REQUIRED)
find_package(graphics CONFIG REQUIRED)

target_link_libraries(client PRIVATE com_app::backend com_app::graphics)
```

Pass `<install-prefix>` through `CMAKE_PREFIX_PATH` when configuring the client.

See the README in each component directory for its API and implementation notes.
