# Third-party notices

Feather PDF uses a single third-party component at run time: **PDFium**,
distributed as `pdfium.dll` (prebuilt by the
[pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) project,
release `chromium/8066`, no V8/XFA).

| Component | Used for | License |
|---|---|---|
| PDFium | PDF parsing, rendering, text extraction | BSD-3-Clause (some files Apache-2.0) |
| pdfium-binaries build scripts | Prebuilt DLL | MIT |
| FreeType | Font rendering (inside pdfium.dll) | FreeType License (FTL) |
| libjpeg-turbo | JPEG images | IJG + BSD-3-Clause + zlib |
| libpng | PNG images | libpng license |
| zlib | Flate streams | zlib license |
| OpenJPEG | JPEG 2000 images | BSD-2-Clause |
| Little CMS (lcms2) | Colour management | MIT |
| Anti-Grain Geometry 2.3 | Rasterisation | AGG license (BSD-style) |
| ICU | Unicode support | Unicode License v3 |
| Abseil | Utility library | Apache-2.0 |
| fast_float, simdutf | Number / UTF parsing | MIT / Apache-2.0 |
| LLVM libc | Support routines | Apache-2.0 with LLVM exception |

The full license texts are installed in the `licenses/` folder next to
`FeatherPDF.exe` (copied from the pdfium-binaries package by the build).

All of these are permissive licenses: Feather PDF can be distributed,
including commercially and in closed-source form, as long as these notices
are shipped with it.

The application itself uses only Windows system libraries (user32, gdi32,
comctl32, comdlg32, shell32, ole32, gdiplus, winhttp and others) and the C++
standard library, which is statically linked.

## Text recognition (OCR)

Feather PDF recognises text in scanned pages with **Tesseract** and **Leptonica**, linked into `FeatherPDF.exe`,
and the English language data (`tessdata/eng.traineddata`, "tessdata_fast").

| Component | Used for | License |
|---|---|---|
| Tesseract OCR | Text recognition | Apache-2.0 |
| Leptonica | Image processing for Tesseract | BSD-2-Clause |
| tessdata_fast language data | Recognition models | Apache-2.0 |

Their license texts are in the `licenses/` folder (`tesseract-APACHE-2.0.txt`, `leptonica.txt`). More languages can be added by
placing further `<language>.traineddata` files in the `tessdata` folder next to `FeatherPDF.exe`.

## Fonts for equations in documents made with TeX

When an equation is edited or added in a document made with LaTeX, Feather PDF sets it in fonts made from **Latin
Modern Math** (by the GUST e-foundry), so it looks like the equations TeX set around it. The fonts are built into the
app (`shared/fonts`, made by `scripts/make-math-fonts.py`) and embedded in the PDF where they are used. As the licence
asks of changed versions, they carry their own names: FeatherLMMath-Regular, FeatherLMMath-Italic and FeatherLMMath-Bold.

| Component | Used for | License |
|---|---|---|
| Latin Modern Math (subset, TrueType outlines, renamed) | Equations in TeX documents | GUST Font License (LPPL 1.3c) |

```
% This is a preliminary version (2006-09-30), barring acceptance from
% the LaTeX Project Team and other feedback, of the GUST Font License.
% (GUST is the Polish TeX Users Group, http://www.gust.org.pl)
%
% For the most recent version of this license see
% http://www.gust.org.pl/fonts/licenses/GUST-FONT-LICENSE.txt
% or
% http://tug.org/fonts/licenses/GUST-FONT-LICENSE.txt
%
% This work may be distributed and/or modified under the conditions
% of the LaTeX Project Public License, either version 1.3c of this
% license or (at your option) any later version.
% 
% Please also observe the following clause:
% 1) it is requested, but not legally required, that derived works be
%    distributed only after changing the names of the fonts comprising this
%    work and given in an accompanying "manifest", and that the
%    files comprising the Work, as listed in the manifest, also be given
%    new names. Any exceptions to this request are also given in the
%    manifest.
%    
%    We recommend the manifest be given in a separate file named
%    MANIFEST-<fontid>.txt, where <fontid> is some unique identification
%    of the font family. If a separate "readme" file accompanies the Work, 
%    we recommend a name of the form README-<fontid>.txt.
%
% The latest version of the LaTeX Project Public License is in
% http://www.latex-project.org/lppl.txt and version 1.3c or later
% is part of all distributions of LaTeX version 2006/05/20 or later.
```
