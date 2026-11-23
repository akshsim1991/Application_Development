# Feather View V2 - specification and roadmap (analysis only; no source was changed)

Audit date 2026-11-23. Basis: the V1 code in `FeatherView-Windows/` as committed (`2a101dd`), plus the GitHub Actions results.

---
## 0. Read this first - honest state of V1

| Fact | Detail |
|---|---|
| Size | Core 542 lines C#, App 1,401 lines C# + 549 lines XAML, tests 143 lines (18 xUnit tests). |
| Stack | C# / .NET 10 / WPF / hand-written MVVM (no MVVM library). **Zero third-party runtime dependencies.** Test project uses xunit only. |
| **Build status** | **The WPF app has never compiled in CI.** Run 1 failed (`ThemeMode` clashes with .NET 10's `System.Windows.ThemeMode`; fixed in `2a101dd`). Run 2 fails on `FileOperations.cs(13)`: `File` unresolved because the WPF SDK's implicit usings do not include `System.IO` (one-line fix: add `using System.IO;`). More compile errors may follow, because my container could only type-check against .NET 8 reference assemblies. |
| Consequence | Every UI feature is *written but unexecuted*. The 18 Core tests pass on .NET 10 in CI. Nothing in the App (XAML bindings, virtualization, GIF animator, slideshow, update UI) has been run. V2 planning must treat V1 as "unverified prototype", not as a stable base. |
| Video | Not implemented. Video files are scanned and listed (tile says "VIDEO"); opening one shows "not available yet". |

**Milestone 0 of the plan is therefore: get V1 green in CI and run it on real Windows.** Everything below depends on that.

### V1 architecture, as found
```
FeatherView.Core (net10.0, no UI)         FeatherView.App (net10.0-windows, WPF)
  Settings: AppSettings, SettingsService    Views: MainWindow, SettingsWindow, UpdateWindow, ProgressWindow, MessageWindow
  Media: MediaTypes, MediaScanner,          ViewModels: MainViewModel (398 lines), GalleryViewModel, MediaItemViewModel
         NaturalComparer                    Services: ImageLoader, ThumbnailService, GifAnimator, ExifReader, ThemeService,
  Updates: UpdateService (HTTPS, host              UpdateController, WpfDialogService, FileOperations, Converters
         allow-list, size + SHA-256)        Controls: ZoomPanImage (custom FrameworkElement)
  Infrastructure: FileLog, LruCache         Themes: Dark/Light brushes, Icons (Lucide-style paths), Controls (templates)
  Mvvm: ObservableObject, RelayCommand
```
**Reusable as-is:** all of Core; ZoomPanImage (cursor-anchored zoom, pan, fit, actual size); ThumbnailService (bounded LRU, limited parallelism); ThemeService; update pipeline; MediaScanner/NaturalComparer; ExifReader orientation logic.

### Defects, debt and risks found in V1
| # | Finding | Impact | V2 action |
|---|---|---|---|
| 1 | `MainViewModel` is a 398-line god-object (navigation, loading, GIF, slideshow, delete, clipboard, settings, fullscreen). | Tabs, video, comparison and customizable commands cannot be added cleanly. | Split before adding features (Milestone 1). |
| 2 | Gallery holds one `MediaItemViewModel` per file and rebuilds *all* rows on every resize/sort. | Fine to ~20k files, sluggish beyond; no incremental updates. | Chunked/lazy row model, incremental diff. |
| 3 | Thumbnails are memory-only. | Every launch re-decodes everything. | Persistent on-disk cache (keyed path+mtime+size). |
| 4 | Full-resolution decode for viewing (`BitmapCacheOption.OnLoad`). | A 100 MP image is about 400 MB of RAM. | Fit-size decode + on-demand high-res/tiled decode. |
| 5 | ICC profiles ignored: thumbnails use `IgnoreColorProfile`; viewer does not colour-convert. | Wrong colours for AdobeRGB/Display-P3 photos. | `ColorConvertedBitmap` to sRGB/monitor. |
| 6 | GIF animator allocates a `RenderTargetBitmap` per frame. | GC churn on large GIFs. | Reuse a `WriteableBitmap`; cap frame cache. |
| 7 | No `FileSystemWatcher`; folder changes need F5. | Stale views after delete/copy elsewhere. | Debounced watcher per tab. |
| 8 | Shortcuts hard-coded in XAML `InputBindings`. | Not customizable. | Command registry + gesture map in settings. |
| 9 | Icon buttons have tooltips but no `AutomationProperties.Name`. | Screen readers announce nothing. | Names on every control; keyboard focus visuals. |
| 10 | Update trust = SHA-256 from the same GitHub origin; installer is not Authenticode-signed. | SmartScreen warning; no protection if the releases repo is compromised. | Code-signing certificate; optionally signed manifest. |
| 11 | No single-instance handling, no crash recovery, no portable mode. | Each double-click starts a new process. | Named-pipe single instance; recovery file; `portable.flag`. |
| 12 | ViewModels depend on WPF types (`BitmapSource`), so they are unit-testable only on Windows. | Thin regression net. | Abstract image handles behind interfaces for tests. |
| 13 | Settings window edits a POCO with no validation UI; no backup/restore. | Minor. | Versioned schema + export/import. |

---
## A/B/C. Master feature matrix, release plan and technical plan

**V1 status:** `Done*` = code exists but has never run (see section 0) | `Partial` | `Missing` | `Improve` (exists, needs work) | `Not recommended`.
**Release:** 2.0 essential | 2.1 productivity | 2.2 advanced media | 3 future | `X` excluded.
**Needs:** `Native` = Windows/.NET only | `MF` = Media Foundation | `mpv` = libmpv (new dependency) | `+lib` = other library | `UI` = pure feature work.
**Cx:** S < 1 day, M 1-3 days, L 1-2 weeks, XL > 2 weeks. **Perf** = expected cost: 0 none, low, med, high.

### 2. Photo viewing
| Feature | V1 | Rel | Needs | Cx | Perf | Notes / compatibility |
|---|---|---|---|---|---|---|
| Zoom, cursor-anchored zoom | Done* | 2.0 | Native | S | 0 | Exists in `ZoomPanImage`. Add zoom limits and 1:1 snap. |
| Smooth panning | Partial | 2.0 | Native | M | 0 | Drag works; add inertia and keyboard pan. |
| Fit / actual size | Done* | 2.0 | Native | S | 0 | Add "fit width" and "fill". |
| Continuous next/previous | Done* | 2.0 | Native | S | low | Add neighbour pre-decode (next/prev) with cancellation. |
| Thumbnail filmstrip | Missing | 2.0 | Native | M | low | Reuse `ThumbnailService`; horizontal virtualizing list; toggle. |
| Borderless viewing | Partial | 2.0 | Native | S | 0 | Full-screen exists; add borderless window mode. |
| Side-by-side comparison | Missing | 2.1 | Native | L | med | Two synced `ZoomPanImage`s; doubles decoded memory. |
| Multi-page TIFF | Missing | 2.1 | Native | M | low | WIC `decoder.Frames`; page control. |
| Animated GIF | Done* | 2.0 | Native | S | low | Fix per-frame allocation (finding 6). |
| Animated WebP | Missing | 2.2 | +lib (spike) | L | med | WIC's WebP decoder is codec-extension dependent and does not expose animation reliably. Needs a spike: libwebp via a small managed binding or SkiaSharp (about 10 MB). Do not promise until proven. |
| Rotate / flip (view only) | Done* | 2.0 | Native | S | 0 | Add vertical flip, remember per file (session). |
| Slideshow | Done* | 2.0 | Native | S | low | Exists. |
| Slideshow shuffle | Missing | 2.0 | Native | S | 0 | Fisher-Yates over indices; no repeats until all shown. |
| Slideshow transitions | Missing | 2.1 | Native | M | low | Cross-fade only (opacity animation); no 3D effects. |
| EXIF information | Partial | 2.0 | Native | M | 0 | Basic fields exist; add lens, full panel, GPS display (V1 deliberately never reads GPS; V2 shows it only on request, never uploads). |
| ICC colour profile | Missing | 2.0 | Native | M | low | `ColorConvertedBitmap`; see finding 5. |
| Transparency checkerboard | Missing | 2.0 | Native | S | 0 | Tiled brush behind the image. |
| Set as wallpaper | Missing | 2.1 | Native | S | 0 | `SystemParametersInfo(SPI_SETDESKWALLPAPER)`; copy file to app data first. |
| Printing | Missing | 2.1 | Native | M | 0 | WPF `PrintDialog.PrintVisual`; fit/fill/actual options. |
| Mouse side-button navigation | Missing | 2.0 | Native | S | 0 | `XButton1/XButton2` events. |
| Remember zoom/position | Missing | 2.0 | Native | S | 0 | Per-session dictionary; optional persisted per folder. |
| EXIF orientation | Done* | 2.0 | Native | S | 0 | Exists (viewer and thumbnails). |
| Multiple backgrounds | Missing | 2.0 | Native | S | 0 | Theme/black/white/checker/custom colour. |
| Full-screen controls | Partial | 2.0 | Native | M | 0 | V1 hides all chrome; V2 needs an auto-hiding overlay bar. |
| High-resolution images | Partial | 2.0 | Native | L | high | Finding 4. Decode at fit size first, upgrade on zoom; hard memory cap; low-memory detection. |
| SVG | Missing | 2.1 | +lib (SharpVectors, MIT, ~1 MB) | M | low | Optional. |
| HEIC / AVIF | Partial | 2.0 | Native (Windows codec) | S | 0 | Works only if the user has Microsoft's HEIF/AV1 extensions; show an actionable message when WIC cannot decode. HEVC-based HEIC may need the paid HEVC extension. |
| RAW | Missing | 3 | Native codec / +lib | XL | med | Camera-specific; see section 15. |

### 3. Video playback (all Missing in V1)
**Engine decision (needs your approval, see section 18 and the closing questions).** Media Foundation (WPF `MediaElement`/`MediaPlayer`) is built into Windows and has zero dependencies, but cannot do frame-accurate seeking, frame stepping, audio-track selection, subtitle rendering, A-B repeat, chapters or MKV without codec packs. `libmpv` can do all of them but is a ~30-40 MB native dependency with licensing obligations. Recommendation: **an `IVideoEngine` abstraction with two back-ends - Media Foundation built-in (basic) and libmpv as an optional download** - or libmpv alone if you prefer one code path.

| Feature | Rel | Needs | Cx | Perf | Notes |
|---|---|---|---|---|---|
| Play/pause, seek, volume, mute, full screen | 2.0 | MF or mpv | M | low | Baseline on either engine. |
| Frame-accurate seeking | 2.0 | **mpv** | M | low | `hr-seek=yes`. MF seeks to keyframes. |
| Speed 0.25x-4x | 2.0 | MF (limited) / mpv | S | low | MF `SpeedRatio` is unreliable below ~0.5x and drops audio pitch handling; mpv corrects pitch. |
| Frame step | 2.0 | **mpv** | S | 0 | `frame-step`, `frame-back-step`. |
| Loop; A-B repeat | 2.0 / 2.1 | mpv (A-B), either (loop) | S / M | 0 | `ab-loop-a/b`. |
| Resume position, playback history | 2.0 | Native (JSON) | S | 0 | Store position per file hash (path+size+mtime); capped list. |
| External + embedded subtitles; SRT/ASS/SSA/VTT/SUB | 2.0 | **mpv** | M | low | mpv renders ASS/SSA with libass. MF has no subtitle renderer; a custom SRT/VTT overlay is possible but ASS styling is not. |
| Subtitle sync, font, colour, position, background opacity, language, remember prefs | 2.1 | mpv | M | 0 | `sub-delay`, `sub-font-size`, `sub-color`, `sub-pos`, `sub-back-color`. |
| Multiple audio tracks; external audio; audio delay | 2.0 / 2.1 | mpv | M | 0 | `aid`, `audio-add`, `audio-delay`. |
| Screenshot / save video frame | 2.0 | mpv (or MF render capture) | S | 0 | `screenshot-to-file`; saves a file only, no editing. |
| Aspect ratio selection; video rotation | 2.1 | mpv | S | 0 | `video-aspect-override`, `video-rotate`. Display-only. |
| Codec, bitrate, frame-rate, colour space, HDR, channels info | 2.0 | mpv properties (MF cannot) | M | 0 | `track-list`, `video-params`. |
| Volume memory; amplification (to 200%) | 2.0 | either / mpv | S | 0 | `volume-max=200`. Warn about clipping. |
| Hardware decoding | 2.0 | mpv (`hwdec=auto-safe`) / MF automatic | S | -CPU | D3D11VA. Fall back to software automatically. |
| Deinterlacing | 2.2 | mpv | S | low | `deinterlace=auto`. |
| HDR playback | 2.2 | mpv + display HDR | L | med | Windows HDR mode, correct tone-mapping, driver variance. Test matrix needed. |
| Timeline thumbnail previews | 2.1 | mpv second instance or ffmpeg-less seek frames | L | med | Costly to do well; start with sparse keyframe thumbnails generated lazily. |
| Direct time entry | 2.0 | UI | S | 0 | Ctrl+T "mm:ss". |
| Chapters | 2.1 | mpv | S | 0 | `chapter-list`. |
| Multi-monitor playback | 2.2 | Native | M | 0 | Full screen on the window's current monitor (V1 already uses the window's monitor). Explicit monitor choice in 2.2. |
| Auto-hide controls | 2.0 | UI | S | 0 | 2.5 s idle, mouse move shows. |
| Audio-only playback | 2.1 | mpv | S | 0 | Cover-art-less simple view. |
| Audio output device selection | 2.2 | mpv (`audio-device`) | M | 0 | |

