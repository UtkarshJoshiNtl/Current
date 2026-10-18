/* viz.c - SDL2 renderer: soft Windows Aero-style chrome around a two-panel
 * spatial-screening demo.
 *
 * Text uses an anti-aliased system TrueType font when SDL2_ttf is available
 * (searched at runtime across Linux/Windows/macOS), and falls back to a tiny
 * embedded bitmap font otherwise. The simulation itself (blue-green planet and
 * satellites) renders unchanged inside a clipped black viewport in each child
 * window.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>

#if !defined(ASTRO_NO_TTF) && defined(__has_include)
#  if __has_include(<SDL_ttf.h>)
#    include <SDL_ttf.h>
#    define ASTRO_TTF 1
#  endif
#endif

#include "gui.h"
#include "font3x5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- window layout ------------------------------------------------------ */
#define WIN_W       1280
#define WIN_H       720
#define TITLEBAR_H  32
#define TOOLBAR_H   80
#define STATUS_H    26
#define MARGIN      12
#define CAPTION_H   26
#define CHILD_PAD   8
#define CHILD_STAT_H 72

/* ---- simulation-view constants (unchanged) ------------------------------ */
#define SCALE_PX    112.0
#define EARTH_PX    16
#define DRAW_CAP    32768

/* ---- soft Aero palette -------------------------------------------------- */
static const Uint8 C_TITLE_TOP[3]  = { 220, 235, 250 };
static const Uint8 C_TITLE_BOT[3]  = { 146, 184, 222 };
static const Uint8 C_TITLE_EDGE[3] = { 108, 148, 188 };
static const Uint8 C_TITLE_TXT[3]  = {  34,  54,  86 };
static const Uint8 C_CLIENT[3]     = { 240, 240, 240 };
static const Uint8 C_CAP_TOP[3]    = { 246, 249, 252 };
static const Uint8 C_CAP_BOT[3]    = { 224, 233, 242 };
static const Uint8 C_FACE_TOP[3]   = { 251, 251, 251 };
static const Uint8 C_FACE_BOT[3]   = { 228, 228, 228 };
static const Uint8 C_FACE_EDGE[3]  = { 168, 168, 168 };
static const Uint8 C_FIELD[3]      = { 255, 255, 255 };
static const Uint8 C_INK[3]        = {  46,  56,  76 };
static const Uint8 C_INK_SOFT[3]   = { 116, 132, 150 };
static const Uint8 C_WHITE[3]      = { 255, 255, 255 };
static const Uint8 C_HOVER[3]      = { 232, 241, 251 };
static const Uint8 C_HOVER_ED[3]   = { 140, 175, 215 };

/* ---- tiny vector / projection helpers ---------------------------------- */

typedef struct { double x, y, z; } D3;
typedef struct { int x, y, w, h; } Rect;

static double g_yaw = 0.0;
static const double PITCH = 0.34;

static void project(D3 p, double cx, double cy, double *sx, double *sy, double *depth)
{
    double cy_ = cos(g_yaw), sy_ = sin(g_yaw);
    double cp = cos(PITCH), sp = sin(PITCH);

    double x1 =  p.x * cy_ + p.z * sy_;
    double z1 = -p.x * sy_ + p.z * cy_;
    double y1 =  p.y;

    double y2 = y1 * cp - z1 * sp;
    double z2 = y1 * sp + z1 * cp;

    *sx = cx + x1 * SCALE_PX;
    *sy = cy - y2 * SCALE_PX;
    *depth = z2;
}

/* ---- text layer: TrueType if available, else embedded bitmap ------------ */

#ifdef ASTRO_TTF
static int g_ttf = 0;
static TTF_Font *g_ft = NULL;   /* title */
static TTF_Font *g_fb = NULL;   /* body  */
#endif

