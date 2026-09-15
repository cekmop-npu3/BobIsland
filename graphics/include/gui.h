#ifndef GUI_H
#define GUI_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#ifdef GRAPHICS_BUILD
# define GUI_API __declspec(dllexport)
#else
# define GUI_API __declspec(dllimport)
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct gui_t gui_t;
typedef struct gui_options {
    const wchar_t *title;
    const wchar_t *control_caption;
    const wchar_t *selector_prompts[2];
    const wchar_t *const *choices[2];
    size_t choice_counts[2];
    const wchar_t *input_caption;
    const wchar_t *output_caption;
    const wchar_t *initial_status;
    void *context;
    /* Indices are zero based. On success both selectors are permanently locked. */
    int (*on_configure)(void *context, gui_t *gui, size_t first, size_t second);
    /* Return nonzero to append the entered Unicode scalar to the input history. */
    int (*on_character)(void *context, gui_t *gui, uint32_t scalar);
    void (*on_tick)(void *context, gui_t *gui);
    /* Called once before destroying the window; join producers here. */
    void (*on_close)(void *context);
} gui_options;

/* Blocking UI loop. Options and strings must remain valid until return. */
GUI_API int gui_run(const gui_options *options, int show_command);
/* UI thread only. */
GUI_API void gui_set_status(gui_t *gui, const wchar_t *text);
/* Any thread, until on_close returns. Posts a scalar for immediate UI delivery. */
GUI_API int gui_post_character(gui_t *gui, uint32_t scalar);

#ifdef __cplusplus
}
#endif
#endif
