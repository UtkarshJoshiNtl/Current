/*
 * win32_ui.c - native Windows front-end (authentic Win32 controls + GDI).
 *
 * Instead of imitating Windows chrome in SDL, this uses the real thing:
 * genuine BUTTON / STATIC / STATUS BAR controls and the system font, with a
 * GDI-rendered space view inside a sunken (WS_EX_CLIENTEDGE) child window.
 * Built only by build.bat on Windows; the whole file compiles to nothing
 * elsewhere so a stray glob can never pull window headers into the Linux build.
 */
#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui.h"

#define TIMER_ID   1
#define ID_START   1001
#define ID_STOP    1002
#define ID_RESET   1003
#define ID_STATUS  1004
#define ID_ENGL    1010
#define ID_ENGR    1011
#define ID_ROC     1020
#define ID_CFG     1100   /* ID_CFG + slot*2 (+0 minus, +1 plus) */

#define N_CFG      4      /* dt, thr, mu, cd  */

#define SCALE_PX   112.0
#define EARTH_PX   16
#define DRAW_CAP   32768
#define PITCH_     0.34

/* ---- geometry ------------------------------------------------------------ */

typedef struct { double x, y, z; } D3;
typedef struct { double px, py, depth; Object *o; } DrawObj;

static double g_yaw = 0.0;

static void project(D3 p, double cx, double cy, double yaw,
                    double *sx, double *sy, double *depth)
{
    double cyw = cos(yaw), syw = sin(yaw);
    double cp = cos(PITCH_), sp = sin(PITCH_);

    double x1 =  p.x * cyw + p.z * syw;
    double z1 = -p.x * syw + p.z * cyw;
    double y1 =  p.y;

    double y2 = y1 * cp - z1 * sp;
    double z2 = y1 * sp + z1 * cp;

    *sx = cx + x1 * SCALE_PX;
    *sy = cy - y2 * SCALE_PX;
    *depth = z2;
}

static int cmp_far(const void *A, const void *B)
{
    const DrawObj *a = (const DrawObj *)A, *b = (const DrawObj *)B;
    return (a->depth > b->depth) - (a->depth < b->depth);
}

/* ---- application state --------------------------------------------------- */

typedef struct {
    Sim              *sim[2];
    const GuiOptions *opt;
    HWND              hStart, hStop, hReset;
    HWND              hEng[2];          /* engine combo boxes */
    HWND              hCfgMinus[N_CFG];
    HWND              hCfgLabel[N_CFG];
    HWND              hCfgPlus[N_CFG];
    HWND              hRoc;
    HWND              hCap[2];
    HWND              hView[2];
    HWND              hStat[2][3];
    HWND              hStatus;
    int               paused;
    int               frames;
} App;

typedef struct { App *app; int idx; } ViewCtx;

static App g_app;

static HFONT  g_fontBold = NULL;
static HBRUSH g_brQuiet = NULL, g_brConj = NULL, g_brColl = NULL;
static HBRUSH g_brEarth[EARTH_PX + 1];
static HPEN   g_penRed = NULL, g_penYel = NULL;

static ULONGLONG ms_now(void) { return GetTickCount64(); }

/* ---- GDI resources ------------------------------------------------------- */

static void gdi_init(void)
{
    if (g_brQuiet) return;

    g_brQuiet = CreateSolidBrush(RGB(150, 170, 190));
    g_brConj  = CreateSolidBrush(RGB(255,  72,  72));
    g_brColl  = CreateSolidBrush(RGB(255, 215,  40));

    static const int core[3] = { 12,  46,  92 };
    static const int edge[3] = { 34, 190, 128 };
    for (int r = 1; r <= EARTH_PX; r++) {
        double t = (double)r / EARTH_PX;
        int cr = (int)(core[0] + (edge[0] - core[0]) * t);
        int cg = (int)(core[1] + (edge[1] - core[1]) * t);
        int cb = (int)(core[2] + (edge[2] - core[2]) * t);
        g_brEarth[r] = CreateSolidBrush(RGB(cr, cg, cb));
    }

    g_penRed = CreatePen(PS_SOLID, 1, RGB(245,  90,  90));
    g_penYel = CreatePen(PS_SOLID, 1, RGB(255, 215,  40));

    NONCLIENTMETRICSA ncm;
    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0)) {
        ncm.lfMessageFont.lfWeight = FW_BOLD;
        g_fontBold = CreateFontIndirectA(&ncm.lfMessageFont);
    }
}