static int text_init(void)
{
#ifdef ASTRO_TTF
    static const char *cands[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
        NULL
    };
    if (TTF_Init() != 0) return 0;
    for (int i = 0; cands[i]; i++) {
        g_fb = TTF_OpenFont(cands[i], 14);
        if (g_fb) {
            g_ft = TTF_OpenFont(cands[i], 17);
            if (!g_ft) g_ft = g_fb;
            g_ttf = 1;
            break;
        }
    }
    if (!g_ttf) { TTF_Quit(); return 0; }
#endif
    return 0;
}

static void text_quit(void)
{
#ifdef ASTRO_TTF
    if (g_ttf) {
        if (g_ft && g_ft != g_fb) TTF_CloseFont(g_ft);
        if (g_fb) TTF_CloseFont(g_fb);
        TTF_Quit();
        g_ttf = 0;
    }
#endif
}

static int size_scale(int size) { return size >= 15 ? 3 : 2; }

static int text_w(int size, const char *s)
{
#ifdef ASTRO_TTF
    if (g_ttf) {
        TTF_Font *f = (size >= 15) ? g_ft : g_fb;
        int w = 0, h = 0;
        if (TTF_SizeUTF8(f, s, &w, &h) == 0) return w;
    }
#endif
    int sc = size_scale(size);
    return (int)strlen(s) * 4 * sc - sc;
}

static void text(SDL_Renderer *r, int x, int y, int size, const char *s, const Uint8 col[3])
{
#ifdef ASTRO_TTF
    if (g_ttf) {
        TTF_Font *f = (size >= 15) ? g_ft : g_fb;
        SDL_Color c = { col[0], col[1], col[2], 255 };
        SDL_Surface *su = TTF_RenderUTF8_Blended(f, s, c);
        if (su) {
            SDL_Texture *tx = SDL_CreateTextureFromSurface(r, su);
            if (tx) {
                SDL_Rect d = { x, y, su->w, su->h };
                SDL_RenderCopy(r, tx, NULL, &d);
                SDL_DestroyTexture(tx);
            }
            SDL_FreeSurface(su);
        }
        return;
    }
#endif
    /* bitmap fallback: uppercase and draw as blocks */
    int sc = size_scale(size);
    SDL_SetRenderDrawColor(r, col[0], col[1], col[2], 255);
    for (; *s; s++) {
        char c = *s;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        Glyph g = glyph_for(c);
        for (int row = 0; row < 5; row++)
            for (int col2 = 0; col2 < 3; col2++)
                if (g.r[row][col2] == '#') {
                    SDL_Rect rc = { x + col2 * sc, y + row * sc, sc, sc };
                    SDL_RenderFillRect(r, &rc);
                }
        x += 4 * sc;
    }
}

static void text_sh(SDL_Renderer *r, int x, int y, int size, const char *s,
                    const Uint8 col[3], const Uint8 sh[3])
{
#ifdef ASTRO_TTF
    if (g_ttf) {
        text(r, x, y + 1, size, s, sh);
        text(r, x, y, size, s, col);
        return;
    }
#endif
    text(r, x + 1, y + 1, size, s, sh);
    text(r, x, y, size, s, col);
}

/* ---- drawing primitives ------------------------------------------------- */

static void setcol(SDL_Renderer *r, const Uint8 c[3])
{
    SDL_SetRenderDrawColor(r, c[0], c[1], c[2], 255);
}

static void rectf(SDL_Renderer *r, int x, int y, int w, int h, const Uint8 c[3])
{
    setcol(r, c);
    SDL_Rect R = { x, y, w, h };
    SDL_RenderFillRect(r, &R);
}

static void frame(SDL_Renderer *r, int x, int y, int w, int h, const Uint8 c[3])
{
    setcol(r, c);
    SDL_Rect R = { x, y, w, h };
    SDL_RenderDrawRect(r, &R);
}

static void hline(SDL_Renderer *r, int x, int y, int w, const Uint8 c[3])
{
    setcol(r, c);
    SDL_RenderDrawLine(r, x, y, x + w - 1, y);
}