**Codec licensing (not legal advice):** H.264/HEVC/AAC/AC-3/DTS are patent-encumbered. Windows includes H.264/AAC licences; HEVC needs Microsoft's paid extension or the GPU vendor decoder. Bundling an FFmpeg-based player means *you* distribute codecs. Mitigation: distribute libmpv only as an **optional user-initiated download**, use an **LGPL** build (not GPL) and dynamically link, ship licence texts and a source offer, state in the UI that patent licensing is the user's responsibility in their jurisdiction. Get proper legal review before publishing widely.

### 4-5. Library, browsing, search and filtering
| Feature | V1 | Rel | Needs | Cx | Perf | Notes |
|---|---|---|---|---|---|---|
| Folder navigation, breadcrumb | Missing | 2.0 | Native | M | 0 | V1 opens one folder; no up/sub-folder navigation. |
| Sidebar folder tree + favorites + bookmarks | Missing | 2.0 | Native | L | low | Lazy-expanded tree; drives list; drives (USB) via `DriveInfo`. |
| Thumbnail grid | Done* | 2.0 | Native | - | - | Exists. |
| List / details view | Missing | 2.1 | Native | M | 0 | Virtualized `ListView`/`GridView`; sortable columns. |
| Adjustable thumbnail size | Partial | 2.0 | Native | S | 0 | Setting exists; add slider in gallery and Ctrl+wheel. |
| Filter by image / video / GIF | Missing | 2.0 | Native | S | 0 | Segmented control. |
| Sort name/date/size/type | Done* | 2.0 | Native | - | - | Core tested. Add resolution (needs metadata read; lazy). |
| Sort by resolution | Missing | 2.1 | Native | M | med | Requires reading headers; cache in the thumbnail DB. |
| Filename search (current folder) | Missing | 2.0 | Native | S | 0 | Instant filter as you type. |
| Recursive search | Missing | 2.1 | Native | M | med | Cancellable background enumerator; never on a UI thread. |
| Extension, size, date, resolution filters; HD/FHD/2K/4K/8K | Missing | 2.1 | Native | M | low | Filter model over cached metadata. |
| Search history | Missing | 2.1 | Native | S | 0 | Local JSON, clearable. |
| Recent files | Done* | 2.0 | Native | - | - | Exists (popup). Add recent folders. |
| Favorites | Missing | 2.0 | Native | S | 0 | Heart toggle + sidebar list. |
| Hidden-file visibility | Partial | 2.0 | Native | S | 0 | Scanner skips hidden/system; add setting. |
| Network folders (UNC) | Partial | 2.0 | Native | S | med | UNC paths work through .NET; need timeouts, lazy thumbnails, no auto-scan on slow shares. |
| Folder refresh / watcher | Partial | 2.0 | Native | M | low | F5 exists; add debounced `FileSystemWatcher`. |
| File grouping (by date/type/folder) | Missing | 2.2 | Native | M | low | Group headers in virtualized rows. |
| Duplicate detection | Missing | 2.1 | Native | L | high | Size grouping then partial hash then full hash (SHA-256 / xxHash via `System.IO.Hashing`); optional visual hash is V3. Cancellable, opt-in, never automatic. |
| Large-folder handling (100k files) | Improve | 2.0 | Native | L | high | Finding 2; stream enumeration, chunked UI updates. |
| Lazy thumbnails | Done* | 2.0 | Native | - | - | Exists; add priority for visible rows. |
| Multi-file selection | Missing | 2.0 | Native | M | 0 | Ctrl/Shift click, rubber-band later. |
| Explorer drag-and-drop (in and out) | Partial | 2.0 | Native | S | 0 | Drop-in exists; drag-out of selected files is missing. |
| Properties, copy path, open containing folder | Partial | 2.0 | Native | S | 0 | "Show in Explorer" exists; add `ShellExecute "properties"` and path copy. |
| Customizable gallery layout | Missing | 2.1 | Native | M | 0 | Tile size, caption on/off, spacing, aspect (square crop *display*, not editing). |

