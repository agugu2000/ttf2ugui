/*
 * Copyright (c) 2015, Ari Suutari <ari@stonepile.fi>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT,  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  *  19.10.2017 jkpublic@kartech.biz - Added support for 8BPP fonts ( anti alliased)
  *
  */

/*
 * New format (math-correct glyph model):
 *   header(20) + codepoints + metrics + data_offsets + data
 *
 * Each glyph has its own ink w/h, x_off, y_off, adv.
 * The font also carries ascender / descender in the header,
 * used by the renderer to position the baseline.
 * Bitmap stored tight (no cell, no padding).
 * Binary search over codepoints.
 *
 * CLI:
 *   --font=FILE --size=N [--dpi=N] [--bpp=1|8] [--chars=LIST]
 *   [--dump] [--preview --text=TEXT]
 *
 *   --preview writes preview.bmp (512x512, 8bpp indexed) into the
 *   current working directory (same place as the generated .c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H

#include "ugui.h"

#define DEFAULT_CHARS  "32-126"
#define MAX_GLYPH_W    4096
#define MAX_GLYPH_H    4096
#define MAX_BEARING    32767

#define PREVIEW_W      512
#define PREVIEW_H      512

typedef struct { uint16_t *cp; uint32_t n, cap; } CpVec;
typedef struct { uint8_t *p; size_t n, cap; } ByteVec;

static void cps_push(CpVec *v, uint16_t c) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 1024;
        v->cp = realloc(v->cp, v->cap * sizeof(uint16_t));
        if (!v->cp) { fprintf(stderr, "oom\n"); exit(1); }
    }
    v->cp[v->n++] = c;
}

static void bv_push(ByteVec *v, uint8_t b) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 4096;
        v->p = realloc(v->p, v->cap);
        if (!v->p) { fprintf(stderr, "oom\n"); exit(1); }
    }
    v->p[v->n++] = b;
}

static float fontSize = 0;
static int   dpi = 0, bpp = 1;
static int   opt_dump = 0;
static int   opt_preview = 0;
static char *opt_text = NULL;
static char *opt_font = NULL;
static char *opt_chars = NULL;
static int   opt_list_faces  = 0;
static int   opt_face_index  = 0;
static int   opt_weight      = -1;   /* -1 = not set */
static int   opt_shadow      = 1;

typedef struct {
    uint16_t w, h;
    int16_t  x_off, y_off;
    uint16_t adv;
    ByteVec  bitmap;
} Glyph;

static int is_digit(int c) { return c >= '0' && c <= '9'; }

static void parse_chars(CpVec *v, const char *s) {
    /* Deduplication bitmap: one bit per codepoint (0x0000-0xFFFF) */
    uint8_t *seen = calloc(0x10000 / 8, 1);
    if (!seen) { fprintf(stderr, "oom\n"); exit(1); }

    while (*s) {
        while (*s && !is_digit((unsigned char)*s)) s++;
        if (!*s) break;
        unsigned long l = 0;
        while (is_digit((unsigned char)*s)) { l = l*10 + (*s - '0'); s++; }
        unsigned long r = l;
        if (*s == '-' || *s == '_') {
            s++; r = 0;
            while (is_digit((unsigned char)*s)) { r = r*10 + (*s - '0'); s++; }
        }
        if (l > 0xFFFF || r > 0xFFFF || l > r) {
            fprintf(stderr, "bad range %lu-%lu\n", l, r);
            free(seen);
            exit(1);
        }
        for (unsigned long c = l; c <= r; c++) {
            if (seen[c >> 3] & (1 << (c & 7))) continue;
            seen[c >> 3] |= (1 << (c & 7));
            cps_push(v, (uint16_t)c);
        }
    }
    free(seen);

    if (v->n == 0) { fprintf(stderr, "no chars\n"); exit(1); }
}

static void list_faces(const char *path)
{
    FT_Library lib;
    FT_Face    face;
    FT_Error   err;

    if ((err = FT_Init_FreeType(&lib))) {
        fprintf(stderr, "FT init %d\n", err);
        exit(1);
    }

    /* Count faces in the file (TTC may contain several) */
    FT_Long num_faces = 0;
    {
        FT_Face tmp;
        err = FT_New_Face(lib, path, -1, &tmp);
        if (err == 0) {
            num_faces = tmp->num_faces;
            FT_Done_Face(tmp);
        }
    }
    if (num_faces <= 0) num_faces = 1;

    for (FT_Long i = 0; i < num_faces; i++) {
        err = FT_New_Face(lib, path, i, &face);
        if (err) {
            fprintf(stderr, "face %ld: <cannot load, err %d>\n", i, err);
            continue;
        }

        /* usWeightClass from OS/2 table, if present */
        int weight = -1;
        if (face->style_flags & FT_STYLE_FLAG_BOLD) weight = 700;

        /* Variable font weight axis default */
        FT_MM_Var *mm = NULL;
        if (FT_Get_MM_Var(face, &mm) == 0) {
            for (FT_UInt a = 0; a < mm->num_axis; a++) {
                if (mm->axis[a].tag == FT_MAKE_TAG('w','g','h','t')) {
                    weight = (int)(mm->axis[a].def / 65536);
                    break;
                }
            }
            FT_Done_MM_Var(lib, mm);
        }

        printf("face %ld: %s, style \"%s\"",
               i,
               face->family_name ? face->family_name : "?",
               face->style_name  ? face->style_name  : "?");
        if (weight > 0) printf(", weight %d", weight);
        printf("\n");

        FT_Done_Face(face);
    }

    FT_Done_FreeType(lib);
}

