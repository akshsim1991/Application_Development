# Feather PDF 2.13 against the 500-feature checklist

Status of every item of *PDF Reader & Editor - 500 Features* (23 modules). **Done** = works in 2.13 · **Partly** = part of it, see the note · **Missing** = not built. Tested under Wine; printing, Windows voices and live time-stamp servers were not testable there.

**Overall: 163 done, 82 partly, 255 missing (of 500).**

| Module | Done | Partly | Missing |
|---|---|---|---|
| 1. PDF Reading and Navigation | 21 | 4 | 5 |
| 2. Text Editing | 12 | 8 | 10 |
| 3. Image Editing | 7 | 3 | 16 |
| 4. Vector Graphics and Object Editing | 1 | 6 | 13 |
| 5. Page Management | 13 | 4 | 9 |
| 6. Annotations and Markup | 14 | 1 | 11 |
| 7. OCR and Scanned PDF Editing | 6 | 3 | 18 |
| 8. PDF Conversion | 6 | 2 | 17 |
| 9. PDF Forms | 8 | 2 | 10 |
| 10. Digital Signatures | 9 | 4 | 2 |
| 11. Security and Privacy | 12 | 2 | 6 |
| 12. PDF Compression and Optimization | 7 | 1 | 7 |
| 13. PDF Repair and Diagnostics | 0 | 4 | 11 |
| 14. PDF Comparison | 12 | 2 | 1 |
| 15. AI-Powered Features | 1 | 4 | 20 |
| 16. Batch Processing and Automation | 6 | 2 | 17 |
| 17. Engineering and Scientific Features | 5 | 7 | 13 |
| 18. Accessibility | 0 | 2 | 13 |
| 19. Printing and Publishing | 7 | 1 | 12 |
| 20. Performance and Reliability | 6 | 7 | 7 |
| 21. User Interface and Customization | 7 | 5 | 8 |
| 22. Collaboration and Integrations | 1 | 3 | 16 |
| 23. Advanced Competitive Features | 2 | 5 | 13 |
| **Total** | **163** | **82** | **255** |

## 1. PDF Reading and Navigation

| # | Feature | Status | Note |
|---|---|---|---|
| 1 | Open PDF files | Done |  |
| 2 | Open multiple PDFs in tabs | Done |  |
| 3 | Single-page viewing | Done |  |
| 4 | Continuous scrolling | Done |  |
| 5 | Two-page viewing | Done |  |
| 6 | Book reading mode | Partly | Two pages with the cover page shown separately; no page-turn effect |
| 7 | Full-screen reading | Done |  |
| 8 | Presentation mode | Done |  |
| 9 | Dark mode | Done | Window theme and dark page colours |
| 10 | Sepia reading mode | Partly | "Dimmed" page colours; no sepia tint yet |
| 11 | Zoom in and out | Done |  |
| 12 | Fit page and fit width | Done |  |
| 13 | Custom zoom percentage | Partly | Presets 50-400 % and Ctrl+wheel; no box to type an exact percentage |
| 14 | Page rotation | Done |  |
| 15 | Page thumbnails | Done |  |
| 16 | Bookmark navigation | Done |  |
| 17 | Table of contents navigation | Done | Clickable links inside the document |
| 18 | Search text within PDFs | Done |  |
| 19 | Search across multiple PDFs | Done | Search all PDFs in a folder, with a saved index |
| 20 | Advanced search using regular expressions | Missing |  |
| 21 | Text-to-speech | Done | Windows voices, speed setting |
| 22 | Auto-scroll | Missing |  |
| 23 | Split-screen viewing | Partly | Two documents side by side; not two views of one document |
| 24 | Side-by-side PDF viewing | Done |  |
| 25 | Synchronized scrolling | Done |  |
| 26 | Remember last reading position | Done |  |
| 27 | Keyboard shortcuts | Done |  |
| 28 | Touchscreen and gesture support | Missing | Touch works as mouse and wheel only; no pinch zoom |
| 29 | View PDF attachments | Missing |  |
| 30 | View PDF layers | Missing |  |

## 2. Text Editing

