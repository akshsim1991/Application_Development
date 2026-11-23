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

## UI redesign (matches the supplied mockup)
All items below are `built, untested on Windows` (the logic parts - aspect ratio, filters/counts, histogram maths, favorites - are unit-tested in Core).

| Mockup element | Implementation |
|---|---|
| Custom title bar with feather logo and caption buttons | WindowChrome, own minimize/maximize/close |
| Sidebar: Gallery, Folders, Favorites, Recent, Search | Gallery = thumbnail grid; Folders = folder picker (a folder tree is planned); Favorites/Recent = collections shown in the gallery; Search = live filename filter |
| Quick Filters with counts: All, JPG, PNG, GIF, Other (+ Videos) | Real filters; counts always show each category's total in the folder |
| Rounded toolbar: back/forward, zoom drop-down, fit, gallery, rotate, actual size, flip, slideshow, more | Same icons; zoom menu has Fit/25/50/100/200/400 %; "..." menu holds open, recent, rotate left, copy, delete, reload, sort, panels, settings, updates, about |
| Rounded picture area | Clipped corners |
| Details panel with Info / Histogram / EXIF tabs, thumbnail, name, type/size, dimensions + ratio, Image Details, Taken On (date, camera, lens) | Info tab (details table, Taken On from metadata), Histogram tab (luminance + RGB), EXIF tab (all metadata found) |
| Filmstrip with scroll chevrons | Virtualized strip (toggle with T), shown while viewing a picture or video |
| Status bar: "248 items - 1 selected", Set as Wallpaper, Print, Open Folder, full screen | Real actions (wallpaper asks first and stores a PNG copy in the app data folder; print uses the Windows print dialog) |
Not in the mockup but added: Favorites toggle (star, Ctrl+D), sidebar/filmstrip/details toggles (Ctrl+B / T / I).

## Ideas taken from the 'Aura Photos' screenshot (AI/editing/cloud parts deliberately left out)
| Item | Status |
|---|---|
| Floating photo toolbar on the picture: zoom out/in, fit, compare, shuffle | built, untested on Windows |
| Compare side by side (current + next picture, zoom and pan stay in sync; Esc closes) | built, untested on Windows |
| Slideshow shuffle (random, no repeats until every picture was shown) | logic tested (Core); UI untested on Windows; also a Settings checkbox |
| Details panel as collapsible cards: Details, Camera (one-line summary + date/camera/lens), Location, EXIF Data, Histogram, Text in picture | built, untested on Windows; camera summary and GPS maths tested |
| Location card: coordinates from the picture's metadata; the map opens in the browser only after you click and confirm | built, untested on Windows |
| Text in picture: offline OCR with Windows' built-in recognizer (needs an installed OCR language), runs only on click, copy button | built, untested on Windows |
| Not adopted: edit tools, AI scene tags, quality score, Copilot/AI search, interactive map, Albums/Projects/Devices | out of scope (editing/AI/cloud) or V3 |

## Full screen and video (fixes after the first hands-on test)
| Item | Status |
|---|---|
| True full screen: covers the whole monitor, hides the taskbar, no borders or margins, black background; Esc or F11 leaves it (works even when focus is inside the video window) | **verified on a Windows 1024x768 CI machine (F11 fills the screen exactly, Esc restores)**; video full screen not yet verified |
| Full screen: video controls and the mouse pointer start hidden, appear when the mouse moves and hide again after N seconds (Settings, default 5) | built, untested on Windows |
| Space = play/pause in video mode, also when the video window has focus | built, untested on Windows |
| Setting "Keep a video playing when I switch to another tab" (default on): the sound continues, the picture is not decoded while hidden, a speaker icon marks the tab, returning to the tab shows the same playback; closing the tab stops it | rules unit-tested (Core); UI untested on Windows |

## Folders and drives
| Item | Status |
|---|---|
| Folders inside the opened folder are listed above the pictures; click one to go in; Up button and the folder path | built, untested on Windows |
| Opening a folder or drive shows the pictures of ALL folders underneath in one go (on by default; menu/Settings switch). Runs in the background with a live count, never freezes the window; up to 200,000 items; pictures stay grouped by folder; single files opened from Explorer still open instantly | scanner (cancel, progress, grouping, limit) tested in Core; UI being verified in CI |
| Opening a drive or a folder that holds only sub-folders now explains this instead of showing an empty window | built, untested on Windows |