static void convert_font(const char *path, CpVec *cps, Glyph **out,
                         uint16_t *omw, uint16_t *omh, uint16_t *onotdef_adv,
                         int16_t *oascender, int16_t *odescender)
{
    FT_Library lib; FT_Face face; FT_Error err;
    if ((err = FT_Init_FreeType(&lib))) { fprintf(stderr, "FT init %d\n", err); exit(1); }
    if ((err = FT_New_Face(lib, path, opt_face_index, &face))) {
        fprintf(stderr, "FT face %d\n", err); exit(1);
    }

    /* Variable font weight (only if --weight given and font has "wght" axis) */
    if (opt_weight >= 0) {
        FT_MM_Var *mm = NULL;
        if (FT_Get_MM_Var(face, &mm) == 0) {
            FT_UInt wght_idx = 0;
            int found = 0;
            for (FT_UInt a = 0; a < mm->num_axis; a++) {
                if (mm->axis[a].tag == FT_MAKE_TAG('w','g','h','t')) {
                    wght_idx = a;
                    found = 1;
                    break;
                }
            }
            if (found) {
                FT_Fixed coords[16] = {0};
                FT_UInt n = mm->num_axis;
                if (n > 16) n = 16;
                for (FT_UInt a = 0; a < n; a++) coords[a] = mm->axis[a].def;
                coords[wght_idx] = (FT_Fixed)opt_weight * 65536;
                FT_Set_Var_Design_Coordinates(face, n, coords);
            } else {
                fprintf(stderr, "note: font has no 'wght' axis, --weight ignored\n");
            }
            FT_Done_MM_Var(lib, mm);
        } else {
            fprintf(stderr, "note: font is not variable, --weight ignored\n");
        }
    }
    if (dpi > 0) err = FT_Set_Char_Size(face, 0, (FT_F26Dot6)(fontSize * 64.0f), dpi, dpi);
    else         err = FT_Set_Pixel_Sizes(face, 0, (FT_UInt)fontSize);
    if (err) { fprintf(stderr, "FT size %d\n", err); exit(1); }

    /* Industry standard: font-wide metrics (read after FT_Set_*_Size) */
    int16_t ascender  = (int16_t)(face->size->metrics.ascender  >> 6);
    int16_t descender = (int16_t)(face->size->metrics.descender >> 6);

    /* Load .notdef to get its advance, used as placeholder for missing glyphs */
    uint16_t notdef_adv = 0;
    {
        FT_UInt nd_idx = 0;
        if (FT_Load_Glyph(face, nd_idx, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) == 0) {
            notdef_adv = (uint16_t)(face->glyph->advance.x >> 6);
        }
    }

    Glyph *gs = calloc(cps->n, sizeof(Glyph));
    if (!gs) { fprintf(stderr, "oom\n"); exit(1); }

    CpVec    cps_valid = {0};
    uint32_t nvalid    = 0;
    uint16_t maxw = 0, maxh = 0;

    for (uint32_t i = 0; i < cps->n; i++) {
        FT_UInt cp = cps->cp[i];

        /* Missing-glyph detection: FT_Get_Char_Index returns 0 when the
         * codepoint is not present in this font (glyph index 0 is always
         * reserved for .notdef). Report and skip. */
        FT_UInt glyph_index = FT_Get_Char_Index(face, cp);
        if (glyph_index == 0) {
            fprintf(stderr, "[missing] U+%04X not present in font, skipped\n", cp);
            continue;
        }

        FT_Int32 fl = (bpp == 8) ? (FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_BITMAP)
                                 : (FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
        if ((err = FT_Load_Char(face, cp, fl))) {
            fprintf(stderr, "[error] U+%04X FT_Load_Char failed %d, skipped\n", cp, err);
            continue;
        }

        FT_Bitmap *bm = &face->glyph->bitmap;
        Glyph *g = &gs[nvalid];

        if (bm->width  > MAX_GLYPH_W || bm->rows > MAX_GLYPH_H ||
            face->glyph->bitmap_left < -MAX_BEARING ||
            face->glyph->bitmap_left >  MAX_BEARING ||
            face->glyph->bitmap_top  < -MAX_BEARING ||
            face->glyph->bitmap_top  >  MAX_BEARING ||
            (face->glyph->advance.x >> 6) < 0 ||
            (face->glyph->advance.x >> 6) > 65535) {
            fprintf(stderr, "[error] U+%04X metric out of range, skipped\n", cp);
            continue;
        }

        g->w     = (uint16_t)bm->width;
        g->h     = (uint16_t)bm->rows;
        g->x_off = (int16_t)face->glyph->bitmap_left;
        g->y_off = (int16_t)face->glyph->bitmap_top;
        g->adv   = (uint16_t)(face->glyph->advance.x >> 6);

        if (g->w > maxw) maxw = g->w;
        if (g->h > maxh) maxh = g->h;

        if (bpp == 1) {
            uint16_t bpr = (g->w + 7) / 8;
            for (uint16_t y = 0; y < g->h; y++) {
                for (uint16_t bx = 0; bx < bpr; bx++) {
                    uint8_t outb = 0;
                    for (uint8_t k = 0; k < 8; k++) {
                        uint16_t x = bx * 8 + k;
                        if (x >= g->w) break;
                        uint8_t byte = bm->buffer[y * bm->pitch + (x >> 3)];
                        if (byte & (0x80 >> (x & 7))) outb |= (1u << k);
                    }
                    bv_push(&g->bitmap, outb);
                }
            }
        } else {
            for (uint16_t y = 0; y < g->h; y++)
                for (uint16_t x = 0; x < g->w; x++)
                    bv_push(&g->bitmap, bm->buffer[y * bm->pitch + x]);
        }

        cps_push(&cps_valid, cp);
        nvalid++;
    }

    free(cps->cp);
    *cps = cps_valid;

    FT_Done_Face(face); FT_Done_FreeType(lib);

    *out         = gs;
    *omw         = maxw;
    *omh         = maxh;
    *onotdef_adv = notdef_adv;
    *oascender   = ascender;
    *odescender  = descender;
}

/* ------------------------------------------------------------------ */
/* Sort codepoints + glyphs together, ascending by codepoint           */
/* ------------------------------------------------------------------ */

typedef struct { uint16_t cp; Glyph g; } CpGlyphPair;

static int cmp_cp_glyph_pair(const void *a, const void *b)
{
    uint16_t ca = ((const CpGlyphPair*)a)->cp;
    uint16_t cb = ((const CpGlyphPair*)b)->cp;
    return (ca > cb) - (ca < cb);
}

static void sort_cps_glyphs(CpVec *cps, Glyph *gs)
{
    uint32_t n = cps->n;
    if (n < 2) return;

    if (n > 64) {
        CpGlyphPair *pairs = malloc(n * sizeof(CpGlyphPair));
        if (!pairs) { fprintf(stderr, "oom\n"); exit(1); }
        for (uint32_t i = 0; i < n; i++) {
            pairs[i].cp = cps->cp[i];
            pairs[i].g  = gs[i];
        }
        qsort(pairs, n, sizeof(CpGlyphPair), cmp_cp_glyph_pair);
        for (uint32_t i = 0; i < n; i++) {
            cps->cp[i] = pairs[i].cp;
            gs[i]      = pairs[i].g;
        }
        free(pairs);
    } else {
        for (uint32_t i = 1; i < n; i++) {
            uint16_t cp = cps->cp[i];
            Glyph    g  = gs[i];
            uint32_t j = i;
            while (j > 0 && cps->cp[j-1] > cp) {
                cps->cp[j] = cps->cp[j-1];
                gs[j]      = gs[j-1];
                j--;
            }
            cps->cp[j] = cp;
            gs[j]      = g;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Font name sanitizer (returns malloc'd string)                       */
/* ------------------------------------------------------------------ */

static char *sanitize_alloc(const char *path)
{
    const char *b = strrchr(path, '/');
#ifdef _WIN32
    const char *b2 = strrchr(path, '\\');
    if (!b || (b2 && b2 > b)) b = b2;
#endif
    b = b ? b + 1 : path;

    size_t len = 0;
    while (b[len] && b[len] != '.') len++;

    char *out = malloc(len + 1);
    if (!out) { fprintf(stderr, "oom\n"); exit(1); }

    for (size_t i = 0; i < len; i++) {
        char c = b[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '_')) {
            c = '_';
        }
        out[i] = c;
    }
    out[len] = 0;
    return out;
}

/* ------------------------------------------------------------------ */
/* Dump font as C array                                               */
/* ------------------------------------------------------------------ */

static void dump_font(const char *path, CpVec *cps, Glyph *gs,
                      uint16_t maxw, uint16_t maxh, uint16_t notdef_adv,
                      int16_t ascender, int16_t descender)
{
    char *base = sanitize_alloc(path);

    size_t base_len  = strlen(base);
    size_t fname_len = base_len + 12 + 1;
    size_t oname_len = base_len + 14 + 1;

    char *fname = malloc(fname_len);
    char *oname = malloc(oname_len);
    if (!fname || !oname) { fprintf(stderr, "oom\n"); exit(1); }

    snprintf(fname, fname_len, "%s_%uX%u", base, (unsigned)maxw, (unsigned)maxh);
    snprintf(oname, oname_len, "%s.c", fname);

    FILE *o = fopen(oname, "w");
    if (!o) { fprintf(stderr, "open %s: %s\n", oname, strerror(errno)); exit(1); }

    uint32_t n = cps->n;

    uint32_t total = UG_FONT_HEADER_SIZE
                   + n * UG_FONT_CODEPOINT_SIZE
                   + n * UG_FONT_METRICS_SIZE
                   + n * UG_FONT_DATA_OFFSET_SIZE;
    for (uint32_t i = 0; i < n; i++) total += (uint32_t)gs[i].bitmap.n;

    /* ---------------- file header comment ---------------- */
    fprintf(o, "// Converted from %s\n", path);
    fprintf(o, "//  --size %.1f\n", fontSize);
    if (dpi > 0) fprintf(o, "//  --dpi %d\n", dpi);
    fprintf(o, "//  --bpp %d\n", bpp);
    fprintf(o, "//\n");
    fprintf(o, "// Font array layout:\n");
    fprintf(o, "//   [header: 20 bytes]\n");
    fprintf(o, "//     [0]      font format: bit7=0 new, bit0=font type (0=1BPP, 1=8BPP)\n");
    fprintf(o, "//     [1]      reserved, must be 0\n");
    fprintf(o, "//     [2-3]    max_ink_w       2-byte big-endian\n");
    fprintf(o, "//     [4-5]    max_ink_h       2-byte big-endian\n");
    fprintf(o, "//     [6-9]    number_of_chars 4-byte big-endian\n");
    fprintf(o, "//     [10-13]  total_size      4-byte big-endian\n");
    fprintf(o, "//     [14-15]  notdef_adv      2-byte big-endian\n");
    fprintf(o, "//     [16-17]  ascender        2-byte big-endian, signed\n");
    fprintf(o, "//     [18-19]  descender       2-byte big-endian, signed\n");
    fprintf(o, "//   [codepoints:   n * 2 bytes, 2-byte big-endian, ascending]\n");
    fprintf(o, "//   [metrics:      n * 10 bytes, each field 2-byte big-endian]\n");
    fprintf(o, "//                  w(2) h(2) x_off(2 signed) y_off(2 signed) adv(2)\n");
    fprintf(o, "//   [data_offsets: n * 4 bytes, 4-byte big-endian]\n");
    fprintf(o, "//   [data:         1BPP: ceil(w/8)*h bytes per glyph, LSB-left, row-major]\n");
    fprintf(o, "//                  8BPP: w*h bytes per glyph, row-major]\n");
    fprintf(o, "//\n\n");

    fprintf(o, "#include \"ugui.h\"\n");
    fprintf(o, "#ifdef UGUI_USE_FONT_%s\n\n", fname);
    fprintf(o, "UG_FONT FONT_%s[] = {\n", fname);

    /* ---------------- header (20 bytes) ---------------- */
    fprintf(o, "  /* ============================================================ */\n");
    fprintf(o, "  /* Font header: 20 bytes                                         */\n");
    fprintf(o, "  /* ============================================================ */\n");

    fprintf(o, "  /* [0]      font format: bit7=0 new, bit0=font type (0=1BPP, 1=8BPP) */\n");
    fprintf(o, "  /* [1]      reserved, must be 0                                  */\n");
    fprintf(o, "  0x%02X,0x00,\n", (bpp == 8) ? UG_FONT_TYPE_8BPP : UG_FONT_TYPE_1BPP);

    fprintf(o, "  /* [2-3]    max_ink_w: 2-byte big-endian                         */\n");
    fprintf(o, "  0x%02X,0x%02X,\n", (maxw >> 8) & 0xFF, maxw & 0xFF);

    fprintf(o, "  /* [4-5]    max_ink_h: 2-byte big-endian                         */\n");
    fprintf(o, "  0x%02X,0x%02X,\n", (maxh >> 8) & 0xFF, maxh & 0xFF);

    fprintf(o, "  /* [6-9]    number_of_chars: 4-byte big-endian                   */\n");
    fprintf(o, "  0x%02X,0x%02X,0x%02X,0x%02X,\n",
            (n >> 24) & 0xFF, (n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF);

    fprintf(o, "  /* [10-13]  total_size: 4-byte big-endian                        */\n");
    fprintf(o, "  0x%02X,0x%02X,0x%02X,0x%02X,\n",
            (total >> 24) & 0xFF, (total >> 16) & 0xFF, (total >> 8) & 0xFF, total & 0xFF);

    fprintf(o, "  /* [14-15]  notdef_adv: 2-byte big-endian                        */\n");
    fprintf(o, "  0x%02X,0x%02X,\n", (notdef_adv >> 8) & 0xFF, notdef_adv & 0xFF);

    fprintf(o, "  /* [16-17]  ascender: 2-byte big-endian, signed                  */\n");
    fprintf(o, "  0x%02X,0x%02X,\n",
            ((uint16_t)ascender >> 8) & 0xFF, (uint16_t)ascender & 0xFF);

    fprintf(o, "  /* [18-19]  descender: 2-byte big-endian, signed                 */\n");
    fprintf(o, "  0x%02X,0x%02X,\n",
            ((uint16_t)descender >> 8) & 0xFF, (uint16_t)descender & 0xFF);

    /* ---------------- codepoints ---------------- */
    fprintf(o, "\n");
    fprintf(o, "  /* ============================================================ */\n");
    fprintf(o, "  /* Codepoints: number_of_chars * 2 bytes, big-endian, ascending */\n");
    fprintf(o, "  /* ============================================================ */\n");
    for (uint32_t i = 0; i < n; i++) {
        fprintf(o, "  0x%02X,0x%02X,   /* U+%04X */\n",
                (cps->cp[i] >> 8) & 0xFF, cps->cp[i] & 0xFF, cps->cp[i]);
    }

    /* ---------------- metrics ---------------- */
    fprintf(o, "\n");
    fprintf(o, "  /* ============================================================ */\n");
    fprintf(o, "  /* Metrics: number_of_chars * 10 bytes                          */\n");
    fprintf(o, "  /*   [0-1] w     2-byte big-endian, ink width                   */\n");
    fprintf(o, "  /*   [2-3] h     2-byte big-endian, ink height                  */\n");
    fprintf(o, "  /*   [4-5] x_off 2-byte big-endian, signed, left bearing        */\n");
    fprintf(o, "  /*   [6-7] y_off 2-byte big-endian, signed, top bearing         */\n");
    fprintf(o, "  /*   [8-9] adv   2-byte big-endian, advance                     */\n");
    fprintf(o, "  /* ============================================================ */\n");
    for (uint32_t i = 0; i < n; i++) {
        Glyph *g = &gs[i];
        fprintf(o, "  0x%02X,0x%02X, 0x%02X,0x%02X, 0x%02X,0x%02X, 0x%02X,0x%02X, 0x%02X,0x%02X,   /* U+%04X */\n",
                (g->w >> 8) & 0xFF, g->w & 0xFF,
                (g->h >> 8) & 0xFF, g->h & 0xFF,
                ((uint16_t)g->x_off >> 8) & 0xFF, (uint16_t)g->x_off & 0xFF,
                ((uint16_t)g->y_off >> 8) & 0xFF, (uint16_t)g->y_off & 0xFF,
                (g->adv >> 8) & 0xFF, g->adv & 0xFF,
                cps->cp[i]);
    }

    /* ---------------- data_offsets ---------------- */
    fprintf(o, "\n");
    fprintf(o, "  /* ============================================================ */\n");
    fprintf(o, "  /* Data offsets: number_of_chars * 4 bytes, big-endian          */\n");
    fprintf(o, "  /* ============================================================ */\n");
    uint32_t off = 0;
    for (uint32_t i = 0; i < n; i++) {
        fprintf(o, "  0x%02X,0x%02X,0x%02X,0x%02X,   /* U+%04X */\n",
                (off >> 24) & 0xFF, (off >> 16) & 0xFF, (off >> 8) & 0xFF, off & 0xFF,
                cps->cp[i]);
        off += (uint32_t)gs[i].bitmap.n;
    }

    /* ---------------- data ---------------- */
    fprintf(o, "\n");
    fprintf(o, "  /* ============================================================ */\n");
    fprintf(o, "  /* Data: glyph bitmaps                                          */\n");
    fprintf(o, "  /*   1BPP: ceil(w/8) * h bytes per glyph, LSB-left, row-major   */\n");
    fprintf(o, "  /*   8BPP: w * h bytes per glyph, row-major                     */\n");
    fprintf(o, "  /* ============================================================ */\n");
    for (uint32_t i = 0; i < n; i++) {
        Glyph *g = &gs[i];

        if (g->bitmap.n == 0) {
            fprintf(o, "  /* U+%04X: w=0 h=0, no data */\n", cps->cp[i]);
            continue;
        }

        fprintf(o, "  ");
        for (size_t k = 0; k < g->bitmap.n; k++) {
            fprintf(o, "0x%02X,", g->bitmap.p[k]);
        }
        fprintf(o, "   /* U+%04X */\n", cps->cp[i]);
    }

    fprintf(o, "};\n\n#endif\n");
    fclose(o);
    fprintf(stderr, "wrote %s (%u glyphs, max %ux%u, total %u bytes)\n",
            oname, n, maxw, maxh, total);

    free(base);
    free(fname);
    free(oname);
}

/* ------------------------------------------------------------------ */
/* BMP preview                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    int      w, h;
    uint8_t *idx;
    int      row_bytes;
} BmpCanvas;

static uint16_t ugui_mix_rgb565(uint16_t fc, uint16_t bc, uint8_t b)
{
    uint32_t fb = b;
    uint32_t bb = 256 - b;
    uint16_t r = (uint16_t)((((fc >> 11) & 0x1F) * fb + ((bc >> 11) & 0x1F) * bb) >> 8);
    uint16_t g = (uint16_t)((((fc >>  5) & 0x3F) * fb + ((bc >>  5) & 0x3F) * bb) >> 8);
    uint16_t bl= (uint16_t)((( fc        & 0x1F) * fb + ( bc        & 0x1F) * bb) >> 8);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void rgb565_to_rgb888(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >>  5) & 0x3F;
    uint8_t b5 =  c        & 0x1F;
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

static int bmp_write_8(const char *path, const BmpCanvas *c,
                       uint16_t fc, uint16_t bc)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "open %s: %s\n", path, strerror(errno)); return -1; }

    int row_bytes = ((c->w + 3) / 4) * 4;
    int data_size = row_bytes * c->h;
    int file_size = 14 + 40 + 256 * 4 + data_size;

    uint8_t hdr[54] = {0};

    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (uint8_t)(file_size);
    hdr[3] = (uint8_t)(file_size >> 8);
    hdr[4] = (uint8_t)(file_size >> 16);
    hdr[5] = (uint8_t)(file_size >> 24);
    int data_offset = 54 + 256 * 4;
    hdr[10] = (uint8_t)(data_offset);
    hdr[11] = (uint8_t)(data_offset >> 8);
    hdr[12] = (uint8_t)(data_offset >> 16);
    hdr[13] = (uint8_t)(data_offset >> 24);

    hdr[14] = 40;
    hdr[18] = (uint8_t)(c->w);
    hdr[19] = (uint8_t)(c->w >> 8);
    hdr[20] = (uint8_t)(c->w >> 16);
    hdr[21] = (uint8_t)(c->w >> 24);
    hdr[22] = (uint8_t)(c->h);
    hdr[23] = (uint8_t)(c->h >> 8);
    hdr[24] = (uint8_t)(c->h >> 16);
    hdr[25] = (uint8_t)(c->h >> 24);
    hdr[26] = 1;
    hdr[28] = 8;

    fwrite(hdr, 1, 54, f);

    for (int i = 0; i < 256; i++) {
        uint16_t mixed = ugui_mix_rgb565(fc, bc, (uint8_t)i);
        uint8_t r, g, b;
        rgb565_to_rgb888(mixed, &r, &g, &b);
        uint8_t e[4] = { b, g, r, 0 };
        fwrite(e, 1, 4, f);
    }

    uint8_t *row = calloc(1, row_bytes);
    if (!row) { fclose(f); return -1; }
    for (int y = c->h - 1; y >= 0; y--) {
        memcpy(row, c->idx + (size_t)y * c->row_bytes, c->w);
        fwrite(row, 1, row_bytes, f);
    }
    free(row);
    fclose(f);
    return 0;
}

static void render_preview(CpVec *cps, Glyph *gs, uint16_t maxh,
                           const char *text, const char *out_path)
{
    int line_h = maxh + 1;

    /* ---------------------------------------------------------------- */
    /* Pass 1: measure the tight bounding box of the rendered text       */
    /* ---------------------------------------------------------------- */
    int xp = 0, yp = 0;
    int min_x = INT32_MAX, min_y = INT32_MAX;
    int max_x = INT32_MIN, max_y = INT32_MIN;
    int any = 0;

    const char *s = text;
    while (*s) {
        uint32_t cp = 0;
        unsigned char c0 = (unsigned char)*s;
        int n;
        if (c0 < 0x80) { cp = c0; n = 1; }
        else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; n = 2; }
        else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; n = 3; }
        else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; n = 4; }
        else { s++; continue; }

        for (int k = 1; k < n; k++) {
            if ((s[k] & 0xC0) != 0x80) { cp = 0; break; }
            cp = (cp << 6) | (s[k] & 0x3F);
        }
        s += n;
        if (cp == 0) continue;

        if (cp == '\n') { xp = 0; yp += line_h; continue; }

        uint32_t lo = 0, hi = cps->n, gi = 0;
        int found = 0;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (cps->cp[mid] == cp) { gi = mid; found = 1; break; }
            if (cps->cp[mid] < cp) lo = mid + 1; else hi = mid;
        }
        if (!found) continue;

        Glyph *g = &gs[gi];
        if (g->w == 0 || g->h == 0) { xp += g->adv + 1; continue; }

        int draw_x = xp + g->x_off;
        int draw_y = yp - g->y_off;

        if (draw_x        < min_x) min_x = draw_x;
        if (draw_y        < min_y) min_y = draw_y;
        if (draw_x + g->w > max_x) max_x = draw_x + g->w;
        if (draw_y + g->h > max_y) max_y = draw_y + g->h;
        /* 1BPP 影子偏移 (+1, +1)，画布扩 1 像素 */
        if (bpp == 1 && opt_shadow) {
            if (draw_x + g->w + 1 > max_x) max_x = draw_x + g->w + 1;
            if (draw_y + g->h + 1 > max_y) max_y = draw_y + g->h + 1;
        }
        any = 1;

        xp += g->adv + 1;
    }

    if (!any) {
        min_x = min_y = 0;
        max_x = max_y = 1;
    }

    int pad_top = 2;
    int pad_bottom = 2;
    int w = max_x - min_x;
    int h = (max_y - min_y) + pad_top + pad_bottom;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > PREVIEW_W) w = PREVIEW_W;
    if (h > PREVIEW_H) h = PREVIEW_H;

    /* ---------------------------------------------------------------- */
    /* Pass 2: render into an w*h indexed canvas                         */
    /* ---------------------------------------------------------------- */
    BmpCanvas cv;
    cv.w = w;
    cv.h = h;
    cv.row_bytes = ((w + 3) / 4) * 4;
    cv.idx = calloc((size_t)cv.row_bytes * h, 1);
    if (!cv.idx) { fprintf(stderr, "oom\n"); exit(1); }

    uint16_t fc = C_BLACK;
    uint16_t bc = C_WHITE;

    memset(cv.idx, 0, (size_t)cv.row_bytes * h);

    xp = 0; yp = 0;
    s = text;
    while (*s) {
        uint32_t cp = 0;
        unsigned char c0 = (unsigned char)*s;
        int n;
        if (c0 < 0x80) { cp = c0; n = 1; }
        else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; n = 2; }
        else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; n = 3; }
        else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; n = 4; }
        else { s++; continue; }

        for (int k = 1; k < n; k++) {
            if ((s[k] & 0xC0) != 0x80) { cp = 0; break; }
            cp = (cp << 6) | (s[k] & 0x3F);
        }
        s += n;
        if (cp == 0) continue;

        if (cp == '\n') { xp = 0; yp += line_h; continue; }

        uint32_t lo = 0, hi = cps->n, gi = 0;
        int found = 0;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (cps->cp[mid] == cp) { gi = mid; found = 1; break; }
            if (cps->cp[mid] < cp) lo = mid + 1; else hi = mid;
        }
        if (!found) continue;

        Glyph *g = &gs[gi];
        if (g->w == 0 || g->h == 0) { xp += g->adv + 1; continue; }

        int draw_x = xp + g->x_off;
        int draw_y = yp - g->y_off;

        int bx = draw_x - min_x;
        int by = draw_y - min_y + pad_top;

        if (bpp == 1) {
            /* --- 1BPP: shadow pass (offset +1, +1, value 128) --- */
            uint16_t bpr = (g->w + 7) / 8;
            if (opt_shadow) {
                /* --- 1BPP: shadow pass (offset +1, +1, value 128) --- */
                for (int j = 0; j < g->h; j++) {
                    for (int i = 0; i < g->w; i++) {
                        uint8_t byte = g->bitmap.p[j * bpr + (i >> 3)];
                        if (!((byte >> (i & 7)) & 1)) continue;
                        int sx = bx + i + 1;
                        int sy = by + j + 1;
                        if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                            cv.idx[(size_t)sy * cv.row_bytes + sx] = 128;
                        }
                    }
                }
            }
            /* --- 1BPP: main pass (value 255) --- */
            for (int j = 0; j < g->h; j++) {
                for (int i = 0; i < g->w; i++) {
                    uint8_t byte = g->bitmap.p[j * bpr + (i >> 3)];
                    if (!((byte >> (i & 7)) & 1)) continue;
                    int sx = bx + i;
                    int sy = by + j;
                    if (sx >= 0 && sx < w && sy >= 0 && sy < h) {
                        cv.idx[(size_t)sy * cv.row_bytes + sx] = 255;
                    }
                }
            }
        } else {
            /* --- 8BPP: 原来的画法 --- */
            for (int j = 0; j < g->h; j++) {
                for (int i = 0; i < g->w; i++) {
                    int sx = bx + i;
                    int sy = by + j;
                    if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
                    cv.idx[(size_t)sy * cv.row_bytes + sx] = g->bitmap.p[j * g->w + i];
                }
            }
        }

        xp += g->adv + 1;
    }

    if (bmp_write_8(out_path, &cv, fc, bc) == 0)
        fprintf(stderr, "wrote %s (%dx%d, 8bpp indexed)\n", out_path, cv.w, cv.h);

    free(cv.idx);
}

