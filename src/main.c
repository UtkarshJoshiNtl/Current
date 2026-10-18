/* main.c - Astrosis-C driver: visual comparison + headless benchmark */
#include "scenes.h"
#include "sim.h"
#include "gui.h"
#include "engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static void usage(const char *prog)
{
    printf(
        "ASTROSIS-C - spatial indexing & conjunction screening battles\n"
        "usage: %s [options]\n"
        "  --scene <id|index>   scenario to run (default: debris-field)\n"
        "  --list-scenes        print scenarios and exit\n"
        "  --seed <n>           RNG seed (default 12345)\n"
        "  --engine-left <e>    left panel engine  (default kd)\n"
        "  --engine-right <e>   right panel engine (default brute)\n"
        "                       e = kd | brute | grid | octree | bvh\n"
        "  --threshold <f>      conjunction distance (scene default)\n"
        "  --collision-dist <f> contact distance (scene default)\n"
        "  --remove-on-collision <0|1>  (scene default)\n"
        "  --dt <f>             step size / timestep (default 0.010)\n"
        "  --mu <f>             gravity constant (default 1.0)\n"
        "  --budget <ms>        per-panel compute budget per frame (default 10.0)\n"
        "  --frames <n>         quit after n frames (default 0 = until closed)\n"
        "  --no-vsync           disable vertical sync\n"
        "  --shot <file.bmp>    save a BMP of the final frame\n"
        "  --bench              headless benchmark instead of the window\n"
        "  --steps <n>          steps per scene in benchmark (default 400)\n"
        "  engine tuning:\n"
        "  --grid-cell <f>      grid cell size (default 0 = auto)\n"
        "  --octree-loose <f>   loose factor (default 2.0)\n"
        "  --octree-depth <n>   max depth (default 12)\n"
        "  --bvh-leaf <n>       objects per leaf (default 4)\n"
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
                      unsigned long seed, size_t steps,
                      EngineType el, EngineType er)
{
    Sim l, r;
    sim_init(&l, el,  sc->phys, mu, dt, thr, sc->collision_dist, sc->remove_on_collision);
    sim_init(&r, er,  sc->phys, mu, dt, thr, sc->collision_dist, sc->remove_on_collision);
    scene_build(sc, &l.cat, mu, seed);
    scene_build(sc, &r.cat, mu, seed);

    size_t n0 = l.cat.count;
    for (size_t i = 0; i < steps; i++) {
        sim_step(&l);
        sim_step(&r);
    }

    double l_ms = l.screen_ms_total, r_ms = r.screen_ms_total;
    double speed = l_ms > 0.0 ? r_ms / l_ms : 0.0;
    int parity = (l.conj_total == r.conj_total) && (l.coll_total == r.coll_total);

    printf("  %-18s N=%-5lu steps=%-6lu pairs=%-8lu  %s=%8.2f ms  %s=%9.2f ms  x%6.1f  %s\n",
           sc->id, (unsigned long)n0, (unsigned long)steps,
           (unsigned long)l.conj_total,
           engine_type_name(el), l_ms, engine_type_name(er), r_ms, speed,
           parity ? "PARITY OK" : "PARITY MISMATCH");

    sim_free(&l);
    sim_free(&r);
}

static int run_bench(const Scene *only, double mu, double dt, double thr,
                     int have_thr, unsigned long seed, size_t steps,
                     EngineType el, EngineType er)
{
    printf("\nASTROSIS-C benchmark  (same physics, identical pair sets expected)\n"
           "  engines: %s vs %s\n\n",
           engine_type_name(el), engine_type_name(er));
    if (only) {
        bench_one(only, mu, dt, have_thr ? thr : only->threshold, seed, steps, el, er);
    } else {
        for (int i = 0; i < scene_count(); i++) {
            const Scene *s = scene_by_index(i);
            bench_one(s, mu, dt, have_thr ? thr : s->threshold, seed, steps, el, er);
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
    double collision_dist = -1.0;   /* -1 = scene default */
    int frames = 0, vsync = 1, bench = 0, have_scene = 0, have_threshold = 0;
    int remove_on_collision = -1;   /* -1 = scene default */
    const char *shot_path = NULL;
    size_t steps = 400;
    const char *eng_left = "kd", *eng_right = "brute";
    GridParams gp;    gp.cell = 0.0;    gp.periodic = 0;
    gp.world_min = 0.0; gp.world_max = 0.0;
    OctreeParams op;  op.loose = 2.0;  op.max_depth = 12; op.min_cell = 1e-4;
    BVHParams bp;     bp.max_leaf = 4; bp.sah = 1;

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
            else if (streq(a, "--engine-left") && v)  { eng_left = v; }
            else if (streq(a, "--engine-right") && v) { eng_right = v; }
            else if (streq(a, "--threshold") && v) { threshold = atof(v); have_threshold = 1; }
            else if (streq(a, "--collision-dist") && v) { collision_dist = atof(v); }
            else if (streq(a, "--remove-on-collision") && v) { remove_on_collision = atoi(v); }
            else if (streq(a, "--dt") && v)        { dt = atof(v); }
            else if (streq(a, "--mu") && v)        { mu = atof(v); }
            else if (streq(a, "--budget") && v)    { budget = atof(v); }
            else if (streq(a, "--frames") && v)    { frames = atoi(v); }
            else if (streq(a, "--steps") && v)     { steps = (size_t)strtoul(v, NULL, 10); }
            else if (streq(a, "--shot") && v)      { shot_path = v; }
            else if (streq(a, "--grid-cell") && v)     { gp.cell = atof(v); }
            else if (streq(a, "--octree-loose") && v)  { op.loose = atof(v); }
            else if (streq(a, "--octree-depth") && v)  { op.max_depth = atoi(v); }
            else if (streq(a, "--bvh-leaf") && v)      { bp.max_leaf = atoi(v); }
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

    EngineType el = engine_type_from_name(eng_left);
    EngineType er = engine_type_from_name(eng_right);

    if (bench)
        return run_bench(have_scene ? scene : NULL, mu, dt, threshold,
                         have_threshold, seed, steps, el, er);

    double eff_thr = have_threshold ? threshold : scene->threshold;
    double eff_cd  = (collision_dist >= 0.0) ? collision_dist : scene->collision_dist;
    int eff_roc    = (remove_on_collision >= 0) ? (remove_on_collision != 0)
                                                : scene->remove_on_collision;

    /* two engines, identical world */
    Sim left, right;
    sim_init(&left,  el, scene->phys, mu, dt, eff_thr, eff_cd, eff_roc);
    sim_init(&right, er, scene->phys, mu, dt, eff_thr, eff_cd, eff_roc);
    sim_set_grid_params(&left, &gp);   sim_set_grid_params(&right, &gp);
    sim_set_octree_params(&left, &op); sim_set_octree_params(&right, &op);
    sim_set_bvh_params(&left, &bp);    sim_set_bvh_params(&right, &bp);
    scene_build(scene, &left.cat,  mu, seed);
    scene_build(scene, &right.cat, mu, seed);

    GuiOptions opt;
    opt.scene_name = scene->name;
    opt.budget_ms = budget;
    opt.max_frames = frames;
    opt.vsync = vsync;
    opt.shot_path = shot_path;
    opt.engine_left = el;
    opt.engine_right = er;

    ResetCtx rctx = { &left, &right, scene, mu, dt, eff_thr, seed };
    opt.on_reset = do_reset;
    opt.reset_ud = &rctx;

    printf("scene=%s objects=%lu threshold=%.3f dt=%.4f  engines=%s vs %s\n"
           "  [engine pickers, START/STOP/RESET, ESC=quit, SPACE=pause]\n",
           scene->id, (unsigned long)left.cat.count, eff_thr, dt,
           engine_type_name(el), engine_type_name(er));

    int rc = gui_run(&left, &right, &opt);

    sim_free(&left);
    sim_free(&right);
    return rc;
}