### 6. Tabs and multitasking
| Feature | Rel | Needs | Cx | Perf | Notes |
|---|---|---|---|---|---|
| Tabs with independent folder state; rename, duplicate, close, reopen closed | 2.1 | Native | XL | low | **Requires splitting MainViewModel (finding 1).** Each tab owns a `BrowserSession`; heavy objects (decoded image) only for the active tab. |
| Multiple windows | 2.1 | Native | M | med | Share caches via a process-wide service. Single-process model. |
| Picture-in-picture, always on top, floating resizable video | 2.1 | Native | M | 0 | `Topmost` + a small borderless window hosting the video child. |
| Independent playback sessions | 3 | Native | L | high | Two simultaneous decoders are rarely useful; postpone. |
| Session restoration, recent-tab history | 2.1 | Native | M | 0 | JSON of tabs, folders, positions. |

### 7. Audio and subtitles - covered in section 3 (all require mpv for full scope).

### 8. Input
| Feature | V1 | Rel | Cx | Notes |
|---|---|---|---|---|
| Wheel zoom (photo) / wheel volume (video) | Partial | 2.0 | S | Zoom exists. Video wheel context-dependent. |
| Double-click fullscreen toggle | Partial | 2.0 | S | V1 double-click toggles fit/actual; make configurable. |
| Right-click context menu | Missing | 2.0 | M | See section D. |
| Drag-to-pan | Done* | 2.0 | - | |
| Side-button navigation | Missing | 2.0 | S | |
| Touchpad pinch | Partial | 2.1 | S | Precision touchpads send Ctrl+wheel, which wheel-zoom already handles; test only. |
| Touchpad swipe | Not recommended | - | - | Windows does not deliver app-level swipe gestures to WPF. |
| Customizable shortcuts; configurable mouse actions | Missing | 2.1 | L | Command registry (finding 8). |
| Keyboard-only operation | Partial | 2.0 | M | Focus order, visible focus rectangles, no mouse-only functions. |

