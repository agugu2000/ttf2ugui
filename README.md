This is another ttf2gui based on https://github.com/deividAlfa/ttf2ugui with modifications:

1. now the single character range for font conversion will be repeated for setting a clear range.like:unicode 169 has an offset:0x00,0xA9,0x00,0xA9 Flag:0x01,0x00
2. fixed the show function for CJK.
3. Add new Flags for ranges of font data.
4. Anything more??No...Playing this is just for fun~~
5. find character width calculation and character position offset problem in 8bpp mode.Fixed it.

##### Standard ASCII , ©,cyrillic, CJK punctuations , Common Chinese characters , full-width ASCII characters:<br>
```./ttf2ugui --font=SourceHanSansCN_Medium.otf --dpi=75 --size=12 --dump --char=32-126,169,1042-1103,8208-8303,12289-12329,19968-40943,65281-65374 --show="你好，世界！"```   
<img src="./ttf2ugui.png" width="600">


##### Use it in Windows
You can use the tool without any additional operations,but if you wanna see a right unicode previews with --show="xxxx".Turn on the utf8 support in Windows:
1. Open the "Control Panel" (you can search for "Control Panel" in the Start menu).
2. Click on "Clock and Region" (or "Region").
3. Click on the "Region" option.
4. In the window that pops up, select the "Administrative" tab.
5. Click the "Change system locale" button.
6. In the new dialog box, check the option "Beta: Use Unicode UTF-8 for worldwide language support (U)".
7. Click "OK", and you may need to restart your computer.


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