| # | Feature | Status | Note |
|---|---|---|---|
| 31 | Edit existing PDF text directly | Done | One line, or a whole paragraph with reflow |
| 32 | Add new text | Done |  |
| 33 | Delete existing text | Done |  |
| 34 | Move individual text | Done | Move tool |
| 35 | Move entire text blocks | Done | Move tool picks up a paragraph |
| 36 | Resize text boxes | Partly | Corner handles scale the text; no reflow into a new width |
| 37 | Rotate text | Missing |  |
| 38 | Change font family | Partly | Sans, serif and mono standard fonts only |
| 39 | Change font size | Done |  |
| 40 | Change font color | Done |  |
| 41 | Apply bold formatting | Done |  |
| 42 | Apply italic formatting | Done |  |
| 43 | Underline text | Partly | As an annotation, not as part of the text |
| 44 | Strikethrough text | Partly | As an annotation, not as part of the text |
| 45 | Superscript and subscript | Missing |  |
| 46 | Change character spacing | Missing |  |
| 47 | Change line spacing | Missing | The original spacing is kept when a paragraph is edited |
| 48 | Change paragraph spacing | Missing |  |
| 49 | Align text left, right, or center | Done |  |
| 50 | Justify paragraphs | Done |  |
| 51 | Find and replace text | Partly | A match must lie inside one run of text |
| 52 | Replace text across multiple pages | Partly | Whole document at once, same limit as above |
| 53 | Spell-checking | Missing |  |
| 54 | Grammar checking | Missing |  |
| 55 | Font embedding | Missing | New text uses the standard (not embedded) fonts |
| 56 | Font substitution | Done | A similar standard font is used when the original lacks a character |
| 57 | Unicode support | Partly | Reading and search: all scripts. New or edited text: Western characters only |
| 58 | Multilingual text editing | Missing | Western languages only |
| 59 | Mathematical symbol insertion | Missing |  |
| 60 | Preserve original formatting during editing | Partly | Same font when possible; otherwise the closest standard font |

## 3. Image Editing

| # | Feature | Status | Note |
|---|---|---|---|
| 61 | Insert images from computer | Done |  |
| 62 | Paste images from clipboard | Missing |  |
| 63 | Select existing images | Done | Move tool |
| 64 | Move images | Done |  |
| 65 | Resize images | Done |  |
| 66 | Rotate images | Missing |  |
| 67 | Flip images | Missing |  |
| 68 | Crop images | Missing |  |
| 69 | Delete images | Done | Move tool, then Del |
| 70 | Replace existing images | Missing |  |
| 71 | Extract images | Missing | Pages can be exported as pictures, not the embedded images themselves |
| 72 | Copy images between PDFs | Missing |  |
| 73 | Adjust image brightness | Missing |  |
| 74 | Adjust image contrast | Missing |  |
| 75 | Adjust saturation | Partly | Greyscale only |
| 76 | Sharpen images | Missing |  |
| 77 | Reduce image noise | Missing |  |
| 78 | Remove image backgrounds | Missing |  |
| 79 | Change image transparency | Missing |  |
| 80 | Compress images | Done |  |
| 81 | Change image resolution | Done | Compress by dpi |
| 82 | Convert image formats | Partly | Recompressed as JPEG |
| 83 | Align multiple images | Partly | Snapping guides while moving |
| 84 | Group and ungroup images | Missing |  |
| 85 | Bring images forward or backward | Missing |  |
| 86 | Edit image masks and clipping paths | Missing |  |

## 4. Vector Graphics and Object Editing

| # | Feature | Status | Note |
|---|---|---|---|
| 87 | Select vector objects | Partly | Tables made by Feather PDF only |
| 88 | Move vector objects | Partly | Same |
| 89 | Resize vector objects | Partly | Same |
| 90 | Rotate vector objects | Missing |  |
| 91 | Delete vector objects | Partly | Tables and shape annotations |
| 92 | Edit lines and curves | Missing |  |
| 93 | Edit Bezier paths | Missing |  |
| 94 | Modify shapes | Missing |  |
| 95 | Change fill colors | Missing |  |
| 96 | Change stroke colors | Missing |  |
| 97 | Change line thickness | Missing | Only for new drawings |
| 98 | Edit transparency | Missing |  |
| 99 | Group and ungroup objects | Missing |  |
| 100 | Align and distribute objects | Partly | Snapping guides; no distribute command |
| 101 | Snap objects to grids | Partly | Snaps to page edges, centre and other objects; no grid |
| 102 | Use rulers and guidelines | Done | Rulers on the page, snapping guides |
| 103 | Inspect individual PDF objects | Missing |  |
| 104 | Edit object stacking order | Missing |  |
| 105 | Copy objects between pages | Missing |  |
| 106 | Lock and unlock objects | Missing |  |