static void gdi_done(void)
{
    if (g_brQuiet) { DeleteObject(g_brQuiet); g_brQuiet = NULL; }
    if (g_brConj)  { DeleteObject(g_brConj);  g_brConj  = NULL; }
    if (g_brColl)  { DeleteObject(g_brColl);  g_brColl  = NULL; }
    for (int r = 1; r <= EARTH_PX; r++)
        if (g_brEarth[r]) { DeleteObject(g_brEarth[r]); g_brEarth[r] = NULL; }
    if (g_penRed)  { DeleteObject(g_penRed);  g_penRed  = NULL; }
    if (g_penYel)  { DeleteObject(g_penYel);  g_penYel  = NULL; }
    if (g_fontBold){ DeleteObject(g_fontBold); g_fontBold = NULL; }
}

static void fill_ellipse(HDC dc, int cx, int cy, int rad, HBRUSH br)
{
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, cx - rad, cy - rad, cx + rad + 1, cy + rad + 1);
    SelectObject(dc, op);
    SelectObject(dc, ob);
}

static void draw_earth(HDC dc, int cx, int cy)
{
    for (int r = EARTH_PX; r >= 1; r--)
        fill_ellipse(dc, cx, cy, r, g_brEarth[r]);

    for (int a = 0; a < 360; a += 4) {
        double t = a * 0.017453292519943295;
        SetPixel(dc, cx + (int)(cos(t) * EARTH_PX),
                     cy + (int)(sin(t) * EARTH_PX), RGB(150, 235, 190));
    }
}

static void render_space(HDC dc, Sim *s, int cx, int cy)
{
    static DrawObj buf[DRAW_CAP];
    size_t n = 0;

    for (CatalogNode *it = s->cat.head; it && n < DRAW_CAP; it = it->next) {
        Object *o = it->obj;
        D3 p = { o->pos.x, o->pos.y, o->pos.z };
        DrawObj d;
        d.o = o;
        project(p, cx, cy, g_yaw, &d.px, &d.py, &d.depth);
        buf[n++] = d;
    }
    qsort(buf, n, sizeof(DrawObj), cmp_far);

    int drew_earth = 0;
    for (size_t i = 0; i < n; i++) {
        if (!drew_earth && buf[i].depth <= 0.0) { draw_earth(dc, cx, cy); drew_earth = 1; }

        double dx = buf[i].px - cx, dy = buf[i].py - cy;
        int far_side = (buf[i].depth > 0.0);
        if (far_side && dx * dx + dy * dy < (double)((EARTH_PX - 2) * (EARTH_PX - 2)))
            continue;

        HBRUSH br = g_brQuiet;
        if (buf[i].o->flag == 1) br = g_brConj;
        if (buf[i].o->flag == 2) br = g_brColl;
        fill_ellipse(dc, (int)buf[i].px, (int)buf[i].py, buf[i].o->flag ? 3 : 2, br);
    }
    if (!drew_earth) draw_earth(dc, cx, cy);

    for (size_t i = 0; i < s->nvis; i++) {
        D3 pa = { s->vis[i].a->pos.x, s->vis[i].a->pos.y, s->vis[i].a->pos.z };
        D3 pb = { s->vis[i].b->pos.x, s->vis[i].b->pos.y, s->vis[i].b->pos.z };
        double ax, ay, ad, bx, by, bd;
        project(pa, cx, cy, g_yaw, &ax, &ay, &ad);
        project(pb, cx, cy, g_yaw, &bx, &by, &bd);

        HGDIOBJ op = SelectObject(dc, s->vis[i].collision ? g_penYel : g_penRed);
        MoveToEx(dc, (int)ax, (int)ay, NULL);
        LineTo(dc, (int)bx, (int)by);
        SelectObject(dc, op);
    }
}

/* ---- child view window --------------------------------------------------- */

static void view_paint(HWND h)
{
    ViewCtx *vc = (ViewCtx *)GetWindowLongPtrA(h, GWLP_USERDATA);
    if (!vc) return;

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);

    RECT cr;
    GetClientRect(h, &cr);
    int w = cr.right, hh = cr.bottom;
    if (w <= 0 || hh <= 0) { EndPaint(h, &ps); return; }

    HDC     mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, w, hh);
    HGDIOBJ ob  = SelectObject(mem, bmp);

    RECT r = { 0, 0, w, hh };
    HBRUSH bg = CreateSolidBrush(RGB(6, 8, 14));
    FillRect(mem, &r, bg);
    DeleteObject(bg);

    render_space(mem, vc->app->sim[vc->idx], w / 2, hh / 2);

    BitBlt(dc, 0, 0, w, hh, mem, 0, 0, SRCCOPY);

    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK ViewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:      view_paint(h); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: {
        ViewCtx *vc = (ViewCtx *)GetWindowLongPtrA(h, GWLP_USERDATA);
        free(vc);
        return 0;
    }
    }
    return DefWindowProcA(h, m, w, l);
}

