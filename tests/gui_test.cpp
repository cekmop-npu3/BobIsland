#include "gui.h"
#include <windows.h>
#include <gtest/gtest.h>
#include <vector>
#include <string>

struct GuiScenario {
    int phase = 0, configured = 0, closed = 0;
    std::vector<uint32_t> entered;
    static int configure(void *context, gui_t *, size_t first, size_t second) {
        auto self = static_cast<GuiScenario *>(context);
        EXPECT_EQ(first, 0u); EXPECT_EQ(second, 0u);
        ++self->configured;
        return 1;
    }
    static int character(void *context, gui_t *, uint32_t scalar) {
        static_cast<GuiScenario *>(context)->entered.push_back(scalar);
        return 1;
    }
    static void close(void *context) { ++static_cast<GuiScenario *>(context)->closed; }
    static void tick(void *context, gui_t *gui) {
        auto self = static_cast<GuiScenario *>(context);
        HWND window = FindWindowW(L"TextExchangeWindow", L"Graphics test");
        if (!window) { ADD_FAILURE() << "Window was not created"; PostQuitMessage(1); return; }
        HWND first = GetDlgItem(window, 101), second = GetDlgItem(window, 102);
        HWND input = GetDlgItem(window, 103), output = GetDlgItem(window, 104);
        if (self->phase++ == 0) {
            EXPECT_NE(first, nullptr); EXPECT_NE(second, nullptr);
            SendMessageW(first, CB_SETCURSEL, 1, 0);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(101, CBN_SELCHANGE), reinterpret_cast<LPARAM>(first));
            EXPECT_EQ(self->configured, 0); // Both choices are required.
            SendMessageW(second, CB_SETCURSEL, 1, 0);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(102, CBN_SELCHANGE), reinterpret_cast<LPARAM>(second));
            EXPECT_EQ(self->configured, 1);
            EXPECT_FALSE(IsWindowEnabled(first)); EXPECT_FALSE(IsWindowEnabled(second));
            for (auto unit : {0x41, 0x42f, 0xd83d, 0xde42, 13, 8}) SendMessageW(input, WM_CHAR, unit, 1);
            for (auto scalar : {0x41u, 0x42fu, 0x1f642u, 10u}) EXPECT_TRUE(gui_post_character(gui, scalar));
            gui_set_status(gui, L"Transmitted characters: 4");
        } else {
            wchar_t input_text[32]{}, output_text[32]{}, status[64]{};
            GetWindowTextW(input, input_text, 32); GetWindowTextW(output, output_text, 32);
            GetWindowTextW(GetDlgItem(window, 105), status, 64);
            EXPECT_EQ(std::wstring(input_text), L"A\u042f\U0001f642\r\n");
            EXPECT_EQ(std::wstring(output_text), std::wstring(input_text));
            EXPECT_EQ(std::wstring(status), L"Transmitted characters: 4");
            SendMessageW(window, WM_CLOSE, 0, 0);
        }
    }
};

TEST(Graphics, SelectOnceTypeUnicodeReceiveAndClose)
{
    GuiScenario scenario;
    const wchar_t *choices[] = {L"Example"};
    gui_options options{};
    options.title = L"Graphics test";
    options.control_caption = L"Control";
    options.selector_prompts[0] = L"First"; options.selector_prompts[1] = L"Second";
    options.choices[0] = choices; options.choices[1] = choices;
    options.choice_counts[0] = 1; options.choice_counts[1] = 1;
    options.input_caption = L"Input"; options.output_caption = L"Output";
    options.initial_status = L"Ready";
    options.context = &scenario;
    options.on_configure = GuiScenario::configure; options.on_character = GuiScenario::character;
    options.on_tick = GuiScenario::tick; options.on_close = GuiScenario::close;
    EXPECT_TRUE(gui_run(&options, SW_HIDE));
    EXPECT_EQ(scenario.entered, (std::vector<uint32_t>{'A', 0x42f, 0x1f642, '\n'}));
    EXPECT_EQ(scenario.closed, 1);
}