### 9. Windows integration
| Feature | V1 | Rel | Needs | Cx | Notes |
|---|---|---|---|---|---|
| "Open with" registration | Done* (installer) | 2.0 | Native | - | Unverified. |
| Default photo/video handler | Partial | 2.0 | Native | M | Apps cannot force defaults since Windows 8; register capabilities (`RegisteredApplications`) and deep-link to Settings. |
| Explorer context menu ("Open with Feather View", "Open folder") | Missing | 2.1 | Native (registry verbs) | S | Classic verbs; the Windows 11 modern menu needs a packaged COM extension - skip. |
| Multiple-file opening | Partial | 2.0 | Native | S | Currently first file only; open all as a virtual folder set. |
| Clipboard (copy image, copy path) | Done* / Partial | 2.0 | Native | S | |
| DPI scaling | Done* | 2.0 | Native | S | Manifest PerMonitorV2; verify per-monitor changes. |
| Theme detection | Done* | 2.0 | Native | - | |
| High-contrast accessibility | Missing | 2.1 | Native | M | Use `SystemColors` when `SystemParameters.HighContrast`. |
| Taskbar progress, Jump Lists | Missing | 2.1 | Native | S | WPF `TaskbarItemInfo`, `JumpList`. |
| Command-line opening | Done* | 2.0 | Native | - | |
| Multiple-instance management | Missing | 2.0 | Native | M | Named mutex + pipe; setting "open in new tab/window". |
| Portable installation | Partial | 2.0 | Native | S | ZIP exists; settings must follow `portable.flag`. |
| Shell preview handler | Not recommended | - | - | - | Microsoft does not support managed in-process shell extensions; would require C++. |
| Windows Share | 3 | WinRT interop | M | |
| Wallpaper, printing | 2.1 | Native | see section 2 | |

