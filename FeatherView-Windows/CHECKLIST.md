# Feather View - feature checklist

Legend: **done** = implemented and compiled (unit-tested where logic is UI-free) | **built, untested on Windows** = code complete, awaiting your testing | **planned** = not started.
Nothing below is marked done unless the code exists. "Verified" means the Core tests pass and the whole app compiles against the WPF reference assemblies; the running UI has not been exercised yet.

## Phase 1 - Foundation
| Item | Status |
|---|---|
| Solution split: Core (no UI) / App (WPF, MVVM) / Tests | done |
| Dark / Light / System theme, live switching | built, untested on Windows |
| Lucide-style vector icons, tooltips, minimal toolbar | built, untested on Windows |
| JSON settings, atomic save, corrupt-file recovery, value clamping | done (tested) |
| Privacy-safe log (user paths masked), global exception handling | done (tested) |
| Automatic updates (daily check, ask first, HTTPS + host allow-list, size + SHA-256 verification) | done in Core (tested); UI built, untested |

## Phase 2 - Photo viewing
| Item | Status |
|---|---|
| JPG, PNG, BMP, GIF, TIFF, ICO via WIC; WebP/HEIC/AVIF when the Windows codec is installed | built, untested on Windows |
| Next/previous, first/last, wrap-around | built, untested on Windows |
| Zoom at cursor, pan, fit, actual size, double-click toggle | built, untested on Windows |
| Rotate left/right, flip (view only, file never modified) | built, untested on Windows |
| EXIF orientation applied; details panel (size, dimensions, camera, exposure...). GPS is never read | built, untested on Windows |
| Animated GIF with correct timing and frame disposal | built, untested on Windows |
| Slideshow (interval, loop, full screen, Esc to stop) | built, untested on Windows |
| Clipboard copy (image + file) | built, untested on Windows |
| Delete to Recycle Bin with confirmation | built, untested on Windows |
| Corrupted/unsupported file handled with a message, not a crash | built, untested on Windows |
| SVG, RAW | planned |

## Phase 3 - Video playback: **planned** (Media Foundation first, optional libmpv later)
Play/pause, seeking, volume, speed, frame step, screenshots, subtitles, audio tracks, resume, loop, info. Video files already appear in the gallery and show a clear "not available yet" message.

## Phase 4 - Gallery and browsing
| Item | Status |
|---|---|
| Thumbnail gallery, virtualized (only visible rows exist), keyboard navigation | built, untested on Windows |
| Lazy async thumbnails, bounded LRU cache, limited parallelism | built, untested on Windows |
| Folder scan, natural sort (Name/Date/Size/Type, ascending/descending) | done (tested) |
| Drag and drop, recent files, show in Explorer, reload folder | built, untested on Windows |
| Filtering, folder tree, persistent on-disk thumbnail cache | planned |

## Phase 5 - Settings: appearance, general, browsing, photo, slideshow, performance, updates, shortcut list, default-apps link | built, untested on Windows (custom key remapping planned)
## Phase 6 - Windows integration: installer with "Open with" registration, command-line open, Explorer selection | built in CI, untested on Windows (default-app prompt cannot be forced by apps on Windows 10/11)
## Phase 7 - Performance: CI smoke test records start-up time, memory and responsiveness on a Release build | in CI (see run summary)
## Phase 8 - Packaging: self-contained installer, portable ZIP, framework-dependent ZIP, latest.json | in CI