## 5. Page Management

| # | Feature | Status | Note |
|---|---|---|---|
| 107 | Insert pages | Done |  |
| 108 | Delete pages | Done |  |
| 109 | Duplicate pages | Done | Copy and paste in the thumbnails |
| 110 | Extract pages | Done |  |
| 111 | Replace pages | Partly | Delete, then insert from a file |
| 112 | Reorder pages | Done | Drag thumbnails |
| 113 | Merge PDF documents | Done |  |
| 114 | Split PDF documents | Done |  |
| 115 | Rotate selected pages | Done |  |
| 116 | Crop selected pages | Done | Also by drawing a box |
| 117 | Resize pages | Missing |  |
| 118 | Change page orientation | Done | Rotate |
| 119 | Insert blank pages | Done |  |
| 120 | Add page numbers | Done |  |
| 121 | Add headers and footers | Partly | Page numbers top or bottom; no free header/footer text |
| 122 | Add watermarks | Done |  |
| 123 | Remove watermarks where editable | Missing |  |
| 124 | Add background colors | Missing |  |
| 125 | Remove blank pages | Missing |  |
| 126 | Detect duplicate pages | Missing |  |
| 127 | Reverse page order | Missing |  |
| 128 | Create booklet layouts | Missing |  |
| 129 | Create N-up page layouts | Partly | When printing (1, 2, 4, 6, 9 per sheet); not as a new PDF |
| 130 | Overlay PDF pages | Missing |  |
| 131 | Apply page templates | Missing |  |
| 132 | Manage PDF page boxes | Partly | Crop box only |

## 6. Annotations and Markup

| # | Feature | Status | Note |
|---|---|---|---|
| 133 | Highlight text | Done |  |
| 134 | Underline text | Done |  |
| 135 | Strikethrough text | Done |  |
| 136 | Add sticky notes | Done |  |
| 137 | Add comments | Done |  |
| 138 | Draw freehand | Done |  |
| 139 | Add arrows | Done |  |
| 140 | Draw lines | Done |  |
| 141 | Draw rectangles | Done |  |
| 142 | Draw circles | Done |  |
| 143 | Draw polygons | Missing |  |
| 144 | Add text callouts | Missing |  |
| 145 | Add cloud annotations | Missing |  |
| 146 | Insert stamps | Done |  |
| 147 | Create custom stamps | Done |  |
| 148 | Add date and time stamps | Missing |  |
| 149 | Attach files to annotations | Missing |  |
| 150 | Add audio annotations | Missing |  |
| 151 | Reply to comments | Missing |  |
| 152 | Filter comments | Missing |  |
| 153 | Export annotations | Partly | Notes summary as Markdown; no FDF/XFDF |
| 154 | Import annotations | Missing |  |
| 155 | Flatten annotations | Missing |  |
| 156 | Delete annotations | Done |  |
| 157 | Copy annotations between documents | Missing |  |
| 158 | Generate annotation summaries | Done |  |

## 7. OCR and Scanned PDF Editing

| # | Feature | Status | Note |
|---|---|---|---|
| 159 | Recognize text in scanned PDFs | Done |  |
| 160 | Convert scanned PDFs into searchable PDFs | Done |  |
| 161 | Convert scanned text into editable text | Partly | Invisible text layer: searchable and copyable, not editable |
| 162 | OCR individual pages | Done |  |
| 163 | OCR selected regions | Missing |  |
| 164 | Batch OCR multiple PDFs | Done | Batch mode |
| 165 | Automatic language detection | Missing |  |
| 166 | English OCR | Done |  |
| 167 | Kannada OCR | Missing | Needs the Kannada language file (Tesseract supports it) |
| 168 | Hindi OCR | Missing | Same |
| 169 | Tamil OCR | Missing | Same |
| 170 | Multilingual OCR | Partly | Any Tesseract language file placed in tessdata; only English is bundled |
| 171 | Handwriting recognition | Missing |  |
| 172 | Automatic page straightening | Missing |  |
| 173 | Remove scanning noise | Missing |  |
| 174 | Remove shadows | Missing |  |
| 175 | Improve scanned text clarity | Missing |  |
| 176 | Correct page perspective | Missing |  |
| 177 | Detect document boundaries | Missing |  |
| 178 | Separate double-page scans | Missing |  |
| 179 | Recognize tables | Partly | Tables found in text are exported to Excel/CSV |
| 180 | Recognize columns | Missing |  |
| 181 | Recognize mathematical equations | Missing |  |
| 182 | Display OCR confidence scores | Missing |  |
| 183 | Correct OCR errors manually | Missing |  |
| 184 | Reprocess low-confidence text | Missing |  |
| 185 | Preserve scanned document layout | Done | Text layer over the unchanged page picture |