/* ---- simulation stepping + stats ---------------------------------------- */

static void step_panels(void)
{
    const double budget = g_app.opt->budget_ms;
    const size_t max_steps = 200000;

    for (int i = 0; i < 2; i++) {
        Sim *s = g_app.sim[i];
        ULONGLONG t0 = ms_now();
        size_t done = 0;
        do { sim_step(s); done++; }
        while ((double)(ms_now() - t0) < budget && done < max_steps);
    }
}

static void update_stats(void)
{
    char line[192];
    for (int i = 0; i < 2; i++) {
        Sim *s = g_app.sim[i];
        snprintf(line, sizeof line, "Steps %lu", (unsigned long)s->steps);
        SetWindowTextA(g_app.hStat[i][0], line);

        snprintf(line, sizeof line, "Conjunctions %lu      Collisions %lu",
                 (unsigned long)s->conj_total, (unsigned long)s->coll_total);
        SetWindowTextA(g_app.hStat[i][1], line);

        snprintf(line, sizeof line, "Screen %.2f ms      Average %.2f ms",
                 s->screen_ms_last,
                 s->steps ? s->screen_ms_total / s->steps : 0.0);
        SetWindowTextA(g_app.hStat[i][2], line);
    }

    /* captions follow the selected engines */
    for (int i = 0; i < 2; i++) {
        Sim *s = g_app.sim[i];
        snprintf(line, sizeof line, "%s    %s",
                 s->eng.vt ? s->eng.vt->label : "NONE",
                 s->eng.vt ? s->eng.vt->cx : "");
        SetWindowTextA(g_app.hCap[i], line);
    }

    /* config labels */
    Sim *s = g_app.sim[0];
    static const char *nm[N_CFG] = { "dt", "thr", "mu", "cd" };
    double v[N_CFG] = { s->dt, s->threshold, s->mu, s->collision_dist };
    for (int k = 0; k < N_CFG; k++) {
        snprintf(line, sizeof line, "%s %.4f", nm[k], v[k]);
        SetWindowTextA(g_app.hCfgLabel[k], line);
    }
}

/* ---- main window --------------------------------------------------------- */

