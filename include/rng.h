/*
 * rng.h - tiny deterministic PRNG (splitmix64).
 *
 * We deliberately avoid rand(): its output differs between libc/compilers,
 * which would break the "both panels run the SAME simulation" requirement on
 * Windows vs Linux. All arithmetic here uses unsigned 64-bit wraparound,
 * which is well-defined in C11, so sequences are bit-identical everywhere.
 */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

typedef struct { uint64_t s; } Rng;

static inline void rng_seed(Rng *r, uint64_t seed) { r->s = seed ? seed : 0x9E3779B97F4A7C15ULL; }

static inline uint64_t rng_u64(Rng *r)
{
    uint64_t z = (r->s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* uniform in [0,1) */
static inline double rng_double(Rng *r)
{
    return (double)(rng_u64(r) >> 11) * (1.0 / 9007199254740992.0);
}

/* uniform in [lo,hi) */
static inline double rng_range(Rng *r, double lo, double hi)
{
    return lo + (hi - lo) * rng_double(r);
}

#endif /* RNG_H */