static void vgrad(SDL_Renderer *r, int x, int y, int w, int h,
                  const Uint8 top[3], const Uint8 bot[3])
{
    for (int i = 0; i < h; i++) {
        double t = (h > 1) ? (double)i / (h - 1) : 0.0;
        Uint8 c[3];
        c[0] = (Uint8)(top[0] + (bot[0] - top[0]) * t);
        c[1] = (Uint8)(top[1] + (bot[1] - top[1]) * t);
        c[2] = (Uint8)(top[2] + (bot[2] - top[2]) * t);
        hline(r, x, y + i, w, c);
    }
}

static void sunken(SDL_Renderer *r, int x, int y, int w, int h)
{
    static const Uint8 light[3] = { 255, 255, 255 };
    static const Uint8 dark[3]  = { 150, 150, 150 };
    setcol(r, dark);
    SDL_RenderDrawLine(r, x, y, x + w - 1, y);
    SDL_RenderDrawLine(r, x, y, x, y + h - 1);
    setcol(r, light);
    SDL_RenderDrawLine(r, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(r, x + w - 1, y, x + w - 1, y + h - 1);
}

/* ---- the space render (UNCHANGED look) --------------------------------- */

static void fill_circle(SDL_Renderer *r, int cx, int cy, int rad, const Uint8 c[3])
{
    setcol(r, c);
    for (int dy = -rad; dy <= rad; dy++) {
        int half = (int)(sqrt((double)(rad * rad - dy * dy)) + 0.5);
        SDL_RenderDrawLine(r, cx - half, cy + dy, cx + half, cy + dy);
    }
}

static void draw_earth(SDL_Renderer *r, int cx, int cy)
{
    static const Uint8 core[3] = { 12,  46,  92 };
    static const Uint8 edge[3] = { 34, 190, 128 };
    Uint8 col[3];

    for (int rad = EARTH_PX; rad >= 1; rad--) {
        double t = (double)rad / EARTH_PX;
        col[0] = (Uint8)(core[0] + (edge[0] - core[0]) * t);
        col[1] = (Uint8)(core[1] + (edge[1] - core[1]) * t);
        col[2] = (Uint8)(core[2] + (edge[2] - core[2]) * t);
        fill_circle(r, cx, cy, rad, col);
    }
    static const Uint8 rim[3] = { 150, 235, 190 };
    setcol(r, rim);
    for (int a = 0; a < 360; a += 4) {
        double t = a * 0.017453292519943295;
        SDL_RenderDrawPoint(r, cx + (int)(cos(t) * EARTH_PX),
                               cy + (int)(sin(t) * EARTH_PX));
    }
}

typedef struct { double px, py, depth; Object *o; } DrawObj;

static int cmp_far(const void *A, const void *B)
{
    const DrawObj *a = (const DrawObj *)A, *b = (const DrawObj *)B;
    return (a->depth > b->depth) - (a->depth < b->depth);
}

static void render_space(SDL_Renderer *r, Sim *s, int cx, int cy)
{
    static DrawObj buf[DRAW_CAP];
    size_t n = 0;
    for (CatalogNode *it = s->cat.head; it && n < DRAW_CAP; it = it->next) {
        Object *o = it->obj;
        D3 p = { o->pos.x, o->pos.y, o->pos.z };
        DrawObj d;
        d.o = o;
        project(p, cx, cy, &d.px, &d.py, &d.depth);
        buf[n++] = d;
    }
    qsort(buf, n, sizeof(DrawObj), cmp_far);

    int drew_earth = 0;
    for (size_t i = 0; i < n; i++) {
        if (!drew_earth && buf[i].depth <= 0.0) { draw_earth(r, cx, cy); drew_earth = 1; }

        double dx = buf[i].px - cx, dy = buf[i].py - cy;
        int far_side = (buf[i].depth > 0.0);
        if (far_side && dx * dx + dy * dy < (double)((EARTH_PX - 2) * (EARTH_PX - 2)))
            continue;

        static const Uint8 quiet[3] = { 150, 170, 190 };
        static const Uint8 conj[3]  = { 255,  72,  72 };
        static const Uint8 coll[3]  = { 255, 215,  40 };
        const Uint8 *c = quiet;
        if (buf[i].o->flag == 1) c = conj;
        if (buf[i].o->flag == 2) c = coll;
        fill_circle(r, (int)buf[i].px, (int)buf[i].py, buf[i].o->flag ? 3 : 2, c);
    }
    if (!drew_earth) draw_earth(r, cx, cy);

    for (size_t i = 0; i < s->nvis; i++) {
        D3 pa = { s->vis[i].a->pos.x, s->vis[i].a->pos.y, s->vis[i].a->pos.z };
        D3 pb = { s->vis[i].b->pos.x, s->vis[i].b->pos.y, s->vis[i].b->pos.z };
        double ax, ay, ad, bx, by, bd;
        project(pa, cx, cy, &ax, &ay, &ad);
        project(pb, cx, cy, &bx, &by, &bd);
        if (s->vis[i].collision) { static const Uint8 y[3] = { 255, 215, 40 };  setcol(r, y); }
        else                     { static const Uint8 rd[3] = { 245, 90, 90 }; setcol(r, rd); }
        SDL_RenderDrawLine(r, (int)ax, (int)ay, (int)bx, (int)by);
    }
}

/* ---- chrome ------------------------------------------------------------- */

static int g_mx = -1, g_my = -1, g_mdown = 0;

static Rect btn_start(void) { return (Rect){ 14, TITLEBAR_H + 6, 92, 26 }; }
static Rect btn_stop(void)  { return (Rect){ 112, TITLEBAR_H + 6, 92, 26 }; }
static Rect btn_reset(void) { return (Rect){ 210, TITLEBAR_H + 6, 92, 26 }; }

/* engine pickers (row 1) */
static Rect btn_engL(void)  { return (Rect){ 320, TITLEBAR_H + 6, 190, 26 }; }
static Rect btn_engR(void)  { return (Rect){ 522, TITLEBAR_H + 6, 190, 26 }; }

/* config steppers (row 2): [-] value [+] */
#define CFG_Y    (TITLEBAR_H + 44)
#define CFG_W    26
#define CFG_VALW 78
#define CFG_GAP  6

static Rect cfg_minus(int slot)
{
    return (Rect){ 14 + slot * (CFG_W * 2 + CFG_VALW + CFG_GAP), CFG_Y, CFG_W, 26 };
}
static Rect cfg_label(int slot)
{
    Rect m = cfg_minus(slot);
    return (Rect){ m.x + CFG_W, CFG_Y, CFG_VALW, 26 };
}
static Rect cfg_plus(int slot)
{
    Rect m = cfg_minus(slot);
    return (Rect){ m.x + CFG_W + CFG_VALW, CFG_Y, CFG_W, 26 };
}

enum { SLOT_DT = 0, SLOT_THR, SLOT_MU, SLOT_CD, SLOT_ROC, SLOT_COUNT };

static int in_rect(Rect b, int x, int y)
{
    return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

static Rect win_close(void) { return (Rect){ WIN_W - 30, 6, 24, 20 }; }
static Rect win_max(void)   { return (Rect){ WIN_W - 56, 6, 24, 20 }; }
static Rect win_min(void)   { return (Rect){ WIN_W - 82, 6, 24, 20 }; }

static void draw_winbtn(SDL_Renderer *r, Rect b, int kind, int hot)
{
    if (kind == 2 && hot) {
        vgrad(r, b.x, b.y, b.w, b.h, (const Uint8[]){ 232, 110, 92 }, (const Uint8[]){ 190, 46, 30 });
    } else if (hot) {
        vgrad(r, b.x, b.y, b.w, b.h, (const Uint8[]){ 240, 246, 252 }, (const Uint8[]){ 208, 224, 240 });
    } else {
        vgrad(r, b.x, b.y, b.w, b.h, (const Uint8[]){ 236, 243, 250 }, (const Uint8[]){ 210, 224, 238 });
    }
    frame(r, b.x, b.y, b.w, b.h, (const Uint8[]){ 150, 180, 205 });

    int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    const Uint8 *ic = (kind == 2 && hot) ? C_WHITE : C_TITLE_TXT;
    setcol(r, ic);
    if (kind == 0) {
        SDL_RenderDrawLine(r, cx - 4, cy + 4, cx + 4, cy + 4);
        SDL_RenderDrawLine(r, cx - 4, cy + 5, cx + 4, cy + 5);
    } else if (kind == 1) {
        SDL_Rect q = { cx - 4, cy - 4, 9, 9 };
        SDL_RenderDrawRect(r, &q);
    } else {
        SDL_RenderDrawLine(r, cx - 4, cy - 4, cx + 4, cy + 4);
        SDL_RenderDrawLine(r, cx + 4, cy - 4, cx - 4, cy + 4);
        SDL_RenderDrawLine(r, cx - 3, cy - 4, cx + 5, cy + 4);
        SDL_RenderDrawLine(r, cx + 5, cy - 4, cx - 3, cy + 4);
    }
}

static void draw_titlebar(SDL_Renderer *r, const char *title)
{
    vgrad(r, 0, 0, WIN_W, TITLEBAR_H, C_TITLE_TOP, C_TITLE_BOT);
    hline(r, 0, 1, WIN_W, (const Uint8[]){ 250, 253, 255 });
    hline(r, 0, TITLEBAR_H - 1, WIN_W, C_TITLE_EDGE);

    text_sh(r, 12, (TITLEBAR_H - 17) / 2, 15, title, C_TITLE_TXT, (const Uint8[]){ 255, 255, 255 });

    draw_winbtn(r, win_min(), 0, in_rect(win_min(), g_mx, g_my));
    draw_winbtn(r, win_max(), 1, in_rect(win_max(), g_mx, g_my));
    draw_winbtn(r, win_close(), 2, in_rect(win_close(), g_mx, g_my));
}

static void draw_button(SDL_Renderer *r, Rect b, const char *label, int disabled)
{
    int hot = !disabled && in_rect(b, g_mx, g_my);
    int pressed = hot && g_mdown;

    if (pressed) {
        vgrad(r, b.x, b.y, b.w, b.h, (const Uint8[]){ 210, 218, 228 }, (const Uint8[]){ 232, 236, 242 });
    } else if (hot) {
        vgrad(r, b.x, b.y, b.w, b.h, C_HOVER, (const Uint8[]){ 214, 230, 246 });
    } else {
        vgrad(r, b.x, b.y, b.w, b.h, C_FACE_TOP, C_FACE_BOT);
    }
    frame(r, b.x, b.y, b.w, b.h, hot ? C_HOVER_ED : C_FACE_EDGE);

    const Uint8 *tc = disabled ? (const Uint8[]){ 168, 168, 168 } : C_INK;
    int tw = text_w(14, label);
    text(r, b.x + (b.w - tw) / 2, b.y + (b.h - 16) / 2 + (pressed ? 1 : 0), 14, label, tc);
}

/* small centered label over a value box (read-only display) */
static void draw_value(SDL_Renderer *r, Rect b, const char *label)
{
    setcol(r, C_FIELD);
    SDL_Rect R = { b.x, b.y, b.w, b.h };
    SDL_RenderFillRect(r, &R);
    sunken(r, b.x, b.y, b.w, b.h);
    int tw = text_w(14, label);
    text(r, b.x + (b.w - tw) / 2, b.y + (b.h - 16) / 2, 14, label, C_INK);
}

static const char *eng_label(Sim *s)
{
    return s->eng.vt ? s->eng.vt->label : "NONE";
}

static void draw_toolbar(SDL_Renderer *r, Sim *left, Sim *right, int paused)
{
    rectf(r, 0, TITLEBAR_H, WIN_W, TOOLBAR_H, C_CLIENT);
    hline(r, 0, TITLEBAR_H + TOOLBAR_H - 1, WIN_W, (const Uint8[]){ 206, 206, 206 });

    /* row 1: transport + engine pickers */
    draw_button(r, btn_start(), "Start", !paused);
    draw_button(r, btn_stop(),  "Stop",  paused);
    draw_button(r, btn_reset(), "Reset", 0);

    char lab[64];
    snprintf(lab, sizeof lab, "L: %s", eng_label(left));
    draw_button(r, btn_engL(), lab, 0);
    snprintf(lab, sizeof lab, "R: %s", eng_label(right));
    draw_button(r, btn_engR(), lab, 0);

    const char *state = paused ? "Paused" : "Running";
    text(r, WIN_W - text_w(14, state) - 12, TITLEBAR_H + 15, 14, state, C_INK_SOFT);

    /* row 2: configuration steppers (dt, threshold, mu, collision, ROC) */
    static const char *names[SLOT_COUNT] = { "dt", "thr", "mu", "cd", "ROC" };
    for (int s = 0; s < SLOT_ROC; s++) {
        draw_button(r, cfg_minus(s), "-", 0);
        snprintf(lab, sizeof lab, "%s %.4f", names[s],
                 s == SLOT_DT  ? left->dt :
                 s == SLOT_THR ? left->threshold :
                 s == SLOT_MU  ? left->mu : left->collision_dist);
        draw_value(r, cfg_label(s), lab);
        draw_button(r, cfg_plus(s), "+", 0);
    }
    /* ROC toggle spans minus+label+plus of its slot */
    Rect m = cfg_minus(SLOT_ROC), p = cfg_plus(SLOT_ROC);
    Rect roc = { m.x, CFG_Y, (p.x + p.w) - m.x, 26 };
    snprintf(lab, sizeof lab, "ROC %s", left->remove_on_collision ? "ON" : "OFF");
    draw_button(r, roc, lab, 0);
}

static void draw_child(SDL_Renderer *r, Sim *s, Rect ch)
{
    rectf(r, ch.x, ch.y, ch.w, ch.h, C_CLIENT);
    frame(r, ch.x, ch.y, ch.w, ch.h, (const Uint8[]){ 200, 205, 212 });

    vgrad(r, ch.x + 1, ch.y + 1, ch.w - 2, CAPTION_H, C_CAP_TOP, C_CAP_BOT);
    hline(r, ch.x + 1, ch.y + CAPTION_H, ch.w - 2, (const Uint8[]){ 196, 208, 220 });
    const char *cap = eng_label(s);
    text(r, ch.x + 12, ch.y + (CAPTION_H - 16) / 2, 14, cap, C_INK);
    const char *cx = s->eng.vt ? s->eng.vt->cx : "";
    int cw = text_w(13, cx);
    text(r, ch.x + ch.w - cw - 12, ch.y + (CAPTION_H - 15) / 2, 13, cx, C_INK_SOFT);

    Rect v = { ch.x + CHILD_PAD, ch.y + CAPTION_H + CHILD_PAD,
               ch.w - 2 * CHILD_PAD,
               ch.h - CAPTION_H - 3 * CHILD_PAD - CHILD_STAT_H };
    rectf(r, v.x, v.y, v.w, v.h, (const Uint8[]){ 6, 8, 14 });
    sunken(r, v.x, v.y, v.w, v.h);

    SDL_Rect clip = { v.x, v.y, v.w, v.h };
    SDL_RenderSetClipRect(r, &clip);
    render_space(r, s, v.x + v.w / 2, v.y + v.h / 2);
    SDL_RenderSetClipRect(r, NULL);

    Rect st = { ch.x + CHILD_PAD, v.y + v.h + CHILD_PAD,
                ch.w - 2 * CHILD_PAD, CHILD_STAT_H };
    rectf(r, st.x, st.y, st.w, st.h, C_FIELD);
    sunken(r, st.x, st.y, st.w, st.h);

    char line[128];
    int tx = st.x + 12, ty = st.y + 8;
    snprintf(line, sizeof line, "Steps %lu", (unsigned long)s->steps);
    text(r, tx, ty, 15, line, C_INK);

    snprintf(line, sizeof line, "Conjunctions %lu      Collisions %lu",
             (unsigned long)s->conj_total, (unsigned long)s->coll_total);
    text(r, tx, ty + 24, 14, line, C_INK_SOFT);

    snprintf(line, sizeof line, "Screen %.2f ms      Average %.2f ms",
             s->screen_ms_last, s->steps ? s->screen_ms_total / s->steps : 0.0);
    text(r, tx, ty + 42, 14, line, C_INK_SOFT);
}

static void draw_statusbar(SDL_Renderer *r, const char *scene, double budget)
{
    int y = WIN_H - STATUS_H;
    rectf(r, 0, y, WIN_W, STATUS_H, C_CLIENT);
    hline(r, 0, y, WIN_W, (const Uint8[]){ 226, 226, 226 });

    Rect a = { 4, y + 4, WIN_W / 2 - 8, STATUS_H - 8 };
    rectf(r, a.x, a.y, a.w, a.h, C_FIELD);
    sunken(r, a.x, a.y, a.w, a.h);
    char s1[128];
    snprintf(s1, sizeof s1, "Scene   %s", scene);
    text(r, a.x + 8, a.y + 4, 13, s1, C_INK);

    Rect b = { WIN_W / 2 + 4, y + 4, WIN_W / 2 - 8, STATUS_H - 8 };
    rectf(r, b.x, b.y, b.w, b.h, C_FIELD);
    sunken(r, b.x, b.y, b.w, b.h);
    char s2[96];
    snprintf(s2, sizeof s2, "Compute budget %.0f ms per frame      Esc quit      Space pause",
             budget);
    text(r, b.x + 8, b.y + 4, 13, s2, C_INK);
}

/* ---------------------------------------------- main loop ---------------- */

static double sdl_ms(void)
{
    return (double)SDL_GetPerformanceCounter() * 1000.0 /
           (double)SDL_GetPerformanceFrequency();
}

int gui_run(Sim *left, Sim *right, const GuiOptions *opt)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    text_init();

    char title[160];
    snprintf(title, sizeof title, "Astrosis-C   -   %s   (%s vs %s)",
             opt->scene_name ? opt->scene_name : "simulation",
             engine_type_name(opt->engine_left), engine_type_name(opt->engine_right));

    SDL_Window *win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        text_quit();
        SDL_Quit();
        return 1;
    }
    Uint32 rflags = opt->vsync ? SDL_RENDERER_PRESENTVSYNC : 0;
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | rflags);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);

    int running = 1, paused = 0, frames = 0;
    const double budget = opt->budget_ms;
    const size_t max_steps_per_frame = 200000;

    /* engine pickers start from whatever the driver selected */
    EngineType el = opt->engine_left, er = opt->engine_right;

    /* apply a config value to BOTH sims so parity is preserved */
