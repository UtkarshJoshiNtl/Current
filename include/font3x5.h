/*
 * font3x5.h - a hand-built 3x5 pixel font, uppercase only.
 *
 * Why not SDL_ttf? A font file is an extra runtime dependency to ship on
 * Windows. A 3x5 bitmap font is a few hundred bytes, embedded in the binary,
 * and scales crisply (we draw it at 2x-3x). Only the glyphs actually needed
 * for the labels are defined; unknown characters render as blank.
 */
#ifndef FONT3X5_H
#define FONT3X5_H

typedef struct { const char *r[5]; } Glyph;

static inline Glyph glyph_for(char c)
{
    Glyph g;
    switch (c) {
    case '0': { static const Glyph x = {{"###", "#.#", "#.#", "#.#", "###"}}; g = x; break; }
    case '1': { static const Glyph x = {{".#.", "##.", ".#.", ".#.", "###"}}; g = x; break; }
    case '2': { static const Glyph x = {{"###", "..#", "###", "#..", "###"}}; g = x; break; }
    case '3': { static const Glyph x = {{"###", "..#", "###", "..#", "###"}}; g = x; break; }
    case '4': { static const Glyph x = {{"#.#", "#.#", "###", "..#", "..#"}}; g = x; break; }
    case '5': { static const Glyph x = {{"###", "#..", "###", "..#", "###"}}; g = x; break; }
    case '6': { static const Glyph x = {{"###", "#..", "###", "#.#", "###"}}; g = x; break; }
    case '7': { static const Glyph x = {{"###", "..#", "..#", "..#", "..#"}}; g = x; break; }
    case '8': { static const Glyph x = {{"###", "#.#", "###", "#.#", "###"}}; g = x; break; }
    case '9': { static const Glyph x = {{"###", "#.#", "###", "..#", "###"}}; g = x; break; }
    case 'A': { static const Glyph x = {{"###", "#.#", "###", "#.#", "#.#"}}; g = x; break; }
    case 'B': { static const Glyph x = {{"##.", "#.#", "##.", "#.#", "##."}}; g = x; break; }
    case 'C': { static const Glyph x = {{"###", "#..", "#..", "#..", "###"}}; g = x; break; }
    case 'D': { static const Glyph x = {{"##.", "#.#", "#.#", "#.#", "##."}}; g = x; break; }
    case 'E': { static const Glyph x = {{"###", "#..", "###", "#..", "###"}}; g = x; break; }
    case 'F': { static const Glyph x = {{"###", "#..", "###", "#..", "#.."}}; g = x; break; }
    case 'G': { static const Glyph x = {{"###", "#..", "#.#", "#.#", "###"}}; g = x; break; }
    case 'H': { static const Glyph x = {{"#.#", "#.#", "###", "#.#", "#.#"}}; g = x; break; }
    case 'I': { static const Glyph x = {{"###", ".#.", ".#.", ".#.", "###"}}; g = x; break; }
    case 'J': { static const Glyph x = {{"..#", "..#", "..#", "#.#", "###"}}; g = x; break; }
    case 'K': { static const Glyph x = {{"#.#", "#.#", "##.", "#.#", "#.#"}}; g = x; break; }
    case 'L': { static const Glyph x = {{"#..", "#..", "#..", "#..", "###"}}; g = x; break; }
    case 'M': { static const Glyph x = {{"#.#", "###", "###", "#.#", "#.#"}}; g = x; break; }
    case 'N': { static const Glyph x = {{"##.", "#.#", "#.#", "#.#", "#.#"}}; g = x; break; }
    case 'O': { static const Glyph x = {{"###", "#.#", "#.#", "#.#", "###"}}; g = x; break; }
    case 'P': { static const Glyph x = {{"###", "#.#", "###", "#..", "#.."}}; g = x; break; }
    case 'Q': { static const Glyph x = {{"###", "#.#", "#.#", "###", "..#"}}; g = x; break; }
    case 'R': { static const Glyph x = {{"###", "#.#", "###", "##.", "#.#"}}; g = x; break; }
    case 'S': { static const Glyph x = {{"###", "#..", "###", "..#", "###"}}; g = x; break; }
    case 'T': { static const Glyph x = {{"###", ".#.", ".#.", ".#.", ".#."}}; g = x; break; }
    case 'U': { static const Glyph x = {{"#.#", "#.#", "#.#", "#.#", "###"}}; g = x; break; }
    case 'V': { static const Glyph x = {{"#.#", "#.#", "#.#", "#.#", ".#."}}; g = x; break; }
    case 'W': { static const Glyph x = {{"#.#", "#.#", "###", "###", "#.#"}}; g = x; break; }
    case 'X': { static const Glyph x = {{"#.#", "#.#", ".#.", "#.#", "#.#"}}; g = x; break; }
    case 'Y': { static const Glyph x = {{"#.#", "#.#", ".#.", ".#.", ".#."}}; g = x; break; }
    case 'Z': { static const Glyph x = {{"###", "..#", ".#.", "#..", "###"}}; g = x; break; }
    case '.': { static const Glyph x = {{"...", "...", "...", "...", ".#."}}; g = x; break; }
    case ',': { static const Glyph x = {{"...", "...", "...", ".#.", "#.."}}; g = x; break; }
    case ':': { static const Glyph x = {{"...", ".#.", "...", ".#.", "..."}}; g = x; break; }
    case '-': { static const Glyph x = {{"...", "...", "###", "...", "..."}}; g = x; break; }
    case '+': { static const Glyph x = {{"...", ".#.", "###", ".#.", "..."}}; g = x; break; }
    case '/': { static const Glyph x = {{"..#", "..#", ".#.", "#..", "#.."}}; g = x; break; }
    case '(': { static const Glyph x = {{".#.", "#..", "#..", "#..", ".#."}}; g = x; break; }
    case ')': { static const Glyph x = {{".#.", "..#", "..#", "..#", ".#."}}; g = x; break; }
    case '^': { static const Glyph x = {{".#.", "#.#", "...", "...", "..."}}; g = x; break; }
    case '=': { static const Glyph x = {{"...", "###", "...", "###", "..."}}; g = x; break; }
    case '<': { static const Glyph x = {{"..#", ".#.", "#..", ".#.", "..#"}}; g = x; break; }
    case '>': { static const Glyph x = {{"#..", ".#.", "..#", ".#.", "#.."}}; g = x; break; }
    case '*': { static const Glyph x = {{"#.#", ".#.", "###", ".#.", "#.#"}}; g = x; break; }
    default:  { static const Glyph x = {{"...", "...", "...", "...", "..."}}; g = x; break; }
    }
    return g;
}

#endif /* FONT3X5_H */
