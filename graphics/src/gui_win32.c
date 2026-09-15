#include "gui.h"
#include <windows.h>
#include <commctrl.h>
#include <limits.h>

enum { ID_FIRST = 101, ID_SECOND, ID_INPUT, ID_OUTPUT, ID_STATUS,
       MSG_RECEIVED = WM_APP + 1 };

struct gui_t {
    const gui_options *options;
    HWND window, control_group, input_group, output_group, status_group;
    HWND selectors[2], input, output, status;
    HFONT font;
    unsigned high_surrogate;
    int configured, closed, display_error;
};

static int scalar_is_text(uint32_t scalar)
{
    return scalar == '\n' || (scalar >= 32 && scalar <= 0x10ffffu &&
        !(scalar >= 0x7f && scalar <= 0x9f) && !(scalar >= 0xd800 && scalar <= 0xdfff));
}

static int append_scalar(gui_t *gui, HWND edit, uint32_t scalar)
{
    wchar_t text[3] = {0};
    int before, units = 1;
    if (!scalar_is_text(scalar)) return 0;
    if (scalar == '\n') { text[0] = L'\r'; text[1] = L'\n'; units = 2; }
    else if (scalar <= 0xffff) text[0] = (wchar_t)scalar;
    else {
        scalar -= 0x10000;
        text[0] = (wchar_t)(0xd800u + (scalar >> 10));
        text[1] = (wchar_t)(0xdc00u + (scalar & 1023u));
        units = 2;
    }
    before = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, (WPARAM)before, (LPARAM)before);
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(edit, EM_SCROLLCARET, 0, 0);
    if (GetWindowTextLengthW(edit) != before + units) {
        if (!gui->display_error) {
            gui->display_error = 1;
            MessageBoxW(gui->window, L"The text window is full or Windows could not allocate memory. Close and restart the application.",
                        L"Display error", MB_OK | MB_ICONERROR);
        }
        return 0;
    }
    return 1;
}

static int accept_scalar(gui_t *gui, uint32_t scalar)
{
    if (!scalar_is_text(scalar)) return 1; /* Editing/control keys do not change sent text. */
    if (gui->options->on_character(gui->options->context, gui, scalar))
        return append_scalar(gui, gui->input, scalar);
    MessageBeep(MB_ICONWARNING);
    return 0;
}

static int accept_unit(gui_t *gui, unsigned unit)
{
    if (unit >= 0xd800 && unit <= 0xdbff) {
        gui->high_surrogate = unit; return 1;
    }
    if (unit >= 0xdc00 && unit <= 0xdfff) {
        unsigned high = gui->high_surrogate;
        gui->high_surrogate = 0;
        if (!high) return 0;
        return accept_scalar(gui, 0x10000u + ((high - 0xd800u) << 10) + unit - 0xdc00u);
    }
    gui->high_surrogate = 0;
    return accept_scalar(gui, unit == '\r' ? '\n' : unit);
}