### 10. GUI and customization
| Feature | V1 | Rel | Cx |
|---|---|---|---|
| Fluent-inspired UI, dark/light/system | Done* | 2.0 | - |
| OLED black theme; custom accent colour | Missing | 2.0 | S |
| Compact / minimal / borderless modes | Missing | 2.1 | M |
| Customizable toolbar | Missing | 2.1 | L |
| Interface scaling | Missing | 2.1 | M (`LayoutTransform` plus font scale) |
| Hide/show toolbar, sidebar, filmstrip, status bar | Missing | 2.0 | S |
| File-information overlays | Missing | 2.1 | S |
| Resizable panels (splitters) | Missing | 2.0 | S |
| Consistent icons and tooltips | Done* | 2.0 | - (replace hand-drawn icons by genuine Lucide paths: ISC licence) |
| Keyboard accessibility, screen reader | Missing | 2.0 | M (finding 9) |

### 11-12. Information panel and file management
| Feature | V1 | Rel | Needs | Cx | Notes |
|---|---|---|---|---|---|
| Collapsible information panel with photo fields (filename, extension, size, dimensions, aspect, colour depth, DPI, camera, lens, ISO, aperture, shutter, focal length, date, GPS, colour profile) | Partial | 2.0 | Native | M | V1 shows a flat list; add sections, lens, DPI, depth, profile. |
| Video fields (resolution, fps, codecs, bitrate, duration, aspect, colour space, HDR, channels, sample rate, embedded subtitles, container) | Missing | 2.0 | mpv (MF lacks most) | M | |
| Rename | Missing | 2.0 | Native | S | Inline, validates characters/collisions. |
| Delete with confirmation, Recycle Bin | Done* | 2.0 | Native | - | |
| Copy / move to folder; multi-select | Missing | 2.0 | Native | M | Shell `IFileOperation` or `FileSystem.CopyFile` with progress and undo hints. |
| Open with another application | Missing | 2.1 | Native | S | |
| Batch rename / batch delete | Missing | 2.1 | Native | M | Pattern + preview list; always previews before applying. |
| Copy decoded image to clipboard | Done* | 2.0 | Native | - | |