## 8. PDF Conversion

| # | Feature | Status | Note |
|---|---|---|---|
| 186 | Word to PDF | Missing |  |
| 187 | PDF to Word | Done |  |
| 188 | Excel to PDF | Missing |  |
| 189 | PDF to Excel | Done | Tables |
| 190 | PowerPoint to PDF | Missing |  |
| 191 | PDF to PowerPoint | Missing |  |
| 192 | Images to PDF | Done |  |
| 193 | PDF to images | Done | PNG, JPEG |
| 194 | HTML to PDF | Missing |  |
| 195 | PDF to HTML | Missing |  |
| 196 | Markdown to PDF | Missing |  |
| 197 | Text to PDF | Missing |  |
| 198 | PDF to text | Done | Plain text and Markdown |
| 199 | SVG to PDF | Missing |  |
| 200 | PDF to SVG | Missing |  |
| 201 | TIFF to PDF | Done |  |
| 202 | PDF to TIFF | Missing |  |
| 203 | RTF to PDF | Missing |  |
| 204 | Create PDF/A files | Missing |  |
| 205 | Create PDF/X files | Missing |  |
| 206 | Validate PDF/UA files | Missing |  |
| 207 | Batch file conversion | Missing |  |
| 208 | Preserve hyperlinks during conversion | Missing |  |
| 209 | Preserve bookmarks during conversion | Partly | Kept by page operations; not in Word export |
| 210 | Preserve formatting wherever technically possible | Partly | Headings, lists and tables in Word export |

## 9. PDF Forms

| # | Feature | Status | Note |
|---|---|---|---|
| 211 | Create fillable PDF forms | Done |  |
| 212 | Edit existing PDF forms | Partly | Fill in and add fields; field properties cannot be changed |
| 213 | Insert text fields | Done |  |
| 214 | Insert checkboxes | Done |  |
| 215 | Insert radio buttons | Missing |  |
| 216 | Insert dropdown menus | Done |  |
| 217 | Insert list boxes | Missing |  |
| 218 | Insert date fields | Missing |  |
| 219 | Insert form buttons | Missing |  |
| 220 | Automatically detect form fields | Done | Finds blanks in flat forms, asks before adding |
| 221 | Validate form entries | Missing |  |
| 222 | Create calculated fields | Missing |  |
| 223 | Mark mandatory fields | Missing |  |
| 224 | Import form data | Partly | From a CSV sheet into filled-in copies |
| 225 | Export form data | Done | Excel or CSV |
| 226 | Populate forms from Excel or CSV | Done | CSV (save from Excel as CSV) |
| 227 | Batch-fill PDF forms | Done | One copy per row |
| 228 | Flatten completed forms | Missing |  |
| 229 | Reset form fields | Missing |  |
| 230 | Create reusable form templates | Missing |  |

## 10. Digital Signatures

| # | Feature | Status | Note |
|---|---|---|---|
| 231 | Draw handwritten signatures | Done |  |
| 232 | Type signatures | Done |  |
| 233 | Import signature images | Done |  |
| 234 | Create digital signatures | Done | Also makes your own certificate |
| 235 | Verify digital signatures | Done |  |
| 236 | Support certificate-based signing | Done | Windows certificates and .pfx files |
| 237 | Support multiple signatures | Partly | Each signature is a new field; signing an already signed file is untested |
| 238 | Add signature timestamps | Done | RFC 3161 time stamp server |
| 239 | Inspect signing certificates | Partly | Signer, time, reason and trust shown; no certificate detail view |
| 240 | Detect invalid signatures | Done |  |
| 241 | Support long-term signature validation | Missing |  |
| 242 | Preserve signed document integrity | Done | Signatures are added as incremental updates |
| 243 | Warn before modifying signed PDFs | Partly | Warns when a signed file was changed; not before editing |
| 244 | Add multiple signature fields | Missing |  |
| 245 | Export signature validation reports | Partly | Command line --checksig writes a report |