#define APPLY2(fn, val) do { fn(left, val); fn(right, val); } while (0)

    int cw = (WIN_W - 3 * MARGIN) / 2;
    int cy0 = TITLEBAR_H + TOOLBAR_H + MARGIN;
    int ch = WIN_H - STATUS_H - MARGIN - cy0;
    Rect childL = { MARGIN, cy0, cw, ch };
    Rect childR = { MARGIN + cw + MARGIN, cy0, cw, ch };

    while (running) {
        SDL_GetMouseState(&g_mx, &g_my);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                g_mdown = 1;
                int mx = e.button.x, my = e.button.y;
                if (in_rect(win_close(), mx, my)) running = 0;
                else if (in_rect(btn_start(), mx, my)) paused = 0;
                else if (in_rect(btn_stop(),  mx, my)) paused = 1;
                else if (in_rect(btn_reset(), mx, my)) {
                    if (opt->on_reset) opt->on_reset(opt->reset_ud);
                    g_yaw = 0.0;
                }
                else if (in_rect(btn_engL(), mx, my)) {
                    el = (EngineType)((el + 1) % ENG_COUNT);
                    sim_set_engine(left, el);
                }
                else if (in_rect(btn_engR(), mx, my)) {
                    er = (EngineType)((er + 1) % ENG_COUNT);
                    sim_set_engine(right, er);
                }
                else {
                    /* config steppers: [slot] = which value, +/- adjusts */
                    for (int slot = 0; slot < SLOT_COUNT; slot++) {
                        int minus = in_rect(cfg_minus(slot), mx, my);
                        int plus  = in_rect(cfg_plus(slot), mx, my);
                        int is_roc = (slot == SLOT_ROC);
                        if (!is_roc && !minus && !plus) continue;
                        if (is_roc) {
                            if (in_rect(cfg_minus(slot), mx, my) ||
                                in_rect(cfg_label(slot), mx, my) ||
                                in_rect(cfg_plus(slot), mx, my)) {
                                APPLY2(sim_set_remove_on_collision,
                                       !left->remove_on_collision);
                            }
                            continue;
                        }
                        double f = plus ? 1.5 : (1.0 / 1.5);
                        switch (slot) {
                            case SLOT_DT:  APPLY2(sim_set_dt,  left->dt * f);            break;
                            case SLOT_THR: APPLY2(sim_set_threshold, left->threshold * f); break;
                            case SLOT_MU:  APPLY2(sim_set_mu,  left->mu * f);            break;
                            case SLOT_CD:  APPLY2(sim_set_collision_dist,
                                                  left->collision_dist * f);             break;
                        }
                    }
                }
            } else if (e.type == SDL_MOUSEBUTTONUP) {
                g_mdown = 0;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = 0;
                else if (e.key.keysym.sym == SDLK_SPACE) paused = !paused;
                else if (e.key.keysym.sym == SDLK_LEFTBRACKET) {   /* [ ] cycle engines */
                    er = (EngineType)((er + ENG_COUNT - 1) % ENG_COUNT);
                    sim_set_engine(right, er);
                } else if (e.key.keysym.sym == SDLK_RIGHTBRACKET) {
                    er = (EngineType)((er + 1) % ENG_COUNT);
                    sim_set_engine(right, er);
                } else if (e.key.keysym.sym == SDLK_COMMA) {
                    el = (EngineType)((el + ENG_COUNT - 1) % ENG_COUNT);
                    sim_set_engine(left, el);
                } else if (e.key.keysym.sym == SDLK_PERIOD) {
                    el = (EngineType)((el + 1) % ENG_COUNT);
                    sim_set_engine(left, el);
                }
            }
        }

        if (!paused) {
            g_yaw += 0.0035;
            Sim *panels[2] = { left, right };
            for (int i = 0; i < 2; i++) {
                Sim *s = panels[i];
                double t0 = sdl_ms();
                size_t done = 0;
                do { sim_step(s); done++; }
                while (sdl_ms() - t0 < budget && done < max_steps_per_frame);
            }
        }

        rectf(ren, 0, 0, WIN_W, WIN_H, C_CLIENT);
        draw_titlebar(ren, title);
        draw_toolbar(ren, left, right, paused);
        draw_child(ren, left,  childL);
        draw_child(ren, right, childR);
        draw_statusbar(ren, opt->scene_name ? opt->scene_name : "", opt->budget_ms);

        SDL_RenderPresent(ren);
        frames++;
        if (opt->max_frames > 0 && frames >= opt->max_frames) {
            if (opt->shot_path) {
                SDL_Surface *ss = SDL_CreateRGBSurfaceWithFormat(
                    0, WIN_W, WIN_H, 32, SDL_PIXELFORMAT_ARGB8888);
                if (ss) {
                    SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, ss->pixels, ss->pitch);
                    SDL_SaveBMP(ss, opt->shot_path);
                    SDL_FreeSurface(ss);
                }
            }
            running = 0;
        }
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    text_quit();
    SDL_Quit();

    printf("final: %s steps=%lu   %s steps=%lu   (%.1fx more)\n",
           engine_type_name(left->eng_type), (unsigned long)left->steps,
           engine_type_name(right->eng_type), (unsigned long)right->steps,
           right->steps ? (double)left->steps / (double)right->steps : 0.0);
    return 0;
}
