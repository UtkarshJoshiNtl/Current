# Spatial indexing comparison

This is my C project for a data structures class. It compares several ways to
find nearby pairs of moving objects. The same starting scene is shown in two
panels, with a different spatial data structure selected for each one.

Each panel advances independently. A faster engine can complete more simulation
steps while the slower one is still working, which makes the difference easy to
see in the live view. The step counts are therefore expected to differ. Use the
benchmark when you want both engines to process the same fixed number of steps
and compare their measured screening time.

The simulation uses simple central-gravity orbits and checks for close
approaches (conjunctions) and collisions. It is a small visual demonstration;
it is not intended to model real spacecraft or a full N-body gravity problem.

## What the project compares

| Structure | How it is used |
|---|---|
| KD-tree | Splits 3D space into regions to skip distant objects. |
| Brute force | Checks every possible pair; serves as the simple baseline. |
| Uniform grid | Groups objects into cells and checks nearby cells. |
| Loose octree | Subdivides 3D space into nested regions. |
| AABB BVH | Groups objects in bounding boxes to prune pair checks. |

The simulation core uses a catalog to track objects, applies simple motion,
and passes nearby-pair screening through a shared engine interface. The Windows
front end uses native Win32 controls and GDI; the SDL front end is available on
Linux and macOS.

## Build and run

### Windows

Install the MinGW-w64 compiler (for example through MSYS2), then run:

```bat
build.bat
```

This creates `astro_gui.exe` for opening the visual directly and `astro.exe`
for running the visual or command-line benchmark.

### Linux or macOS

Install a C11 compiler and SDL2 development files, then run:

```sh
make
./astro
```

The SDL2_ttf library is optional. Without it, the program uses its built-in
bitmap font.

## Using the Windows window

- **Start**, **Stop**, and **Reset** control the simulation. Reset returns both
  panels to the same starting scene.
- Each panel has its own engine selector above the viewport. Changing one does
  not change the other.
- Hover over controls and panel headings for short explanations.
- **Remove on collision** controls whether colliding objects are removed.
- The counters show the number of simulation steps, close conjunctions, and
  collisions recorded by that panel.
- Press **Space** to pause or resume. Press **Esc** to close the window.

The two panels use the same scene and starting seed, but they advance at
independent speeds. Their counters describe their own progress.

## Benchmark

The benchmark runs both chosen engines through the same fixed number of steps
for each scenario and reports screening time and whether their conjunction and
collision results agree:

```sh
./astro --bench --steps 300
./astro --bench --steps 500 --engine-left kd --engine-right brute
```

Available engine IDs are `kd`, `brute`, `grid`, `octree`, and `bvh`. To select a
single scenario, add `--scene` followed by its ID, such as `--scene debris-field`.
Use `./astro --list-scenes` to see the available scenarios.

The benchmark is useful for comparing measured work under the same step count.
Small scenes can favor brute force because it has little setup overhead. Larger
scenes can benefit from spatial indexes because they avoid checking every pair.
When many objects are close together, every method still has to report many
pairs.

## Other command-line settings

The GUI uses the selected scene's normal settings to keep the focus on the data
structures. The command line still exposes simulation and engine tuning for
experiments:

```text
--dt <value>                 simulation step size
--mu <value>                 central gravity strength
--threshold <value>          conjunction distance
--collision-dist <value>     collision distance
--remove-on-collision <0|1>  whether collisions remove objects
--seed <number>              starting random seed
--grid-cell <value>          grid cell size (0 selects automatically)
--octree-loose <value>       loose-octree factor
--octree-depth <number>      maximum octree depth
--bvh-leaf <number>          objects per BVH leaf
```