## 11. Security and Privacy

| # | Feature | Status | Note |
|---|---|---|---|
| 246 | Password-protect PDFs | Done |  |
| 247 | Encrypt PDF documents | Done | AES-256 |
| 248 | Configure document permissions | Done |  |
| 249 | Restrict printing | Done |  |
| 250 | Restrict copying | Done |  |
| 251 | Restrict editing | Done |  |
| 252 | Remove passwords with authorization | Done |  |
| 253 | Permanently redact text | Done |  |
| 254 | Permanently redact images | Done | Redacted pages become pictures |
| 255 | Search and redact sensitive information | Done |  |
| 256 | Remove metadata | Missing |  |
| 257 | Remove hidden information | Missing |  |
| 258 | Remove embedded attachments | Missing |  |
| 259 | Detect suspicious PDF scripts | Missing |  |
| 260 | Disable PDF JavaScript | Done | Feather PDF never runs scripts in PDFs |
| 261 | Open suspicious PDFs in a sandbox | Missing |  |
| 262 | Verify document integrity | Partly | Through signatures |
| 263 | Maintain security audit logs | Missing |  |
| 264 | Process documents completely offline | Done |  |
| 265 | Provide privacy and sanitization reports | Partly | Redaction check report |

## 12. PDF Compression and Optimization

| # | Feature | Status | Note |
|---|---|---|---|
| 266 | Compress PDF files | Done |  |
| 267 | Select compression quality | Done | Also "smaller than ... MB" |
| 268 | Compress embedded images | Done |  |
| 269 | Remove duplicate resources | Missing |  |
| 270 | Optimize embedded fonts | Missing |  |
| 271 | Remove unnecessary metadata | Missing |  |
| 272 | Optimize PDF object streams | Missing |  |
| 273 | Reduce scanned PDF sizes | Done | Lower dpi, greyscale |
| 274 | Batch-compress PDFs | Done |  |
| 275 | Preview estimated file-size reduction | Missing |  |
| 276 | Compare original and compressed quality | Partly | With visual compare |
| 277 | Lossless optimization mode | Missing |  |
| 278 | Configurable lossy compression | Done |  |
| 279 | Optimize PDFs for web viewing | Missing |  |
| 280 | Report compression results | Done |  |

## 13. PDF Repair and Diagnostics

| # | Feature | Status | Note |
|---|---|---|---|
| 281 | Detect corrupted PDFs | Partly | Clear message when a file cannot be opened |
| 282 | Repair recoverable PDF structures | Partly | Damaged files that open are rewritten cleanly when saved |
| 283 | Rebuild damaged cross-reference tables | Partly | Same |
| 284 | Detect missing fonts | Missing |  |
| 285 | Detect broken hyperlinks | Missing |  |
| 286 | Detect invalid objects | Missing |  |
| 287 | Inspect PDF structure | Missing |  |
| 288 | Inspect embedded resources | Missing |  |
| 289 | Inspect PDF streams | Missing |  |
| 290 | Validate PDF standards | Missing |  |
| 291 | Identify oversized images | Missing |  |
| 292 | Identify unnecessary objects | Missing |  |
| 293 | Generate document diagnostic reports | Missing |  |
| 294 | Preview proposed repairs | Missing |  |
| 295 | Compare files before and after repair | Partly | With compare |

## 14. PDF Comparison

| # | Feature | Status | Note |
|---|---|---|---|
| 296 | Compare two PDF documents | Done |  |
| 297 | Compare text changes | Done |  |
| 298 | Compare image changes | Done | Visual overlay |
| 299 | Compare page layouts | Done | Visual overlay |
| 300 | Compare vector graphics | Done | Visual overlay |
| 301 | Highlight additions | Done | Green in the overlay |
| 302 | Highlight deletions | Done | Red in the overlay; words highlighted in the copy |
| 303 | Highlight formatting changes | Missing |  |
| 304 | Compare scanned documents | Done | Visual overlay |
| 305 | Side-by-side comparison | Done |  |
| 306 | Overlay comparison | Done |  |
| 307 | Pixel-level comparison | Done |  |
| 308 | Compare document versions | Done |  |
| 309 | Export comparison reports | Partly | The comparison opens as a PDF you can save |
| 310 | Filter insignificant differences | Partly | Built-in tolerance for anti-aliasing; not adjustable |