/* ------------------------------------------------------------------ */
/* CLI                                                                */
/* ------------------------------------------------------------------ */

static void usage(void)
{
    fprintf(stderr,
        "\nttf2ugui {--dump | --preview} [options]\n"
        "  --font=FILE     TTF/OTF font file\n"
        "  --size=N        size (px, or pt if --dpi)\n"
        "  --dpi=N         optional\n"
        "  --bpp=N         1 (default) or 8\n"
        "  --chars=LIST    default 32-126, use --chars=...\n"
        "  --dump          write C font file\n"
        "  --preview       write preview.bmp (512x512, 8bpp indexed)\n"
        "  --text=TEXT     text to render into preview.bmp\n"
        "  --list-faces    list all faces in the font file and exit\n"
        "  --face-index=N  select face index N (default 0, for TTC)\n"
        "  --weight=N      variable font weight (100-900), only for VF\n"
        "  --shadow=N      1 = draw shadow (default), 0 = no shadow, 1BPP only\n");
}

#ifdef _WIN32
/*
 * Windows gives us argv in the system ANSI code page (GBK on Chinese
 * Windows). Convert it to UTF-8 so the rest of the code can treat text
 * as UTF-8 uniformly.
 */
static char *ansi_to_utf8(const char *ansi)
{
    int wlen = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    if (wlen <= 0) return NULL;

    wchar_t *wbuf = malloc((size_t)wlen * sizeof(wchar_t));
    if (!wbuf) return NULL;
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, wbuf, wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (ulen <= 0) { free(wbuf); return NULL; }

    char *ubuf = malloc((size_t)ulen);
    if (!ubuf) { free(wbuf); return NULL; }
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, ubuf, ulen, NULL, NULL);

    free(wbuf);
    return ubuf;
}
#endif

