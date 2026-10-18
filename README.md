# Astrosis-C

A lightweight spatial indexing & conjunction screening engine, written from
scratch in C11. Astrosis-C is deliberately *not* a multithreaded mission-control
system: it is a small, sharp demonstration of one computer-science trade-off —
**custom pointer-based spatial structures beat linear scanning as the number of
tracked objects grows.**

It ships a two-panel live view: the identical simulation runs twice, each panel using a selectable spatial index (KD-tree, brute force, uniform grid, loose octree, AABB BVH). Step count, conjunction/collision totals, and per-step screening time are shown under each panel so trade-offs are visible in real time.

```
 +----------------------------+   +----------------------------+
 |      KD TREE  O(N LOG N)   |   |       BRUTE  O(N^2)        |
 |                            |   |                            |
 |          .  .   (earth)    |   |          .  .   (earth)    |
 |        .  o .  .           |   |        .  o .  .           |
 |           .    .           |   |           .    .           |
 |                            |   |                            |
 | STEPS 694                  |   | STEPS 118                  |
 | CONJ 10241   COLL 0        |   | CONJ 10241   COLL 0        |
 | SCREEN 1.21 MS AVG 1.08 MS |   | SCREEN 9.83 MS AVG 9.02 MS |
 +----------------------------+   +----------------------------+
```

## Architecture

| Module | File | Owner | Structure / logic | Purpose |
|-------:|------|:-----:|-------------------|---------|
| 1 | `src/catalog.c` | Member 1 | **Doubly linked list** | O(1) insert / remove registry of live objects (launches and de-orbits) |
| 2 | `src/physics.c` | Member 2 | Minimal 2-body motion | Keeps objects moving: linear drift or central gravity |
| 3 | `src/kd_tree.c` | DS | **3D KD-tree** | Recursive space partitioning + radius pruning: O(n log n) build, O(log n) query |
| 4 | `src/brute_force.c` | DS | **Array-matrix scan** | O(n^2) baseline |
| 5 | `src/grid.c` | DS | **Uniform grid / spatial hash** | Cell binning + neighbor queries |
| 6 | `src/octree.c` | DS | **Loose octree** | Hierarchical space partitioning with loose bounds |
| 7 | `src/bvh.c` | DS | **AABB BVH** | Object-partitioning bounding volume hierarchy |

Supporting files: `scenes.c` (fixed scenarios), `sim.c` (an "engine" = catalog +
physics + one screening algorithm), `main.c` (driver), plus one of two GUI
front-ends: `viz.c` (SDL2) on Linux/macOS, `win32_ui.c` (native Win32) on Windows.

## Build

### Linux / macOS
Requires a C11 compiler and SDL2 development headers.

```sh
make            # builds ./astro
./astro         # run the two-panel visual
./astro --bench # headless timing table
make debug      # ASan/UBSan build (valgrind not required)
```

### Windows (MinGW-w64 / MSYS2)
The Windows build is **dependency-free**: it uses the native Win32 API (real
`BUTTON`/`STATIC` controls, a status bar, the system font) and GDI for the space
view, so **SDL2 is not required**.

```bat
:: from a "MSYS2 MINGW64" shell or cmd.exe with gcc on PATH
build.bat
```

* MSYS2: `pacman -S mingw-w64-x86_64-gcc`, then run `build.bat`.

The build produces two executables:
* `astro_gui.exe` — windowed, no console (double-click to run the visual).
* `astro.exe` — console, so `astro.exe --bench` prints its table (also opens the GUI).

The shared core is standard C11 and POSIX-free (no `unistd.h`, no `getopt`, no
`ioctl`); the console stays attached so `astro.exe --bench` still prints its
table. On Linux the optional `SDL2_ttf` library gives anti-aliased system-font
text, with an embedded 3x5 bitmap font as a no-dependency fallback
(`-DASTRO_NO_TTF` forces it).

## Run

```sh
./astro                          # default scene: debris-field (3000 objects)
./astro --scene kessler          # 15,000 objects
./astro --scene collision-cascade
./astro --shot frame.bmp         # save the final frame (SDL front-end)
./astro --list-scenes
./astro --bench --steps 500
```

The window has **START / STOP / RESET** buttons, two captioned panels (engine selectable per panel), and a status bar. On Windows these are genuine Win32 controls (authentic look); on Linux the same layout is drawn with SDL2. Each panel shows the same scene and differs only in its screening engine. The camera auto-rotates; the central body
is a shaded blue-green planet, satellites are small dots, and any pair closer
than the conjunction threshold is drawn in red with a link. On the
`collision-cascade` scene, objects that actually touch are highlighted yellow and
then **removed from the catalog** (an O(1) linked-list delete). RESET rebuilds
the scene from the same seed, so the two panels always restart identically.
**ESC** quits, **SPACE** toggles pause.

## Benchmark

`./astro --bench --steps 300 --engine-left kd --engine-right brute` (identical pair sets verified on every row). Engine names: `kd`, `brute`, `grid`, `octree`, `bvh`.

```
  close-encounter    N=2     steps=300    pairs=0         kd=    0.22 ms  brute=     0.20 ms  x   0.9  PARITY OK
  shell-crossing     N=120   steps=300    pairs=9287      kd=    6.20 ms  brute=     3.94 ms  x   0.6  PARITY OK
  dense-ring         N=400   steps=300    pairs=2418      kd=   26.73 ms  brute=    36.11 ms  x   1.4  PARITY OK
  collision-cascade  N=80    steps=300    pairs=2029      kd=    2.82 ms  brute=     1.58 ms  x   0.6  PARITY OK
  debris-field       N=3000  steps=300    pairs=43397     kd=  411.35 ms  brute=  3240.68 ms  x   7.9  PARITY OK
  kessler            N=15000 steps=300    pairs=279978    kd= 3212.49 ms  brute= 98424.07 ms  x  30.6  PARITY OK
```

The `PARITY OK` flag means both algorithms reported the exact same set of
conjunctions and collisions, so the comparison is measuring speed, not
correctness — the benchmark doubles as the test suite.

## Why the KD-tree wins (and when it doesn't)

The KD-tree rebuilds every step (objects move) and its cost is included in the
KD timer, so the comparison is honest. Two regimes exist:

* **Small N (tens–hundreds).** Pointer chasing, the per-step rebuild, and the
  loop overhead dominate; brute force over a compact list can win. This is why
  the small scenes show a factor near 1x or slightly below.
* **Large N (thousands+).** Brute force performs `n(n-1)/2` distance tests per
  step — 15,000 objects means ~112 million tests. The KD-tree prunes whole
  subtrees and checks only local neighbours, giving the 8x–31x wins above. This
  is the O(n^2) vs O(n log n) crossover, shown empirically.

Note that when conjunctions are *dense* (a large fraction of all pairs are
within range), the answer set itself is O(n^2), so no spatial index can be
asymptotically better — the benchmark deliberately uses sparse thresholds for
the large scenes to isolate the search cost.


## CLI options (highlights)

```text
--engine-left <kd|brute|grid|octree|bvh>  engine for left panel (default kd)
--engine-right <kd|brute|grid|octree|bvh> engine for right panel (default brute)
--dt <f>             step size / timestep
--mu <f>             gravity constant
--threshold <f>      conjunction distance
--collision-dist <f> override collision distance
--remove-on-collision <0|1>
--grid-cell <f>      grid cell size (0=auto)
--octree-loose <f>   loose factor (>=1.0)
--octree-max-depth <i>
--octree-min-cell <f>
--bvh-max-leaf <i>
--bvh-sah <0|1>
```