### 13-14. Performance, reliability, privacy
| Feature | V1 | Rel | Cx | Notes |
|---|---|---|---|---|
| Thumbnail cache, configurable size | Partial | 2.0 | M | Memory LRU exists; add disk cache with size cap and automatic cleanup. |
| Memory-aware caching, low-memory detection | Missing | 2.0 | M | `GC.GetGCMemoryInfo` + `MemoryFailPoint`-style guard; shrink caches on pressure. |
| Async decoding, cancellation, duplicate-decode prevention | Partial | 2.0 | M | Add in-flight request de-duplication keyed by path. |
| Startup optimization | Partial | 2.0 | M | ReadyToRun on; defer update check (done), lazy-create windows, avoid reflection. |
| GPU acceleration | Done* (WPF default) | 2.0 | - | Video via mpv `gpu` renderer. |
| Crash recovery, corrupted-file handling | Partial | 2.0 | M | Messages exist; add last-session recovery file. |
| Local diagnostic logs; settings backup/restore | Partial | 2.0 | S | Log exists; add export/import and "open log folder". |
| No telemetry/ads/accounts; optional update check; no admin | Done* | 2.0 | - | Installer allows per-user install (`PrivilegesRequiredOverridesAllowed`). |

---
## 15. Advanced features - recommendation
| Feature | Recommendation | Reason |
|---|---|---|
| Media library indexing | **V3** | Needs a background service/database; conflicts with "no background services". If done: opt-in, on-demand index only. |
| DLNA / NAS / advanced network | **V3** | UPnP stack, auth, timeouts; mpv can already open some URLs. |
| 360 image/video, VR | **V3** | Needs a 3D renderer (D3D11 or WPF 3D); mpv has no equirectangular viewing UI. Niche. |
| Advanced RAW preview | **V3** (embedded-JPEG preview could be 2.2) | Camera-specific; rely on Microsoft's RAW codec for decode. |
| Advanced HDR / colour management | Basic in 2.2, advanced **V3** | Display-dependent QA burden. |
| Portable device (MTP) | **V3** | Shell-namespace integration, complex. |
| Plugin architecture | **V3**, probably never | Security and support cost; contradicts "simple". |

---
## D. GUI redesign specification
Principles: the media gets the space; chrome auto-hides; one primary action visible per mode; everything else is in the right-click menu or a keyboard shortcut. At most 9 toolbar icons visible in each mode.

**Main layout (all modes)**
```
+------------------------------------------------------------------------------------------+
| [tab][tab][+]                                                                 (window)   |  tab strip (2.1; hidden with 1 tab)
+------------------------------------------------------------------------------------------+
| < > ^  | Pictures > Holiday > 2026          [search..........]  [Grid|List] [Sort v] [Filter] [i] [gear]
+--------+---------------------------------------------------------------------------------+
| Sidebar|                                                                                 |
| Favs   |                         content (gallery / photo / video)                        |
| Recent |                                                                                 |
| Drives |                                                                                 |
| Folders|                                                                                 |
+--------+---------------------------------------------------------------------------------+
| 1 of 248 | IMG_0412.jpg | 4032x3024 | 3.1 MB | 100%                          status bar     |
+------------------------------------------------------------------------------------------+
```
Sidebar (220 px, resizable, collapsible with Ctrl+B), status bar (collapsible), toolbar (collapsible) - settings persisted per mode.

**Gallery:** breadcrumb + search at top; tiles `thumbSize+16` wide with caption; selection = accent border; multi-select with Ctrl/Shift; size slider bottom-right; right-click: Open, Open with, Favorite, Rename, Copy, Move to..., Delete, Copy path, Show in Explorer, Properties.

