#include "backend.h"
#include "gui.h"
#include <windows.h>
#include <stdlib.h>
#include <wchar.h>

typedef struct application {
    serial_t *serial;
    gui_t *gui;
    unsigned *ports;
    unsigned port, stop_bits;
    unsigned long input_error;
} application;

static const wchar_t *error_text(unsigned long error)
{
    /* Deliberately English even on a Russian Windows installation. */
    switch (error) {
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: return L"The selected COM port is no longer available.";
    case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: return L"The COM port is busy or access was denied.";
    case ERROR_INVALID_PARAMETER: return L"The device does not support these settings, or the input is invalid.";
    case ERROR_NOT_ENOUGH_MEMORY: return L"Windows could not allocate memory.";
    case ERROR_NOT_ENOUGH_QUOTA: return L"The input or display queue is full. The last input was not accepted.";
    case ERROR_NO_UNICODE_TRANSLATION: return L"Invalid text received. Check that both stations use this app and matching settings.";
    case ERROR_TIMEOUT: case ERROR_SEM_TIMEOUT: return L"The COM port write timed out.";
    case ERROR_IO_DEVICE: return L"Serial communication failed. Check the cable and matching settings.";
    case ERROR_DEVICE_NOT_CONNECTED: case ERROR_GEN_FAILURE: return L"The serial device is disconnected or unavailable.";
    case ERROR_OPERATION_ABORTED: return L"Communication has stopped.";
    default: return L"Windows reported a communication error.";
    }
}

static void tick(void *context, gui_t *gui)
{
    application *app = (application *)context;
    wchar_t text[512];
    serial_status status;
    unsigned long error;
    if (!app->serial) return;
    status = serial_get_status(app->serial);
    error = status.error ? status.error : app->input_error;
    if (error) {
        swprintf(text, 512, L"Transmitted characters: %llu. Error %lu: %ls%ls",
            (unsigned long long)status.transmitted, error, error_text(error),
            status.error ? L" Restart the application to reconnect." : L"");
    } else {
        swprintf(text, 512, L"COM%u | 9600 baud, 8 data bits, no parity, %u stop bit(s).\nTransmitted characters: %llu",
            app->port, app->stop_bits, (unsigned long long)status.transmitted);
    }
    gui_set_status(gui, text);
}

static int received(void *context, uint32_t scalar)
{
    application *app = (application *)context;
    return gui_post_character(app->gui, scalar);
}

static int configure(void *context, gui_t *gui, size_t port_index, size_t stop_index)
{
    application *app = (application *)context;
    unsigned long error;
    wchar_t text[512];
    app->gui = gui;
    app->port = app->ports[port_index];
    app->stop_bits = stop_index == 0 ? 1 : 2;
    app->serial = serial_open(app->port, app->stop_bits, received, app, &error);
    if (!app->serial) {
        swprintf(text, 512, L"Transmitted characters: 0. Error %lu: %ls Select the port/settings again to retry.", error, error_text(error));
        gui_set_status(gui, text);
        return 0;
    }
    app->input_error = 0;
    tick(app, gui);
    return 1;
}

static int character(void *context, gui_t *gui, uint32_t scalar)
{
    application *app = (application *)context;
    if (!app->serial) {
        gui_set_status(gui, L"Transmitted characters: 0. Select a COM port and stop bits before typing.");
        return 0;
    }
    if (!serial_send(app->serial, scalar, &app->input_error)) { tick(app, gui); return 0; }
    return 1;
}

static void closed(void *context)
{
    application *app = (application *)context;
    serial_close(app->serial);
    app->serial = NULL;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    application app = {0};
    gui_options options = {0};
    const wchar_t *stops[] = { L"1 stop bit", L"2 stop bits" };
    wchar_t **port_names = NULL;
    wchar_t initial[512];
    size_t count, i, allocated = 0;
    unsigned long error = 0;
    int result = 1;
    (void)instance; (void)previous; (void)command_line;
    count = serial_ports(NULL, 0, &error);
    if (count == SIZE_MAX) goto startup_error;
    if (count) {
        app.ports = (unsigned *)calloc(count, sizeof(*app.ports));
        port_names = (wchar_t **)calloc(count, sizeof(*port_names));
        if (!app.ports || !port_names) { error = ERROR_NOT_ENOUGH_MEMORY; goto startup_error; }
        count = serial_ports(app.ports, count, &error);
        if (count == SIZE_MAX) goto startup_error;
        for (i = 0; i < count; ++i) {
            port_names[i] = (wchar_t *)calloc(16, sizeof(wchar_t));
            if (!port_names[i]) { error = ERROR_NOT_ENOUGH_MEMORY; goto startup_error; }
            ++allocated;
            swprintf(port_names[i], 16, L"COM%u", app.ports[i]);
        }
    }
    options.title = L"COM communication - Laboratory 1, variant 3";
    options.control_caption = L"Control: COM port / Stop bits (select once)";
    options.selector_prompts[0] = count ? L"Choose COM port" : L"No COM ports found";
    options.selector_prompts[1] = L"Choose stop bits";
    options.choices[0] = (const wchar_t *const *)port_names;
    options.choices[1] = stops;
    options.choice_counts[0] = count;
    options.choice_counts[1] = 2;
    options.input_caption = L"Input - transmitted as you type";
    options.output_caption = L"Output - received text";
    options.initial_status = count ? L"Transmitted characters: 0. Choose a port and stop bits to initialize.\nBoth computers must use the same stop-bit setting."
        : L"Transmitted characters: 0. No COM ports found. Connect the adapter, then restart the application.";
    options.context = &app;
    options.on_configure = configure;
    options.on_character = character;
    options.on_tick = tick;
    options.on_close = closed;
    if (!gui_run(&options, show_command)) {
        MessageBoxW(NULL, L"Windows could not create or run the application window.", L"Application error", MB_OK | MB_ICONERROR);
    } else result = 0;
    goto cleanup;
startup_error:
    swprintf(initial, 512, L"Error %lu: %ls", error, error_text(error));
    MessageBoxW(NULL, initial, L"Startup error", MB_OK | MB_ICONERROR);
cleanup:
    for (i = 0; i < allocated; ++i) free(port_names[i]);
    free(port_names);
    free(app.ports);
    return result;
}