static void paste_text(gui_t *gui)
{
    HANDLE data;
    const wchar_t *text;
    SIZE_T i, capacity;
    if (!OpenClipboard(gui->window)) return;
    data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        text = (const wchar_t *)GlobalLock(data);
        capacity = GlobalSize(data) / sizeof(wchar_t);
        if (text) {
            for (i = 0; i < capacity && text[i]; ++i) {
                if (!accept_unit(gui, text[i])) break;
                if (text[i] == '\r' && i + 1 < capacity && text[i + 1] == '\n') ++i;
            }
            gui->high_surrogate = 0;
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
}

static LRESULT CALLBACK input_proc(HWND window, UINT message, WPARAM wparam,
                                  LPARAM lparam, UINT_PTR id, DWORD_PTR data)
{
    gui_t *gui = (gui_t *)data;
    (void)id;
    switch (message) {
    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wparam == 'V') { paste_text(gui); return 0; }
        if ((GetKeyState(VK_SHIFT) & 0x8000) && wparam == VK_INSERT) { paste_text(gui); return 0; }
        break;
    case WM_CHAR:
        if (wparam == 3) return DefSubclassProc(window, message, wparam, lparam); /* Copy. */
        accept_unit(gui, (unsigned)wparam);
        return 0;
    case WM_UNICHAR:
        if (wparam == UNICODE_NOCHAR) return TRUE;
        gui->high_surrogate = 0;
        accept_scalar(gui, (uint32_t)wparam);
        return 0;
    case WM_PASTE: paste_text(gui); return 0;
    case WM_KILLFOCUS: gui->high_surrogate = 0; break;
    case WM_NCDESTROY: RemoveWindowSubclass(window, input_proc, 1); break;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

static HWND child(gui_t *gui, const wchar_t *class_name, const wchar_t *text,
                  DWORD style, int id)
{
    HWND window = CreateWindowExW(0, class_name, text, WS_CHILD | WS_VISIBLE | style,
        0, 0, 1, 1, gui->window, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (window) SendMessageW(window, WM_SETFONT, (WPARAM)gui->font, TRUE);
    return window;
}

static void layout(gui_t *gui, int width, int height)
{
    int half = (width - 36) / 2, text_height = height - 228;
    if (!gui->status) return;
    MoveWindow(gui->control_group, 12, 10, width - 24, 76, TRUE);
    MoveWindow(gui->selectors[0], 24, 38, half - 18, 260, TRUE);
    MoveWindow(gui->selectors[1], width / 2 + 6, 38, half - 18, 160, TRUE);
    MoveWindow(gui->input_group, 12, 98, half, text_height + 40, TRUE);
    MoveWindow(gui->output_group, 24 + half, 98, half, text_height + 40, TRUE);
    MoveWindow(gui->input, 24, 126, half - 24, text_height, TRUE);
    MoveWindow(gui->output, 36 + half, 126, half - 24, text_height, TRUE);
    MoveWindow(gui->status_group, 12, height - 80, width - 24, 68, TRUE);
    MoveWindow(gui->status, 24, height - 55, width - 48, 38, TRUE);
}

static void close_once(gui_t *gui)
{
    if (!gui->closed) {
        gui->closed = 1;
        if (gui->options->on_close) gui->options->on_close(gui->options->context);
    }
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    gui_t *gui = (gui_t *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        gui = (gui_t *)((CREATESTRUCTW *)lparam)->lpCreateParams;
        gui->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)gui);
    }
    if (!gui) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_CREATE: {
        size_t i, j;
        gui->font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        gui->control_group = child(gui, L"BUTTON", gui->options->control_caption, BS_GROUPBOX, 0);
        gui->input_group = child(gui, L"BUTTON", gui->options->input_caption, BS_GROUPBOX, 0);
        gui->output_group = child(gui, L"BUTTON", gui->options->output_caption, BS_GROUPBOX, 0);
        gui->status_group = child(gui, L"BUTTON", L"Status", BS_GROUPBOX, 0);
        for (i = 0; i < 2; ++i) {
            gui->selectors[i] = child(gui, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ID_FIRST + (int)i);
            if (!gui->selectors[i]) return -1;
            if (SendMessageW(gui->selectors[i], CB_ADDSTRING, 0, (LPARAM)gui->options->selector_prompts[i]) < 0) return -1;
            for (j = 0; j < gui->options->choice_counts[i]; ++j)
                if (SendMessageW(gui->selectors[i], CB_ADDSTRING, 0, (LPARAM)gui->options->choices[i][j]) < 0) return -1;
            SendMessageW(gui->selectors[i], CB_SETCURSEL, 0, 0);
        }
        gui->input = child(gui, L"EDIT", L"", WS_BORDER | WS_VSCROLL | WS_TABSTOP |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_READONLY, ID_INPUT);
        gui->output = child(gui, L"EDIT", L"", WS_BORDER | WS_VSCROLL | WS_TABSTOP |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, ID_OUTPUT);
        gui->status = child(gui, L"STATIC", gui->options->initial_status, SS_LEFT, ID_STATUS);
        if (!gui->control_group || !gui->input_group || !gui->output_group || !gui->status_group ||
            !gui->input || !gui->output || !gui->status) return -1;
        SendMessageW(gui->input, EM_SETLIMITTEXT, INT_MAX - 1, 0);
        SendMessageW(gui->output, EM_SETLIMITTEXT, INT_MAX - 1, 0);
        if (!SetWindowSubclass(gui->input, input_proc, 1, (DWORD_PTR)gui)) return -1;
        if (!SetTimer(window, 1, 250, NULL)) return -1;
        return 0;
    }
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lparam)->ptMinTrackSize.x = 640;
        ((MINMAXINFO *)lparam)->ptMinTrackSize.y = 440;
        return 0;
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) layout(gui, LOWORD(lparam), HIWORD(lparam));
        return 0;
    case WM_COMMAND:
        if (!gui->configured && HIWORD(wparam) == CBN_SELCHANGE &&
            (LOWORD(wparam) == ID_FIRST || LOWORD(wparam) == ID_SECOND)) {
            LRESULT first = SendMessageW(gui->selectors[0], CB_GETCURSEL, 0, 0);
            LRESULT second = SendMessageW(gui->selectors[1], CB_GETCURSEL, 0, 0);
            if (first > 0 && second > 0 && gui->options->on_configure(
                gui->options->context, gui, (size_t)first - 1, (size_t)second - 1)) {
                gui->configured = 1;
                EnableWindow(gui->selectors[0], FALSE);
                EnableWindow(gui->selectors[1], FALSE);
                SetFocus(gui->input);
            }
        }
        return 0;
    case WM_TIMER:
        if (gui->options->on_tick) gui->options->on_tick(gui->options->context, gui);
        return 0;
    case MSG_RECEIVED: append_scalar(gui, gui->output, (uint32_t)wparam); return 0;
    case WM_SETFOCUS: SetFocus(gui->input); return 0;
    case WM_CLOSE:
        KillTimer(window, 1);
        close_once(gui);
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        close_once(gui);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int gui_run(const gui_options *options, int show_command)
{
    static const wchar_t class_name[] = L"TextExchangeWindow";
    gui_t gui = {0};
    WNDCLASSEXW window_class = {0};
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES};
    MSG message;
    BOOL result;
    int success = 1;
    if (!options || !options->on_configure || !options->on_character) return 0;
    gui.options = options;
    if (!InitCommonControlsEx(&controls)) return 0;
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleW(NULL);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszClassName = class_name;
    if (!RegisterClassExW(&window_class)) return 0;
    if (!CreateWindowExW(WS_EX_CONTROLPARENT, class_name, options->title,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 900, 590,
        NULL, NULL, window_class.hInstance, &gui)) {
        close_once(&gui);
        UnregisterClassW(class_name, window_class.hInstance);
        return 0;
    }
    ShowWindow(gui.window, show_command);
    UpdateWindow(gui.window);
    while ((result = GetMessageW(&message, NULL, 0, 0)) > 0) {
        /* Leave Enter to WM_CHAR; use dialog navigation only for Tab. */
        if (message.message == WM_KEYDOWN && message.wParam == VK_TAB &&
            IsDialogMessageW(gui.window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (result < 0) success = 0;
    close_once(&gui);
    if (IsWindow(gui.window)) DestroyWindow(gui.window);
    UnregisterClassW(class_name, window_class.hInstance);
    return success;
}

void gui_set_status(gui_t *gui, const wchar_t *text)
{
    if (gui && text) SetWindowTextW(gui->status, text);
}

int gui_post_character(gui_t *gui, uint32_t scalar)
{
    return gui && scalar_is_text(scalar) && PostMessageW(gui->window, MSG_RECEIVED, scalar, 0);
}
