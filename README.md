This is a fork of ttf2ugui based on https://github.com/deividAlfa/ttf2ugui
with a math-correct font format, a BMP preview, and a bundled FreeType.

Changes from deividAlfa's version:

1. New math-correct font format:
   - Each glyph stores its own ink w/h, left bearing (x_off),
     top bearing (y_off) and advance (adv).
   - Bitmap data is stored tight, with no cell padding.
   - Glyph lookup uses a binary search over an ascending codepoint table.
     Codepoints are sorted and deduplicated at conversion time, so the
     order in `--chars` does not matter.
   - Old-format fonts (bit 7 of byte 0 set) are still readable.
   - Not compatible with deividAlfa's custom format; regenerate your fonts.
   - Glyphs whose width, height, bearings or advance exceed the fixed
     limits are rejected at conversion time with a "metric out of range"
     error instead of silently producing a broken font.

2. Fixed the `--show` preview for CJK: replaced the ANSI terminal preview
   with `--preview --text=TEXT`, which renders a 512x512 (auto-sized,
   clamped) 8bpp indexed `preview.bmp` next to the generated `.c`.
   The palette is generated with the exact same mixing formula uGUI uses,
   so the BMP shows what the MCU screen will show. `--preview` supports
   `\n` and UTF-8 text.

3. Added `FT_LOAD_NO_BITMAP` for 8bpp generation, so embedded monochrome
   bitmaps inside a font are ignored and FreeType renders the outline
   into a real 8bpp grayscale bitmap. Without this, fonts such as
   SIMSUN2.TTC return a 1bpp embedded bitmap even when 8bpp was requested.

4. Added `--dump` / `--preview` separation: they are independent and can
   be combined in one invocation.

5. Cleaned up font-name handling: names are sanitized to `[A-Za-z0-9_]`,
   allocated exactly (no truncation), and the generated `.c` and
   `preview.bmp` end up in the same directory.

6. Reworked UTF-8 decoding on the uGUI side: overlong encodings,
   malformed continuation bytes, 5/6-byte sequences and codepoints above
   0xFFFF are rejected instead of being silently accepted. This matters
   now that the tool emits real Unicode codepoints, including CJK.

7. Added `--list-faces` / `--face-index=N` for selecting a face inside a
   TTC, and `--weight=N` for setting the `wght` axis of a variable font.

8. Added `--shadow=0|1` (default 1) for 1BPP fonts, controlling the
   shadow pass drawn offset by (+1,+1) behind the glyph body in
   `preview.bmp`.

9. Reworked `--chars` parsing: ranges accept `-` or `_`, bad ranges and
   empty results are reported as errors. Duplicate codepoints are
   silently deduplicated. Only decimal is accepted; `0x` hex is not
   supported.

10. `ugui.c` / `ugui.h` in this repository were updated to match the new
    format.

11. Bundled FreeType 2.14.3:
    - The FreeType source archive lives in `deps/freetype-2.14.3.tar.gz`
      and is extracted into `build/` at configure time.
    - It is built as a static library and linked into `ttf2ugui`, so the
      resulting executable has no external FreeType dependency.
    - `deps/ftmodule.h` selects the modules compiled in: sfnt, tt, cff,
      bdf, pcf, psnames, pshinter, psaux, smooth, raster1, autofit.
      This covers TTF, OTF, TTC, BDF and PCF inputs.
    - `deps/ftoption.h` enables variable font support
      (`TT_CONFIG_OPTION_GX_VAR_SUPPORT`) for `--weight`, plus the
      TrueType bytecode interpreter.
    - zlib, bzip2, png, harfbuzz and brotli are disabled to keep the
      binary small. Compressed fonts (`.ttf.gz`) are not supported.