**Photo viewer:** toolbar shows only: Prev, Next, Zoom (dropdown: Fit, 100%, 200%), Rotate, Slideshow, Info, Full screen, More (...). Filmstrip (bottom, 72 px, toggle F). Info panel right (300 px, collapsible sections: File, Image, Camera, Colour, Location). Overlay bar in full screen appears on mouse move, hides after 2.5 s.

**Video player:** content fills; bottom overlay: seek bar with hover thumbnail and chapter ticks, then [Play/Pause] [Prev][Next] [time / duration (click to type)] ... [Speed] [Subtitles] [Audio] [Volume] [PiP] [Full screen]. Audio/subtitle tracks and aspect/rotate live in the right-click menu and a small "track" flyout. Controls auto-hide after 2.5 s while playing, always visible while paused.

**Settings layout:** left category list (Appearance, General, Photos, Video, Subtitles, Browsing, Performance, Shortcuts, File types, Updates & privacy, About), single scrolling right pane per category; every control has a one-line description; "Reset category" button; export/import at the bottom of General.

**Context menus:** photo: Copy image, Copy path, Rotate left/right, Flip, Set as wallpaper, Print, Open with..., Show in Explorer, Properties, Delete. Video: Play/Pause, Speed >, Audio track >, Subtitles >, Aspect >, Rotate >, Screenshot, A-B loop, Always on top, Open with..., Properties.

**Toolbar arrangement (default):** left navigation (Back, Forward, Up), centre breadcrumb/status, right mode tools. Customizable in 2.1 (checklist of available actions).

**Themes (tokens)**
| Token | Light | Dark (V1) | OLED |
|---|---|---|---|
| Background | #F2F3F6 | #17181C | #000000 |
| Surface | #FFFFFF | #22242A | #0B0B0D |
| Hover / Pressed | #E8EAF0 / #DCDFE8 | #2F323A / #3A3E48 | #1A1B1F / #26272D |
| Border | #D6D9E1 | #34373F | #202226 |
| Text / Subtle | #1B1C20 / #666B78 | #EDEFF4 / #9AA0AD | #F2F3F7 / #8A90A0 |
| Accent (default, user-selectable) | #1F6FEB | #6EA8FE | #6EA8FE |
| Danger | #D1242F | #FF7B7B | #FF7B7B |
Contrast target: WCAG AA (4.5:1) for text; focus rectangle 2 px accent. High-contrast mode falls back to system colours. Typography Segoe UI Variable 13 px body, 11 px captions, 15-16 px headings; spacing scale 4/8/12/16/24.

---
## E. Performance targets (targets, not measurements; V1 CI smoke test will provide the first real numbers once the build is green)
| Metric | Target | Notes |
|---|---|---|
| Startup to window (SSD, warm) | <= 600 ms; cold <= 1.3 s | WPF self-contained + ReadyToRun; defer everything non-essential. |
| Idle RAM, one photo | 70-110 MB | WPF baseline is about 50-70 MB; do not promise less. |
| RAM with gallery | + about 45 MB at the default 600 thumbnails (160 px is about 77 KB each) | Configurable cap. |
| 12 MP JPEG load (fit view) | <= 150 ms on SSD | Fit-size decode; neighbour pre-decode. |
| 100 MP image | opens at fit size < 1.5 s; RAM capped | Tiled upgrade on zoom. |
| Thumbnails | 80-150/s JPEG with 2-4 decoders; visible tiles first | Disk-cache hits < 5 ms each. |
| Video 1080p H.264, hw decode | CPU <= 6 % on a modern quad-core | D3D11VA. Software decode of 4K HEVC will exceed this; document it. |
| Large folder | 100k files: first tiles in < 1 s, memory growth bounded | Streaming enumeration. |
| Installer size | about 60 MB (self-contained .NET 10 + WPF) | Framework-dependent ZIP about 2 MB. |
| libmpv add-on | +30-40 MB download, optional | |
| Idle CPU | 0 % (no timers when idle) | No background services. |
Add CI performance gates (start-up, working set after opening 3 images, thumbnail throughput on a generated 2,000-file folder) that fail the build on regressions beyond 25 %.