## 15. AI-Powered Features

| # | Feature | Status | Note |
|---|---|---|---|
| 311 | Chat with PDF documents | Missing |  |
| 312 | Summarize entire PDFs | Missing |  |
| 313 | Summarize selected pages | Missing |  |
| 314 | Explain selected text | Partly | Look up: dictionary, Wikipedia (opens the browser) |
| 315 | Explain technical terminology | Partly | Same |
| 316 | Translate PDF content | Partly | Selected text in Google Translate (browser) |
| 317 | Rewrite selected text | Missing |  |
| 318 | Correct grammar | Missing |  |
| 319 | Improve readability | Missing |  |
| 320 | Extract important information | Missing |  |
| 321 | Extract tables | Done | To Excel/CSV |
| 322 | Extract equations | Missing |  |
| 323 | Generate study notes | Partly | Notes summary of highlights and comments |
| 324 | Generate questions and answers | Missing |  |
| 325 | Generate flashcards | Missing |  |
| 326 | Automatically create bookmarks | Missing |  |
| 327 | Automatically generate tables of contents | Missing |  |
| 328 | Compare documents semantically | Missing |  |
| 329 | Identify contradictory statements | Missing |  |
| 330 | Detect sensitive information | Missing |  |
| 331 | Classify documents automatically | Missing |  |
| 332 | Search documents using natural language | Missing |  |
| 333 | Support local offline AI models | Missing |  |
| 334 | Provide source references for AI answers | Missing |  |
| 335 | Require approval before AI modifies documents | Missing |  |

## 16. Batch Processing and Automation

| # | Feature | Status | Note |
|---|---|---|---|
| 336 | Batch merge PDFs | Done | Merge several files at once |
| 337 | Batch split PDFs | Missing |  |
| 338 | Batch rotate pages | Missing |  |
| 339 | Batch crop pages | Missing |  |
| 340 | Batch add watermarks | Done |  |
| 341 | Batch OCR | Done |  |
| 342 | Batch conversion | Missing |  |
| 343 | Batch compression | Done |  |
| 344 | Batch image extraction | Missing |  |
| 345 | Batch text extraction | Missing |  |
| 346 | Batch rename files | Missing |  |
| 347 | Batch find and replace | Missing |  |
| 348 | Batch redaction | Partly | One file per command (--redact) |
| 349 | Batch page numbering | Missing |  |
| 350 | Create reusable processing presets | Done |  |
| 351 | Visual workflow builder | Missing |  |
| 352 | Record repetitive actions | Missing |  |
| 353 | Monitor folders for new PDFs | Missing |  |
| 354 | Command-line automation | Done | Export, compress, protect, OCR, sign, batch, search, redact, forms, compare |
| 355 | Python scripting support | Missing | Python can call the command line |
| 356 | Local REST API | Missing |  |
| 357 | Background processing | Missing | Long jobs show progress and can be cancelled, but block the window |
| 358 | Pause and resume jobs | Missing | Cancel only |
| 359 | Retry failed operations | Missing |  |
| 360 | Export processing logs | Partly | Command-line batch writes a log; the window shows a summary |

## 17. Engineering and Scientific Features