12. Missing-glyph handling:
    - Codepoints not present in the font are detected via
      `FT_Get_Char_Index == 0`, reported as `[missing] U+XXXX`, and
      skipped instead of being emitted as garbage.
    - The font's `.notdef` advance is stored in the generated font header
      as `notdef_adv`, so the renderer can advance the pen by a sensible
      amount when it encounters a codepoint that was not emitted.

CLI:

    ttf2ugui --font=FILE --size=N [--dpi=N] [--bpp=1|8] [--chars=LIST]
             [--dump] [--preview --text=TEXT]
             [--list-faces] [--face-index=N] [--weight=N] [--shadow=0|1]


Examples
========

Basic ASCII font, 1BPP, dump to C
---------------------------------

    ttf2ugui --font=arial.ttf --size=12 --bpp=1 --dump

Generates `arial_<maxw>X<maxh>.c` in the current directory.


Basic ASCII font, 8BPP antialiased, dump to C
---------------------------------------------

    ttf2ugui --font=arial.ttf --size=16 --bpp=8 --dump

8BPP forces the outline to be rasterized by FreeType; embedded bitmap
strikes in the font are ignored (`FT_LOAD_NO_BITMAP`).


Point size instead of pixel size
--------------------------------

    ttf2ugui --font=arial.ttf --size=10 --dpi=96 --bpp=1 --dump

With `--dpi`, `--size` is interpreted as points and converted to pixels
using the given DPI. Without `--dpi`, `--size` is in pixels.


Custom character set
--------------------

    ttf2ugui --font=arial.ttf --size=12 --bpp=1 --dump \
             --chars=32-126,160-255

Ranges accept `-` or `_` as the separator. Individual codepoints can be
mixed with ranges. Only decimal is accepted; `0x` hex is not supported.
Duplicate codepoints are silently deduplicated. The order in which
ranges are given does not matter; codepoints are sorted ascending before
being written to the generated `.c`.


CJK font with explicit codepoint ranges
---------------------------------------

    ttf2ugui --font=SIMSUN2.TTC --face-index=0 --size=12 --bpp=1 \
             --dump --chars=32-126,12289-12329,19968-40943,65281-65374

`--face-index` selects a face inside a TTC. `--list-faces` lists the
available faces first:

    ttf2ugui --font=SIMSUN2.TTC --list-faces


Variable font weight
--------------------

    ttf2ugui --font=Inter-Variable.ttf --size=16 --bpp=8 \
             --weight=700 --dump

`--weight` sets the `wght` axis if the font has one. If the font is not
variable or has no `wght` axis, the option is ignored with a note.


Preview BMP
-----------

    ttf2ugui --font=arial.ttf --size=16 --bpp=8 \
             --preview --text="Hello, world!"

Writes `preview.bmp` (8bpp indexed) next to the generated `.c`. The
palette uses the same mixing formula as uGUI, so the BMP matches what
the MCU will show. `\n` and UTF-8 are supported in `--text`.


Preview with shadow disabled (1BPP only)
----------------------------------------

    ttf2ugui --font=arial.ttf --size=12 --bpp=1 --shadow=0 \
             --preview --text="no shadow"

For 1BPP fonts, `--shadow` controls a (+1,+1) offset shadow drawn behind
the glyph body. The 8BPP path does not use `--shadow`.


Dump and preview in one invocation
----------------------------------

    ttf2ugui --font=arial.ttf --size=16 --bpp=8 \
             --dump --preview --text="Both outputs"

`--dump` and `--preview` are independent and can be combined.


BDF / PCF bitmap fonts
----------------------

    ttf2ugui --font=6x10.bdf --size=10 --bpp=1 --dump

BDF and PCF fonts are bitmap-only, so `--size` must match a size that the
font actually provides. If the requested size does not exist, FreeType
returns `FT_Err_Invalid_Pixel_Size` and the tool exits with `FT size 23`.
Use the size encoded in the font name (e.g. `6x10` -> `--size=10`).