---
## F. Development plan
| M | Milestone | Depends on | Key tasks | Tests / completion criteria |
|---|---|---|---|---|
| 0 | **Make V1 real** | - | Fix CI errors; run on a real Windows 10 and 11 machine; fix runtime binding/layout bugs; record baseline start-up/RAM. | CI green; manual smoke checklist passes; baseline numbers recorded. **No V2 work before this.** |
| 1 | Architecture refactor | M0 | Split `MainViewModel` into `BrowserSession`, `PhotoViewer`, `SlideshowController`, `FileActions`, `NavigationState`; command registry; settings schema v2 + migration; abstract image handle for testability; DI-lite composition root. | Existing behaviour unchanged (regression tests); VM unit tests with fakes; no file > 400 lines. |
| 2 | Video engine | M1 | `IVideoEngine`; Media Foundation back-end; libmpv P/Invoke wrapper, `HwndHost`, on-demand verified download; track/property model. | Plays MP4/MKV/AVI/WMV test corpus; engine unload releases handles; mpv absent -> clear message. |
| 3 | Video UI | M2 | Overlay controls, seek/speed/volume, subtitles, audio tracks, A-B, frame step, screenshots, resume, history, info panel. | Per-feature checklist; keyboard-only operable; CPU/RAM targets met on reference clips. |
| 4 | Photo core | M1 | ICC, high-res strategy, remember zoom, filmstrip, shuffle, checkerboard, backgrounds, side buttons, overlay full-screen, TIFF pages. | Golden-image tests (colour, orientation); memory cap test with a 100 MP file. |
| 5 | Browsing/search | M1 | Folder tree, breadcrumbs, favorites, filters, search, watchers, disk thumbnail cache, list view, large-folder model. | 100k-file generated folder benchmarks; watcher stress test; cache cleanup test. |
| 6 | File management | M5 | Multi-select, rename, copy/move, batch rename, properties, drag-out. | Never touches originals without confirmation; undo-safe (Recycle Bin); collision tests. |
| 7 | Tabs, windows, sessions | M1, M5 | `BrowserSession` per tab, single-instance pipe, PiP, session restore. | Memory flat across 20 tabs (inactive tabs release decoded images). |
| 8 | Customization and accessibility | M3-M6 | OLED/accent, compact/minimal, toolbar customization, shortcut remapping, AutomationProperties, high-contrast. | Narrator/Accessibility Insights pass; contrast checks automated. |
| 9 | Windows integration and release | all | Jump list, context-menu verbs, wallpaper, printing, portable flag, code signing, installer, signed releases. | Clean install/upgrade/uninstall on VMs; SmartScreen reputation plan. |
**Regression net:** Core unit tests (existing 18 + new), ViewModel tests against fakes (Linux-capable), golden-image rendering tests on Windows runners, scripted UI smoke (launch, open sample, navigate, close), perf gates, manual matrix (Win10 22H2, Win11, 100/150/200 % scaling, two monitors, HDD vs SSD, corrupt-file corpus, 20k and 100k folders).

---
## G. The 20 most valuable V2 features (V1 -> V2 impact vs. weight)
1. Reliable video playback with a clear engine strategy (frame-step, speed, resume, loop).
2. Subtitles (SRT/ASS) and audio-track selection.
3. Folder browsing: sidebar, breadcrumb, favorites, drives, recent folders.
4. Search and filters (name, type, size, date, resolution).
5. Persistent thumbnail cache with automatic cleanup.
6. Large-folder and high-resolution-image handling (bounded memory).
7. Correct colour (ICC) and orientation.
8. Thumbnail filmstrip.
9. Remember zoom/position and per-folder view settings.
10. Multi-select and safe file operations (rename, copy, move, delete to Recycle Bin).
11. Collapsible information panel for photos and videos.
12. Customizable keyboard shortcuts and mouse actions (incl. side buttons).
13. Tabs with session restore (2.1).
14. Single-instance handling and multi-file opening.
15. Slideshow with shuffle and fade.
16. A-B repeat, direct time entry, chapters.
17. Duplicate finder (opt-in, cancellable).
18. Set wallpaper and printing.
19. Themes: OLED, custom accent, compact/minimal/borderless, full accessibility.
20. Crash recovery, diagnostics, settings backup/restore, portable mode.

## Explicitly not included
Editing of any kind, conversion, cloud/social/streaming, built-in browser, music library, shell preview handler (unsupported for .NET), touchpad swipe (no API).

## Decisions needed from you before any code change
1. **Approve fixing the CI build now** (one-line `using System.IO;` plus whatever else the next run shows) - it is a V1 bug, but I did not touch the source because you asked for analysis only.
2. **Video engine:** (a) libmpv optional download plus Media Foundation fallback (recommended), (b) libmpv only, (c) Media Foundation only (much smaller, but loses frame-accurate seek, subtitles/ASS, audio-track selection, A-B, chapters, MKV).
3. **Code-signing certificate** budget (affects SmartScreen and update trust).
4. **Legal review** of redistributing/downloading libmpv/FFmpeg in your jurisdictions.
5. Confirm V2.0 scope = milestones 0-6 + selected parts of 8.