int main(int argc, char **argv)
{
    static struct option lo[] = {
        {"preview",    no_argument,       NULL, 'P'},
        {"text",       required_argument, NULL, 't'},
        {"dump",       no_argument,       NULL, 'D'},
        {"dpi",        required_argument, NULL, 'd'},
        {"chars",      required_argument, NULL, 'c'},
        {"size",       required_argument, NULL, 's'},
        {"font",       required_argument, NULL, 'f'},
        {"bpp",        required_argument, NULL, 'b'},
        {"list-faces", no_argument,       NULL, 'L'},
        {"face-index", required_argument, NULL, 'i'},
        {"weight",     required_argument, NULL, 'w'},
        {"shadow",     required_argument, NULL, 'S'},
        {"help",       no_argument,       NULL, 'h'},
        {NULL,0,NULL,0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "", lo, NULL)) != -1) {
        switch (ch) {
        case 'P': opt_preview = 1; break;
        case 't': opt_text    = optarg; break;
        case 'D': opt_dump    = 1; break;
        case 'd': dpi = atoi(optarg); break;
        case 'c': opt_chars = optarg; break;
        case 's': fontSize = (float)atof(optarg); break;
        case 'f': opt_font = optarg; break;
        case 'b': bpp = atoi(optarg);
                  if (bpp != 1 && bpp != 8) { fprintf(stderr, "--bpp 1 or 8\n"); return 1; }
                  break;
        case 'L': opt_list_faces = 1; break;
        case 'i': opt_face_index = atoi(optarg); break;
        case 'w': opt_weight = atoi(optarg); break;
        case 'S': opt_shadow = atoi(optarg) ? 1 : 0; break;
        case 'h': usage(); return 0;
        default:  usage(); return 1;
        }
    }

    if (opt_list_faces) {
        if (!opt_font) {
            fprintf(stderr, "--list-faces requires --font=FILE\n");
            return 1;
        }
        list_faces(opt_font);
        return 0;
    }

    char *opt_text_alloc = NULL;
    #ifdef _WIN32
        if (opt_text) {
            char *u = ansi_to_utf8(opt_text);
            if (u) { opt_text = u; opt_text_alloc = u; }
        }
    #endif

    if (!opt_font || fontSize <= 0 || (!opt_dump && !opt_preview)) {
        usage(); return 1;
    }
    if (opt_preview && !opt_text) {
        fprintf(stderr, "--preview requires --text=TEXT\n");
        return 1;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    CpVec cps = {0};

    parse_chars(&cps, opt_chars ? opt_chars : DEFAULT_CHARS);

    Glyph *gs = NULL;
    uint16_t maxw = 0, maxh = 0, notdef_adv = 0;
    int16_t ascender = 0, descender = 0;
    convert_font(opt_font, &cps, &gs, &maxw, &maxh, &notdef_adv,
                 &ascender, &descender);

    sort_cps_glyphs(&cps, gs);

    if (opt_preview)
        render_preview(&cps, gs, maxh, opt_text, "preview.bmp");

    if (opt_dump)
        dump_font(opt_font, &cps, gs, maxw, maxh, notdef_adv,
                  ascender, descender);

    for (uint32_t i = 0; i < cps.n; i++) free(gs[i].bitmap.p);
    free(gs);
    free(cps.cp);
    free(opt_text_alloc);
    return 0;
}