Building
========

    ./build.sh          # configure if needed, build
    ./build.sh all      # clean + configure + build + strip
    ./build.sh clean    # remove build/
    ./build.sh strip    # strip only

The FreeType source archive is extracted into `build/freetype-src` and
built as a static library. The resulting `build/ttf2ugui` (or
`ttf2ugui.exe` on MinGW) has no external FreeType dependency.

Dependencies for building:
  - CMake >= 3.20
  - A C99 compiler (GCC, Clang, MinGW)
  - No system FreeType is required or used.


deividalfa's ttf2ugui
========
This is my version of [ttf2gui](https://github.com/AriZuu/ttf2ugui), modified for my [uGUI](https://github.com/deividalfa/UGUI) fork.<br>
Adds UTF8 compatibility and supports the custom font structure used in my uGUI version.<br>
Not compatible with the original!<br>

ttf2ugui
========

[uGUI][1] is a free open source graphics library for embedded systems.<br>
To display text, it uses bitmap/raster fonts, that are included in application as C-language structs & arrays.<br>
uGUI includes some fonts in itself, but I wanted to use some TrueType fonts.<br>

I didn't find a tool that would convert a .ttf file into C structures used by uGUI easily, so I wrote one using [Freetype library][2].<br>

This is a simple utility to convert TrueType fonts into uGUI compatible structures.<br>
It reads font file, renders each character into bitmap and outputs it as uGUI compatible C structure.<br>

Optionally it can display ascii art sample of font by using UGUI to render pixels as '*' with ansi escape sequences.<br>
Fonts generated with 8BPP show in blue pixels with less then 100% fill.<br>
Please remember to respect font copyrights when converting.<br>
Examples:<br>

##### Convert font in Luna.ttf to 14 point size bitmap font for 140 DPI display:<br>
```./ttf2ugui --font=Luna.ttf --dpi=140 --size=14 --dump```

Results are in Luna.c and Luna.h, just compile the .c and include .h in your uGUI application.<br><br>

##### Show ascii art of same font:<br>
```./ttf2ugui --font=Luna.ttf --dpi=140 --size=14 --show="aString"```

##### If you want to generate 8BPP fonts ( so you get anti alliased fonts ) use:<br>
```./ttf2ugui --font=Luna.ttf --dpi=140 --size=14 --show="aString" --bpp=8```

##### If the font file is not in the same directory, use full path to the font otf/ttf file, e.g.<br>
###### MacOS

```./ttf2ugui --font=/System/Library/Fonts/Supplemental/Arial.ttf --dpi=140 --size=14```

###### Windows

```./ttf2ugui --font=C:\Windows\Fonts\Arial.ttf --dpi=140 --size=14```

<br>

#### You can specify chars to be generated:
##### Space and numbers:<br>
```./ttf2ugui --dump --font=arial.ttf --size=14 --chars=32,48-57```

##### Space, uppercase and numbers<br>
```./ttf2ugui --dump --font=arial.ttf --size=14 --chars=32,48-57,65-90```

##### Standard ASCII and cyrillic<br>
```./ttf2ugui --dump --font=arial.ttf --size=14 --chars=32-126,1042-1103```

##### Standard ASCII, © symbol and cyrillic<br>
```./ttf2ugui --dump --font=arial.ttf --size=14 --chars=32-126,169,1042-1103```

"--chars" use Unicode or ASCII codes.<br>
<br>

#### Compiling
To compile, freetype library is needed.<br>
Easiest way to get is to install suitable package for your operating system.<br>

- For FreeBSD, install "print/freetype2".<br>
- For Debian, install libfreetype6-dev with apt-get.<br>
- For MacOS, install freetype with brew.<br>
- For Windows, install cygwin64 and add these packages: make gcc-core libfreetype-devel<br>
There's a precompiled build for Windows (ttf2ugui-win.zip)<br>

Then, just type "make".<br>

[1]: http://www.embeddedlightning.com/ugui/
[2]: http://freetype.org/