static void create_controls(HWND h)
{
    g_app.hStart = CreateWindowExA(0, "BUTTON", "Start",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_START, NULL, NULL);
    g_app.hStop = CreateWindowExA(0, "BUTTON", "Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_STOP, NULL, NULL);
    g_app.hReset = CreateWindowExA(0, "BUTTON", "Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_RESET, NULL, NULL);

    /* engine pickers (native combo boxes) */
    for (int i = 0; i < 2; i++) {
        g_app.hEng[i] = CreateWindowExA(0, "COMBOBOX", "",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 10, 100, h,
            (HMENU)(INT_PTR)(i == 0 ? ID_ENGL : ID_ENGR), NULL, NULL);
        for (int e = 0; e < ENG_COUNT; e++) {
            const EngineVTable *vt = engine_vtable((EngineType)e);
            if (vt) SendMessageA(g_app.hEng[i], CB_ADDSTRING, 0, (LPARAM)vt->label);
        }
        EngineType cur = g_app.sim[i]->eng_type;
        SendMessageA(g_app.hEng[i], CB_SETCURSEL, (WPARAM)cur, 0);
    }

    /* config steppers: [-] value [+] for dt / threshold / mu / collision */
    static const char *cfg_name[N_CFG] = { "dt", "thr", "mu", "cd" };
    for (int s = 0; s < N_CFG; s++) {
        g_app.hCfgMinus[s] = CreateWindowExA(0, "BUTTON", "-",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, h, (HMENU)(INT_PTR)(ID_CFG + s * 2), NULL, NULL);
        g_app.hCfgLabel[s] = CreateWindowExA(0, "STATIC", cfg_name[s],
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, 10, 10, h, NULL, NULL, NULL);
        g_app.hCfgPlus[s] = CreateWindowExA(0, "BUTTON", "+",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 10, 10, h, (HMENU)(INT_PTR)(ID_CFG + s * 2 + 1), NULL, NULL);
    }
    g_app.hRoc = CreateWindowExA(0, "BUTTON", "ROC",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_ROC, NULL, NULL);
    SendMessageA(g_app.hRoc, BM_SETCHECK,
                 g_app.sim[0]->remove_on_collision ? BST_CHECKED : BST_UNCHECKED, 0);

    static const char *caps[2] = { "KD-tree    O(n log n)", "Brute force    O(n^2)" };
    for (int i = 0; i < 2; i++) {
        g_app.hCap[i] = CreateWindowExA(0, "STATIC", caps[i],
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 10, 10, h, NULL, NULL, NULL);
        if (g_fontBold)
            SendMessageA(g_app.hCap[i], WM_SETFONT, (WPARAM)g_fontBold, TRUE);

        for (int j = 0; j < 3; j++)
            g_app.hStat[i][j] = CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 0, 10, 10, h, NULL, NULL, NULL);
    }

    for (int i = 0; i < 2; i++) {
        ViewCtx *vc = (ViewCtx *)malloc(sizeof *vc);
        vc->app = &g_app;
        vc->idx = i;
        g_app.hView[i] = CreateWindowExA(WS_EX_CLIENTEDGE, "AstrosisView", "",
            WS_CHILD | WS_VISIBLE,
            0, 0, 10, 10, h, NULL, NULL, NULL);
        SetWindowLongPtrA(g_app.hView[i], GWLP_USERDATA, (LONG_PTR)vc);
    }

    g_app.hStatus = CreateWindowExA(0, STATUSCLASSNAMEA, "",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, h, (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
}

static void layout(HWND h)
{
    SendMessageA(g_app.hStatus, WM_SIZE, 0, 0);

    RECT sr;
    GetWindowRect(g_app.hStatus, &sr);
    int sh = sr.bottom - sr.top;

    RECT cr;
    GetClientRect(h, &cr);
    int W = cr.right, H = cr.bottom;

    int bw = 88, bh = 25, by = 8, bx = 10;
    MoveWindow(g_app.hStart, bx, by, bw, bh, TRUE); bx += bw + 8;
    MoveWindow(g_app.hStop,  bx, by, bw, bh, TRUE); bx += bw + 8;
    MoveWindow(g_app.hReset, bx, by, bw, bh, TRUE); bx += bw + 16;

    /* engine combos (row 1) */
    int comboW = 170;
    MoveWindow(g_app.hEng[0], bx, by, comboW, bh + 120, TRUE); bx += comboW + 12;
    MoveWindow(g_app.hEng[1], bx, by, comboW, bh + 120, TRUE); bx += comboW + 16;

    /* ROC toggle (row 1, right of combos) */
    MoveWindow(g_app.hRoc, bx, by + 3, 90, bh - 4, TRUE);

    /* config steppers (row 2) */
    int cy = by + bh + 6;
    int slotW = 24, valW = 78, gap = 4;
    int cx0 = 10;
    for (int s = 0; s < N_CFG; s++) {
        MoveWindow(g_app.hCfgMinus[s], cx0, cy, slotW, bh, TRUE);
        MoveWindow(g_app.hCfgLabel[s], cx0 + slotW, cy, valW, bh, TRUE);
        MoveWindow(g_app.hCfgPlus[s],  cx0 + slotW + valW, cy, slotW, bh, TRUE);
        cx0 += slotW * 2 + valW + gap + 8;
    }

    int top = cy + bh + 10;
    int margin = 10;
    int panelW = (W - 3 * margin) / 2;
    int availH = H - sh - top - 10;
    int capH = 22, statH = 66;
    int viewH = availH - capH - statH - 8;
    if (viewH < 40) viewH = 40;

    for (int i = 0; i < 2; i++) {
        int px = margin + i * (panelW + margin);
        MoveWindow(g_app.hCap[i],    px,     top,                 panelW, capH,  TRUE);
        MoveWindow(g_app.hView[i],   px,     top + capH,          panelW, viewH, TRUE);
        int sy = top + capH + viewH + 4;
        MoveWindow(g_app.hStat[i][0], px + 4, sy,      panelW - 8, 18, TRUE);
        MoveWindow(g_app.hStat[i][1], px + 4, sy + 18, panelW - 8, 18, TRUE);
        MoveWindow(g_app.hStat[i][2], px + 4, sy + 36, panelW - 8, 18, TRUE);
    }

    int parts[2] = { W / 3, -1 };
    SendMessageA(g_app.hStatus, SB_SETPARTS, 2, (LPARAM)parts);

    char sb[192];
    snprintf(sb, sizeof sb, "Scene: %s",
             g_app.opt->scene_name ? g_app.opt->scene_name : "");
    SendMessageA(g_app.hStatus, SB_SETTEXTA, 0, (LPARAM)sb);

    snprintf(sb, sizeof sb, "Compute budget %.0f ms/frame      Esc quit      Space pause",
             g_app.opt->budget_ms);
    SendMessageA(g_app.hStatus, SB_SETTEXTA, 1, (LPARAM)sb);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        create_controls(h);
        update_stats();
        return 0;

    case WM_SIZE:
        layout(h);
        return 0;

    case WM_TIMER:
        if (w == TIMER_ID) {
            if (!g_app.paused) {
                g_yaw += 0.0035;
                step_panels();
                InvalidateRect(g_app.hView[0], NULL, FALSE);
                InvalidateRect(g_app.hView[1], NULL, FALSE);
            }
            update_stats();
            g_app.frames++;
            if (g_app.opt->max_frames > 0 && g_app.frames >= g_app.opt->max_frames)
                PostQuitMessage(0);
        }
        return 0;

    case WM_COMMAND:
        /* engine pickers (combo boxes) */
        if (HIWORD(w) == CBN_SELCHANGE) {
            int idx = LOWORD(w) - ID_ENGL;
            if (idx >= 0 && idx < 2) {
                int sel = (int)SendMessageA(g_app.hEng[idx], CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < ENG_COUNT)
                    sim_set_engine(g_app.sim[idx], (EngineType)sel);
            }
            return 0;
        }
        switch (LOWORD(w)) {
        case ID_START: g_app.paused = 0; break;
        case ID_STOP:  g_app.paused = 1; break;
        case ID_RESET:
            if (g_app.opt->on_reset) g_app.opt->on_reset(g_app.opt->reset_ud);
            g_yaw = 0.0;
            InvalidateRect(g_app.hView[0], NULL, FALSE);
            InvalidateRect(g_app.hView[1], NULL, FALSE);
            break;
        case ID_ROC: {
            int on = (SendMessageA(g_app.hRoc, BM_GETCHECK, 0, 0) == BST_CHECKED);
            sim_set_remove_on_collision(g_app.sim[0], on);
            sim_set_remove_on_collision(g_app.sim[1], on);
            break;
        }
        default:
            if (LOWORD(w) >= ID_CFG && LOWORD(w) < ID_CFG + N_CFG * 2) {
                int slot = (LOWORD(w) - ID_CFG) / 2;
                int plus = (LOWORD(w) - ID_CFG) % 2;
                double f = plus ? 1.5 : (1.0 / 1.5);
                Sim *s = g_app.sim[0];
                switch (slot) {
                case 0: { double v = s->dt * f;
                          sim_set_dt(g_app.sim[0], v); sim_set_dt(g_app.sim[1], v); break; }
                case 1: { double v = s->threshold * f;
                          sim_set_threshold(g_app.sim[0], v); sim_set_threshold(g_app.sim[1], v); break; }
                case 2: { double v = s->mu * f;
                          sim_set_mu(g_app.sim[0], v); sim_set_mu(g_app.sim[1], v); break; }
                case 3: { double v = s->collision_dist * f;
                          sim_set_collision_dist(g_app.sim[0], v);
                          sim_set_collision_dist(g_app.sim[1], v); break; }
                }
                update_stats();
            }
            break;
        }
        return 0;

    case WM_KEYDOWN:
        if (w == VK_ESCAPE)     DestroyWindow(h);
        else if (w == VK_SPACE) g_app.paused = !g_app.paused;
        return 0;

    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

    case WM_DESTROY:
        KillTimer(h, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int gui_run(Sim *left, Sim *right, const GuiOptions *opt)
{
    memset(&g_app, 0, sizeof g_app);
    g_app.sim[0] = left;
    g_app.sim[1] = right;
    g_app.opt    = opt;

    gdi_init();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof icc;
    icc.dwICC  = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    HINSTANCE inst = GetModuleHandleA(NULL);

    WNDCLASSA wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = MainProc;
    wc.hInstance     = inst;
    wc.lpszClassName = "AstrosisMainWnd";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    if (!RegisterClassA(&wc)) { gdi_done(); return 1; }

    WNDCLASSA vc;
    memset(&vc, 0, sizeof vc);
    vc.style         = CS_HREDRAW | CS_VREDRAW;
    vc.lpfnWndProc   = ViewProc;
    vc.hInstance     = inst;
    vc.lpszClassName = "AstrosisView";
    vc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    vc.hbrBackground = NULL;
    if (!RegisterClassA(&vc)) { gdi_done(); return 1; }

    RECT r = { 0, 0, 1024, 700 };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    HWND h = CreateWindowExA(0, "AstrosisMainWnd",
        "Astrosis-C  -  spatial indexing & conjunction screening",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
        NULL, NULL, inst, NULL);
    if (!h) { gdi_done(); return 1; }

    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    SetTimer(h, TIMER_ID, 15, NULL);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    gdi_done();
    return 0;
}

#endif /* _WIN32 */
