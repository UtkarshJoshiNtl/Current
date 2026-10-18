/* main.c - Astrosis-C driver: visual comparison + headless benchmark */
#include "scenes.h"
#include "sim.h"
#include "gui.h"
#include "brute_force.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static void usage(const char *prog)
{
    printf(
        "ASTROSIS-C - spatial indexing & conjunction screening\n"
        "usage: %s [options]\n"
        "  --scene <id|index>   scenario to run (default: dense-ring)\n"
        "  --list-scenes        print scenarios and exit\n"
        "  --seed <n>           RNG seed (default 12345)\n"
        "  --threshold <f>      conjunction distance (default 0.060)\n"
        "  --dt <f>             timestep (default 0.010)\n"
        "  --mu <f>             gravity constant (default 1.0)\n"
        "  --budget <ms>        per-panel compute budget per frame (default 10.0)\n"
        "  --frames <n>         quit after n frames (default 0 = until closed)\n"
        "  --no-vsync           disable vertical sync\n"
        "  --shot <file.bmp>    save a BMP of the final frame\n"
        "  --bench              headless benchmark instead of the window\n"
        "  --steps <n>          steps per scene in benchmark (default 400)\n"
        "  (in the window: ESC quits, SPACE pauses)\n",
        prog);
}

static void list_scenes(void)
{
    printf("scenarios:\n");
    for (int i = 0; i < scene_count(); i++) {
        const Scene *s = scene_by_index(i);
        printf("  [%d] %-18s %s\n", i, s->id, s->desc);
    }
}

/* ------------------------------------------------------------------ bench */

static void bench_one(const Scene *sc, double mu, double dt, double thr,
                      unsigned long seed, size_t steps)
{
    Sim kd, bf;
    sim_init(&kd, ENGINE_KD,    sc->phys, mu, dt, thr, sc->collision_dist, sc->remove_on_collision);
    sim_init(&bf, ENGINE_BRUTE, sc->phys, mu, dt, thr, sc->collision_dist, sc->remove_on_collision);
    scene_build(sc, &kd.cat, mu, seed);
    scene_build(sc, &bf.cat, mu, seed);

    size_t n0 = kd.cat.count;
    for (size_t i = 0; i < steps; i++) {
        sim_step(&kd);
        sim_step(&bf);
    }

    double kd_ms = kd.screen_ms_total, bf_ms = bf.screen_ms_total;
    double speed = kd_ms > 0.0 ? bf_ms / kd_ms : 0.0;
    int parity = (kd.conj_total == bf.conj_total) && (kd.coll_total == bf.coll_total);

    printf("  %-18s N=%-5lu steps=%-6lu pairs=%-8lu  kd=%8.2f ms  brute=%9.2f ms  x%6.1f  %s\n",
           sc->id, (unsigned long)n0, (unsigned long)steps,
           (unsigned long)kd.conj_total, kd_ms, bf_ms, speed,
           parity ? "PARITY OK" : "PARITY MISMATCH");

    sim_free(&kd);
    sim_free(&bf);
}

static int run_bench(const Scene *only, double mu, double dt, double thr, int have_thr,
                     unsigned long seed, size_t steps)
{
    printf("\nASTROSIS-C benchmark  (same physics, identical pair sets expected)\n\n");
    if (only) {
        bench_one(only, mu, dt, have_thr ? thr : only->threshold, seed, steps);
    } else {
        for (int i = 0; i < scene_count(); i++) {
            const Scene *s = scene_by_index(i);
            bench_one(s, mu, dt, have_thr ? thr : s->threshold, seed, steps);
        }
    }
    printf("\n");
    return 0;
}

/* ------------------------------------------------------------------- main */

typedef struct {
    Sim         *l, *r;
    const Scene *scene;
    double       mu, dt, thr;
    unsigned long seed;
} ResetCtx;

static void do_reset(void *ud)
{
    ResetCtx *c = (ResetCtx *)ud;
    sim_clear(c->l);
    sim_clear(c->r);
    scene_build(c->scene, &c->l->cat, c->mu, c->seed);
    scene_build(c->scene, &c->r->cat, c->mu, c->seed);
}

int main(int argc, char **argv)
{
    const char *scene_id = "debris-field";
    unsigned long seed = 12345;
    double threshold = 0.060, dt = 0.010, mu = 1.0, budget = 10.0;
    int frames = 0, vsync = 1, bench = 0, have_scene = 0, have_threshold = 0;
    const char *shot_path = NULL;
    size_t steps = 400;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (streq(a, "--help") || streq(a, "-h")) { usage(argv[0]); return 0; }
        else if (streq(a, "--list-scenes"))            { list_scenes(); return 0; }
        else if (streq(a, "--no-vsync"))               { vsync = 0; }
        else if (streq(a, "--bench"))                  { bench = 1; }
        else {
            const char *v = (i + 1 < argc) ? argv[++i] : NULL;
            if      (streq(a, "--scene") && v)     { scene_id = v; have_scene = 1; }
            else if (streq(a, "--seed") && v)      { seed = strtoul(v, NULL, 10); }
            else if (streq(a, "--threshold") && v) { threshold = atof(v); have_threshold = 1; }
            else if (streq(a, "--dt") && v)        { dt = atof(v); }
            else if (streq(a, "--mu") && v)        { mu = atof(v); }
            else if (streq(a, "--budget") && v)    { budget = atof(v); }
            else if (streq(a, "--frames") && v)    { frames = atoi(v); }
            else if (streq(a, "--steps") && v)     { steps = (size_t)strtoul(v, NULL, 10); }
            else if (streq(a, "--shot") && v)      { shot_path = v; }
            else { fprintf(stderr, "unknown or incomplete option: %s\n\n", a); usage(argv[0]); return 2; }
        }
    }

    const Scene *scene = scene_by_id(scene_id);
    if (!scene) {
        char *end = NULL;
        long idx = strtol(scene_id, &end, 10);
        if (end && *end == '\0') scene = scene_by_index((int)idx);
    }
    if (!scene) { fprintf(stderr, "no such scene: %s\n", scene_id); list_scenes(); return 2; }

    if (bench) return run_bench(have_scene ? scene : NULL, mu, dt, threshold, have_threshold, seed, steps);

    double eff_thr = have_threshold ? threshold : scene->threshold;

    /* two engines, identical world */
    Sim left, right;
    sim_init(&left,  ENGINE_KD,    scene->phys, mu, dt, eff_thr, scene->collision_dist, scene->remove_on_collision);
    sim_init(&right, ENGINE_BRUTE, scene->phys, mu, dt, eff_thr, scene->collision_dist, scene->remove_on_collision);
    scene_build(scene, &left.cat,  mu, seed);
    scene_build(scene, &right.cat, mu, seed);

    GuiOptions opt;
    opt.scene_name = scene->name;
    opt.budget_ms = budget;
    opt.max_frames = frames;
    opt.vsync = vsync;
    opt.shot_path = shot_path;

    ResetCtx rctx = { &left, &right, scene, mu, dt, eff_thr, seed };
    opt.on_reset = do_reset;
    opt.reset_ud = &rctx;

    printf("scene=%s objects=%lu threshold=%.3f  [START/STOP/RESET buttons, ESC=quit, SPACE=pause]\n",
           scene->id, (unsigned long)left.cat.count, eff_thr);

    int rc = gui_run(&left, &right, &opt);

    sim_free(&left);
    sim_free(&right);
    return rc;
}