| # | Feature | Status | Note |
|---|---|---|---|
| 361 | Measure distances in PDF drawings | Done |  |
| 362 | Measure angles | Done |  |
| 363 | Measure areas | Missing |  |
| 364 | Measure perimeters | Missing |  |
| 365 | Calibrate drawing scales | Partly | Fixed scales 1:1 ... 1:100, 2:1 ... 10:1; no calibration from a known length |
| 366 | Support engineering measurement units | Done | mm, cm, in, pt |
| 367 | Support A0, A1, A2, A3 and A4 drawings | Partly | Any page size opens and measures; printing fits or prints actual size |
| 368 | Add engineering dimensions | Partly | Measurements can be kept on the page |
| 369 | Add leaders and dimension arrows | Partly | Arrows and lines |
| 370 | Add geometric symbols | Missing |  |
| 371 | Insert mathematical equations | Missing |  |
| 372 | Support LaTeX equation insertion | Missing |  |
| 373 | Extract scientific charts | Partly | Copy area as image |
| 374 | Extract engineering diagrams | Partly | Copy area as image |
| 375 | Compare engineering drawing revisions | Done | Visual overlay |
| 376 | Highlight changes in technical drawings | Done | Visual overlay |
| 377 | Inspect vector geometry | Missing |  |
| 378 | Display drawing coordinates | Missing |  |
| 379 | Verify print scales | Partly | Print at actual size |
| 380 | Export measurement reports | Missing |  |
| 381 | View supported 3D PDFs | Missing |  |
| 382 | Control drawing layer visibility | Missing |  |
| 383 | Add tolerance annotations | Missing |  |
| 384 | Create technical annotation templates | Missing |  |
| 385 | Convert suitable raster drawings into vector graphics | Missing |  |

## 18. Accessibility

| # | Feature | Status | Note |
|---|---|---|---|
| 386 | Create tagged PDFs | Missing |  |
| 387 | Edit PDF tags | Missing |  |
| 388 | Edit reading order | Missing |  |
| 389 | Add alternative text to images | Missing |  |
| 390 | Support screen readers | Missing | Standard Windows controls only; the page view is not exposed |
| 391 | Keyboard-only navigation | Partly | Many shortcuts and the command palette; some tools need a mouse |
| 392 | High-contrast mode | Missing | Dark theme only |
| 393 | Adjustable interface scaling | Partly | Follows Windows display scaling |
| 394 | Accessible form creation | Missing |  |
| 395 | Detect accessibility issues | Missing |  |
| 396 | Generate accessibility reports | Missing |  |
| 397 | Validate PDF/UA compliance | Missing |  |
| 398 | Support multilingual accessibility | Missing |  |
| 399 | Improve table accessibility | Missing |  |
| 400 | Repair document structure tags | Missing |  |

## 19. Printing and Publishing

| # | Feature | Status | Note |
|---|---|---|---|
| 401 | Print selected pages | Done |  |
| 402 | Print custom page ranges | Done |  |
| 403 | Print multiple pages per sheet | Done |  |
| 404 | Duplex printing | Missing | No printer properties button yet |
| 405 | Booklet printing | Missing |  |
| 406 | Poster printing | Missing |  |
| 407 | Print engineering drawings at actual scale | Done | "Actual size" |
| 408 | High-resolution printing | Done | At the printer's resolution |
| 409 | Grayscale printing | Missing |  |
| 410 | Color profile management | Missing |  |
| 411 | Print annotations | Done |  |
| 412 | Print form fields | Done |  |
| 413 | Print selected document regions | Missing |  |
| 414 | Add crop marks | Missing |  |
| 415 | Add bleed marks | Missing |  |
| 416 | Validate print resolution | Missing |  |
| 417 | Print preview | Missing |  |
| 418 | Detect paper-size mismatches | Partly | Orientation follows the pages automatically |
| 419 | Create print presets | Missing |  |
| 420 | Preflight PDF documents | Missing |  |

## 20. Performance and Reliability

| # | Feature | Status | Note |
|---|---|---|---|
| 421 | Fast application startup | Done | Native program, no runtime |
| 422 | Fast PDF opening | Done |  |
| 423 | GPU-accelerated rendering | Missing |  |
| 424 | Multithreaded processing | Missing |  |
| 425 | Lazy page loading | Done |  |
| 426 | Smooth scrolling | Done | Double-buffered drawing |
| 427 | Efficient memory management | Partly | Page cache; no memory limit setting |
| 428 | Support very large PDFs | Partly | Up to 4 GB; very large pages are drawn in parts |
| 429 | Background thumbnail generation | Partly | Thumbnails are drawn when they come into view |
| 430 | Incremental saving where appropriate | Partly | Signatures and bookmarks; ordinary saves rewrite the file |
| 431 | Automatic crash recovery | Done |  |
| 432 | Automatic document backup | Partly | Unsaved changes are kept every 20 s until a normal exit |
| 433 | Unlimited undo and redo within configured history limits | Done | 30 steps |
| 434 | Non-destructive editing workflow | Partly | Safe save, Save as, undo; no layers |
| 435 | Detect external file modifications | Partly | Reload from disk; no automatic notice |
| 436 | Low-memory mode | Missing |  |
| 437 | Configurable cache | Missing |  |
| 438 | Portable Windows version | Missing |  |
| 439 | Automatic update system | Missing |  |
| 440 | Performance diagnostic tools | Missing |  |