## Video fixes after hands-on testing (2.1.1)
| Item | Status |
|---|---|
| Subtitles in the built-in player (SRT, VTT, ASS text): auto-load next to the video, load file, delay, size/colour/background/position | parser tested (Core, 6 tests); on-screen drawing untested on Windows |
| Aspect ratio (original, 16:9, 4:3, 21:9, 1:1) in the built-in player | built, untested on Windows |
| "Download extended video support" works: the build publishes an LGPL libmpv build as release `featherview-engine` (commit message tag [engine]) | being verified in CI |
| Seek bar: one seek on release instead of one per mouse move; the bar no longer jumps back after a click; repeated seek keys add up; built-in player's scrubbing only while paused | built, untested on Windows (big files are still slow in the built-in player - the extended engine seeks much faster) |
| Full-screen video controls hide after N seconds (Settings, default 5); phantom mouse-move guard; CI test with screenshots (entered / after mouse move / after 8 s idle) | being verified in CI |

## Tabs (V2.1 item brought forward)
| Item | Status |
|---|---|
| Tab strip in the title bar (new, close, switch, middle-click close, right-click menu) | built, untested on Windows |
| Independent state per tab: folder or collection (Favorites/Recent), selected file, view mode, quick filter, search | built, untested on Windows; the state logic is unit-tested |
| Rename, duplicate, close others, reopen closed tab (last 10), up to 20 tabs | logic tested (Core); UI untested on Windows |
| Ctrl+T new, Ctrl+W close, Ctrl+Shift+T reopen, Ctrl+Tab / Ctrl+Shift+Tab switch | built, untested on Windows (Ctrl+T used to be "go to time", now Ctrl+J) |
| Open folder in a new tab (More menu) | built, untested on Windows |
| Session restore: tabs reopen at start-up (setting "Reopen my tabs") | logic tested (Core); wiring untested on Windows |
| Low memory by design: inactive tabs hold a small record only; switching re-reads the folder, decoded images exist only for the active tab | by design |
Not done: dragging tabs to reorder, separate windows, tab-specific zoom position, pinned tabs.

## Phase 3 - Video playback (engine option A: Windows built-in + optional libmpv)
`built, untested on Windows` = code complete; the libmpv wrapper itself **is** tested against a real libmpv 0.37 in the dev container (load, tracks, info, exact seek, frame step, speed, A-B, volume, subtitles/audio selection, EOF/error events). The WPF player UI and window embedding have never run.

| Item | Built-in (Media Foundation) | Extended (libmpv, optional download) |
|---|---|---|
| Play/pause, seek bar, volume, mute, loop, speed 0.25x-4x | built, untested on Windows | built, untested on Windows |
| Frame-accurate seek, frame step back/forward | no (engine limitation) | built, untested on Windows |
| A-B repeat | no | built, untested on Windows |
| Subtitles (SRT/ASS/SSA/VTT/SUB), embedded + external, delay, size/colour/position/background | no | built, untested on Windows |
| Audio track selection, external audio, audio delay | no | built, untested on Windows |
| Volume above 100% (to 200%) | no | built, untested on Windows |
| Chapters (list, next/previous) | no | built, untested on Windows |
| Screenshot (saved to Pictures\Feather View) | built (renders the player surface) | built (video frame only) |
| Rotate, aspect ratio | rotate only | built, untested on Windows |
| Info panel (codec, bitrate, fps, HDR flag, tracks, container...) | size/duration only | built, untested on Windows |
| Hardware decoding (D3D11VA auto-safe), HDR flag display | automatic in Windows | built, untested on Windows (real HDR output not verified) |
| Resume position, playback history, volume memory | built, tested (Core) | built, tested (Core) |
| Direct time entry (Ctrl+J), mouse-wheel volume, click = play/pause, double-click = full screen | built, untested on Windows | built, untested on Windows |
| Automatic fallback: built-in engine fails -> libmpv if installed; otherwise a clear message with a download button | built, untested on Windows | |
| Extended-engine download (HTTPS, host allow-list, SHA-256, safe unzip) | built, tested (Core, stub server) | needs `engine.json` + ZIP published in the releases repo (see docs/UPDATES.md) |
| Auto-hiding controls, timeline thumbnails, picture-in-picture, multi-monitor choice, deinterlace options, audio device choice | planned (V2.1/2.2) | planned |

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
