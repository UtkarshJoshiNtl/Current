/*
 * gui.h - platform GUI entry point.
 *
 * Two implementations share this interface:
 *   - src/viz.c       SDL2 window  (Linux / macOS)
 *   - src/win32_ui.c  native Win32 controls + GDI (Windows)
 *
 * Both drive the same Sim objects and the same reset callback.
 */
#ifndef GUI_H
#define GUI_H

#include "sim.h"

typedef struct {
    const char *scene_name;
    double      budget_ms;   /* per-panel compute budget per frame */
    int         max_frames;  /* 0 = run until closed */
    int         vsync;       /* SDL only */
    const char *shot_path;   /* SDL only: save a BMP of the final frame */
    void      (*on_reset)(void *ud);
    void       *reset_ud;
} GuiOptions;

/* Returns 0 on clean exit, non-zero if the UI could not be created. */
int gui_run(Sim *left, Sim *right, const GuiOptions *opt);

#endif /* GUI_H */