## 21. User Interface and Customization

| # | Feature | Status | Note |
|---|---|---|---|
| 441 | Modern Windows 11 interface | Partly | Light/dark window, dark title bar; not the Fluent design |
| 442 | Traditional toolbar mode | Done |  |
| 443 | Ribbon interface mode | Missing |  |
| 444 | Minimal reading mode | Partly | Full screen |
| 445 | Professional editing mode | Missing |  |
| 446 | Customizable toolbars | Missing |  |
| 447 | Customizable keyboard shortcuts | Missing |  |
| 448 | Command palette | Done |  |
| 449 | Dockable side panels | Missing |  |
| 450 | Object properties inspector | Missing |  |
| 451 | Layer panel | Missing |  |
| 452 | Thumbnail panel | Done |  |
| 453 | Bookmark panel | Done |  |
| 454 | Annotation panel | Partly | Notes summary |
| 455 | Search results panel | Partly | Folder search results list; find bar for the open tab |
| 456 | Drag-and-drop support | Done |  |
| 457 | Context-sensitive right-click menus | Done |  |
| 458 | Customizable themes | Done | Light, dark, like Windows |
| 459 | Multi-monitor support | Partly | Per-monitor display scaling |
| 460 | Workspace presets | Missing |  |

## 22. Collaboration and Integrations

| # | Feature | Status | Note |
|---|---|---|---|
| 461 | Import and export comments | Partly | Export as a Markdown summary only |
| 462 | Track document versions | Missing |  |
| 463 | Compare revisions | Done |  |
| 464 | Assign review comments | Missing |  |
| 465 | Mark comments resolved | Missing |  |
| 466 | Optional cloud storage integration | Missing | Files in synced folders work like local files |
| 467 | Google Drive integration | Missing | Same |
| 468 | OneDrive integration | Missing | Same |
| 469 | Dropbox integration | Missing | Same |
| 470 | WebDAV support | Missing |  |
| 471 | Network-folder support | Partly | Network paths open and save like local files (untested) |
| 472 | Email PDF documents | Missing |  |
| 473 | Microsoft Office integration | Missing |  |
| 474 | Scanner integration | Missing |  |
| 475 | Third-party plugin support | Missing |  |
| 476 | External application integration | Partly | Command line, "Open with", default viewer |
| 477 | Optional collaborative editing | Missing |  |
| 478 | Conflict detection | Missing |  |
| 479 | Share document review packages | Missing |  |
| 480 | Export review histories | Missing |  |

## 23. Advanced Competitive Features

| # | Feature | Status | Note |
|---|---|---|---|
| 481 | Edit PDF content using a visual object tree | Missing |  |
| 482 | Switch between visual and structural editing | Missing |  |
| 483 | Smart layout-preserving text replacement | Partly | Same font and place where the font allows |
| 484 | Smart image replacement | Missing |  |
| 485 | Automatic object alignment | Partly | Snapping guides |
| 486 | Scan-to-editable-layout reconstruction | Missing |  |
| 487 | Editable table reconstruction | Partly | Tables to Excel; tables made in Feather PDF are editable |
| 488 | Automatic equation recognition | Missing |  |
| 489 | Technical diagram vectorization | Missing |  |
| 490 | Visual document quality analysis | Missing |  |
| 491 | One-click document optimization with preview | Missing |  |
| 492 | Before-and-after editing comparison | Partly | Compare the saved file with an edited copy |
| 493 | Document snapshots | Partly | Undo history and crash-recovery copies |
| 494 | Selective version rollback | Missing |  |
| 495 | Automatic document organization | Missing |  |
| 496 | Offline document indexing | Done | Folder search index |
| 497 | Advanced cross-document search | Done | Folder search |
| 498 | Custom automation plugins | Missing |  |
| 499 | Detailed modification reports | Missing |  |
| 500 | Built-in performance benchmarking | Missing |  |
