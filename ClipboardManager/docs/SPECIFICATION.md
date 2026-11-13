> **Development brief for Claude Code — 13 November 2026**
>
> Source: the assistant response in [Windows Clipboard Features](https://chatgpt.com/c/6af689da-1094-83ec-8e42-ce7f5ae2cbd4). All 16 feature categories, feature rows, priorities, architectural guidance, performance targets and roadmap are preserved below. Additional implementation guidance is clearly separated at the end. The source's feature-count discrepancy is corrected; overlapping requirements remain included.

# Comprehensive Feature List for a Modern Windows Clipboard Manager

**Platform:** Windows 10 and Windows 11
**Architecture:** Offline-first, local-only desktop application
**Primary objectives:** Speed, productivity, reliability, privacy, security, and minimal resource consumption.

A modern clipboard manager should extend the Windows clipboard into a powerful productivity system without becoming a resource-intensive application.

The following specification covers **216 feature rows across 16 functional categories**, classified into three development priorities.

**Priority classification** Development roadmap

(P0)

**Essential**

Required for a reliable initial release

(P1)

**Recommended**

Strong productivity improvements

(P2)

**Advanced**

Power-user and specialized capabilities

## 1. Clipboard Capture and History Management

The foundation of the application. Every clipboard event must be captured accurately without interfering with normal Windows copy-and-paste behavior.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Automatic clipboard monitoring | Capture new clipboard content in the background. |
| P0 | Unlimited history option | Allow unrestricted history subject to disk limits. |
| P0 | Text capture | Store plain text from any compatible application. |
| P0 | Rich-text capture | Preserve formatted text, including RTF and HTML. |
| P0 | Image capture | Capture screenshots and copied bitmap images. |
| P0 | File and folder capture | Preserve copied file paths and file-drop references. |
| P0 | One-click history paste | Reuse previous clipboard entries instantly. |
| P0 | Duplicate detection | Prevent unnecessary repeated entries. |
| P0 | Chronological history | Display recent entries first. |
| P0 | Manual item deletion | Remove individual clipboard records. |
| P0 | Clear entire history | Delete all stored clipboard data. |
| P0 | Persistent history | Restore stored entries after restarting the application. |
| P1 | Configurable history limits | Restrict item count, age, and storage usage. |
| P1 | Intelligent duplicate handling | Update the recency of an existing duplicate instead of adding another. |
| P1 | Source application identification | Display the program from which content was copied, when identifiable. |
| P2 | Clipboard format inspector | Inspect available native clipboard formats for troubleshooting. |

**Important implementation detail:** For file and folder entries, the application should distinguish between storing a reference to a file and storing its actual contents. Normal clipboard history should not silently duplicate large files.

## 2. Search, Filtering, and Organization

A clipboard manager becomes substantially more useful when users can retrieve information copied days or weeks earlier.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Instant search | Find text as the user types. |
| P0 | Case-insensitive search | Match text regardless of capitalization. |
| P0 | Search highlighting | Highlight matching words in results. |
| P0 | Pinned items | Protect important entries from automatic cleanup. |
| P0 | Favorites | Mark frequently reused content. |
| P1 | Tags | Organize entries using custom labels. |
| P1 | Categories | Group URLs, code, images, files, and text. |
| P1 | Date filters | Search entries copied within a date range. |
| P1 | Application filters | Find entries copied from a particular program. |
| P1 | Content-type filters | Show only images, text, links, or files. |
| P1 | Multi-keyword search | Search using multiple terms. |
| P1 | Exact phrase search | Locate an exact sequence of words. |
| P1 | Saved searches | Reuse frequently applied search criteria. |
| P2 | Regular expression search | Find complex text patterns. |
| P2 | Fuzzy search | Find approximate matches despite spelling errors. |
| P2 | Duplicate grouping | Group similar or identical entries together. |
| P2 | Smart collections | Automatically organize entries using configurable local rules. |

## 3. Copy, Paste, and Productivity Operations

These features minimize repetitive copying, switching windows, and manually editing content.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Paste selected item | Paste a historical entry into the active application. |
| P0 | Copy without pasting | Restore an item to the Windows clipboard without inserting it. |
| P0 | Paste as plain text | Remove unwanted fonts, colors, and formatting. |
| P0 | Keyboard-first operation | Navigate and paste without touching the mouse. |
| P1 | Paste multiple items | Combine selected entries into one paste operation. |
| P1 | Sequential paste | Paste queued items in a predefined order. |
| P1 | Paste queue | Prepare multiple entries for repetitive data entry. |
| P1 | Text case conversion | Convert text to uppercase, lowercase, title case, or sentence case. |
| P1 | Trim whitespace | Remove leading and trailing spaces. |
| P1 | Remove extra blank lines | Clean text copied from documents or websites. |
| P1 | Find and replace | Modify copied text before pasting. |
| P1 | Text snippets | Save reusable phrases, templates, and responses. |
| P1 | Paste without line breaks | Convert multiline text into a single line. |
| P2 | Paste templates | Insert predefined text with locally generated date and time values. |
| P2 | Clipboard macros | Execute repeatable local transformations and paste sequences. |
| P2 | Multi-item separator rules | Join entries using commas, tabs, newlines, or custom delimiters. |
| P2 | Text transformation pipelines | Chain multiple operations, such as trim, replace, and case conversion. |

## 4. User Interface and Usability

The interface should appear immediately, remain unobtrusive, and require minimal interaction.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Compact popup window | Display history without occupying excessive screen space. |
| P0 | System tray integration | Run quietly in the notification area. |
| P0 | Global keyboard shortcut | Open clipboard history from any supported desktop application. |
| P0 | Clear item previews | Display enough content to identify each entry. |
| P0 | Responsive interface | Remain fluid with large clipboard histories. |
| P0 | Dark and light themes | Match the user's preferred appearance. |
| P0 | Context menus | Expose relevant operations through right-click. |
| P0 | Tooltips | Explain controls without cluttering the interface. |
| P1 | Resizable window | Adjust the history panel to different workflows. |
| P1 | Adjustable preview length | Control how much content each entry displays. |
| P1 | List and grid layouts | Choose a compact text view or visual thumbnail view. |
| P1 | Adjustable font size | Improve readability. |
| P1 | Always-on-top mode | Keep the clipboard window visible when needed. |
| P1 | Window position memory | Reopen the panel at its previous position. |
| P1 | Multi-monitor positioning | Open the panel near the active window or cursor. |
| P1 | Customizable toolbar | Show frequently used operations. |
| P2 | Detachable preview pane | Inspect long entries without leaving the history view. |
| P2 | Command palette | Execute clipboard actions through a searchable command interface. |
| P2 | Workspace layouts | Save different UI arrangements for different tasks. |

## 5. Keyboard Shortcuts and Navigation

Keyboard-driven interaction is essential for a clipboard manager intended for frequent daily use.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Customizable activation shortcut | Configure a global shortcut such as `Ctrl+Shift+V`. |
| P0 | Arrow-key navigation | Browse history without a mouse. |
| P0 | Enter-to-paste | Paste the highlighted item immediately. |
| P0 | Escape-to-close | Dismiss the popup quickly. |
| P1 | Numbered quick-paste | Paste recent entries using configurable number shortcuts. |
| P1 | Favorite-item shortcuts | Access commonly used snippets directly. |
| P1 | Shortcut conflict detection | Warn when a hotkey is unavailable. |
| P1 | Keyboard shortcut reference | Display available commands in one place. |
| P2 | Context-sensitive shortcuts | Apply different actions depending on selected content. |
| P2 | Custom command bindings | Assign shortcuts to transformations and macros. |

## 6. Image and Screenshot Management

Clipboard images should be managed efficiently, without forcing the user to open a separate image editor.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Screenshot history | Retain screenshots captured through Windows tools. |
| P0 | Image thumbnails | Identify copied images visually. |
| P0 | Original image preservation | Avoid degrading captured image quality. |
| P1 | Full-size image preview | Inspect images before reusing them. |
| P1 | Save image to file | Export images as PNG, JPEG, or other supported formats. |
| P1 | Image dimensions | Display width, height, and format information. |
| P1 | Image resizing | Resize clipboard images before copying or saving. |
| P1 | Image compression | Reduce storage consumption. |
| P1 | Format conversion | Convert between supported image formats. |
| P2 | Image cropping | Remove unwanted areas from captured images. |
| P2 | Image annotation | Add arrows, shapes, and labels locally. |
| P2 | Offline OCR | Extract text from screenshots using an installed local OCR engine. |
| P2 | Color picker | Extract hexadecimal and RGB values from clipboard images. |

## 7. Security and Privacy

Security is particularly important because clipboard data may contain passwords, personal information, source code, and confidential documents.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Fully offline operation | No clipboard data transmitted over a network. |
| P0 | Local-only storage | Keep history on the user's device. |
| P0 | Password-manager exclusion | Avoid retaining sensitive entries from recognized password managers. |
| P0 | Pause clipboard monitoring | Temporarily disable collection. |
| P0 | Private mode | Suspend history capture during sensitive activities. |
| P0 | Secure local data permissions | Restrict database access using Windows user permissions. |
| P0 | No telemetry | Avoid transmitting usage analytics or clipboard metadata. |
| P1 | Application exclusion list | Prevent capture from selected applications. |
| P1 | Automatic history expiration | Delete old records after a configured interval. |
| P1 | Sensitive-content detection | Detect likely passwords, API keys, payment card numbers, and identity numbers. |
| P1 | Automatic sensitive-entry removal | Delete detected sensitive records after a configurable period. |
| P1 | Local database encryption | Encrypt stored history using a locally managed key. |
| P1 | Lockable clipboard history | Require Windows authentication before viewing protected entries. |
| P1 | Privacy notifications | Inform users when sensitive-content protection is activated. |
| P2 | Protected collections | Store selected entries in separately protected local storage. |
| P2 | Configurable redaction | Mask emails, phone numbers, tokens, and account numbers in previews. |
| P2 | Local security audit | Identify unprotected settings and potentially sensitive retained data. |

**Security design note:** A clipboard manager cannot guarantee that all secrets will be recognized. The safest design combines application exclusions, clipboard-format awareness, private mode, and optional sensitive-content detection.

## 8. Performance and Resource Optimization

A clipboard manager should remain lightweight even when running continuously for several days.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Event-driven monitoring | Use Windows clipboard notifications rather than continuous polling. |
| P0 | Low idle CPU usage | Avoid unnecessary background processing. |
| P0 | Low idle memory usage | Keep resident memory consumption small. |
| P0 | Fast popup activation | Display history with minimal perceived delay. |
| P0 | Asynchronous database operations | Prevent storage operations from freezing the interface. |
| P0 | Lazy loading | Load older entries only when required. |
| P0 | Database indexing | Keep searches fast as history grows. |
| P1 | Thumbnail caching | Avoid repeatedly decoding large images. |
| P1 | Configurable cache size | Balance responsiveness and memory consumption. |
| P1 | Automatic database maintenance | Perform safe local cleanup and optimization. |
| P1 | Storage usage dashboard | Show database, image, and cache sizes. |
| P1 | Large-content safeguards | Avoid capturing extremely large entries unintentionally. |
| P2 | Performance diagnostics | Display capture latency, search latency, and resource usage. |
| P2 | Adaptive cache management | Adjust cache utilization according to memory pressure. |

## 9. Reliability, Recovery, and Data Integrity

Clipboard data must survive ordinary restarts, application failures, and unexpected shutdowns.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Crash-resistant storage | Use transactional writes to reduce corruption risk. |
| P0 | Graceful error handling | Recover from clipboard contention and temporary access failures. |
| P0 | Safe shutdown | Close database connections and complete pending writes. |
| P0 | Startup recovery | Restore valid history after an interrupted session. |
| P0 | Clipboard retry mechanism | Retry transient clipboard-access failures with bounded delays. |
| P1 | Local backup | Create backups in user-selected local folders. |
| P1 | Local restore | Recover history from a previously created local backup. |
| P1 | Export history | Export selected entries or complete history to local files. |
| P1 | Import history | Restore compatible locally exported records. |
| P1 | Database integrity checks | Detect structural problems in stored data. |
| P1 | Corruption recovery | Recover readable records from damaged databases when possible. |
| P2 | Versioned local backups | Retain several previous backup generations. |
| P2 | Diagnostic log viewer | Inspect local application errors without uploading logs. |
| P2 | Safe migration rollback | Recover from failed database schema upgrades. |

## 10. Windows Integration

The application should behave like a native Windows utility rather than an isolated desktop program.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Windows 10/11 support | Operate on supported Windows desktop environments. |
| P0 | Native clipboard API integration | Use supported Win32 clipboard mechanisms. |
| P0 | System tray menu | Access core operations from the notification area. |
| P0 | Startup with Windows | Optionally launch automatically after sign-in. |
| P0 | Native copy/paste compatibility | Preserve expected behavior in ordinary Windows applications. |
| P1 | Windows 11 visual integration | Support modern window styling and system appearance settings. |
| P1 | High-DPI support | Render correctly on 125%, 150%, 200%, and other display scales. |
| P1 | Multi-monitor support | Handle different resolutions and DPI settings. |
| P1 | Explorer integration | Provide optional file-path and file-related clipboard commands. |
| P1 | Windows notification integration | Display useful local notifications when appropriate. |
| P1 | Portable mode | Run without installation where permissions permit. |
| P1 | Standard installer/uninstaller | Provide predictable installation and complete removal options. |
| P2 | Windows context-menu extensions | Expose selected clipboard operations in supported shell contexts. |
| P2 | Virtual desktop awareness | Open the popup on the active desktop. |
| P2 | Command-line interface | Permit local scripts and developer tools to access supported operations. |

## 11. Text, Code, and Developer Tools

These features are particularly useful for programmers, engineers, technical writers, and researchers.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P1 | Code-friendly previews | Preserve indentation and line breaks. |
| P1 | Syntax highlighting | Make common programming languages easier to inspect. |
| P1 | JSON formatting | Pretty-print and validate JSON locally. |
| P1 | XML formatting | Indent and validate XML. |
| P1 | URL recognition | Identify copied web addresses without opening them automatically. |
| P1 | Markdown preview | Display locally rendered Markdown. |
| P1 | Line numbering | Improve navigation through long code snippets. |
| P2 | JSON minification | Compress formatted JSON into compact text. |
| P2 | Text diff viewer | Compare two clipboard entries line by line or word by word. |
| P2 | Regex tester | Test expressions against clipboard text. |
| P2 | Encoding utilities | Convert between UTF-8, Base64, and supported encodings. |
| P2 | Hash generation | Calculate SHA-256 and other supported hashes locally. |
| P2 | SQL formatting | Format SQL statements for readability. |
| P2 | Path conversion | Convert Windows paths into common slash conventions. |
| P2 | Unicode inspector | Inspect code points and special characters. |

## 12. Automation and Intelligent Offline Processing

Automation can improve productivity without requiring cloud services or external AI APIs.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P1 | Automatic text cleanup | Normalize whitespace and formatting when requested. |
| P1 | Content-aware actions | Suggest relevant operations based on entry type. |
| P1 | Rule-based classification | Categorize URLs, code, email addresses, and other recognized patterns. |
| P1 | Automatic tagging | Apply locally defined tags to matching content. |
| P2 | Rule engine | Execute user-defined conditions and transformations. |
| P2 | Offline text summarization | Summarize content using an optional local model. |
| P2 | Offline language detection | Identify the language of copied text locally. |
| P2 | Offline translation | Translate supported languages using installed local models. |
| P2 | Local semantic search | Find conceptually related entries using an optional local embedding model. |
| P2 | User-defined scripts | Run explicitly approved local automation scripts with appropriate restrictions. |

Advanced AI features should be optional modules. They should never force users to install large models or increase baseline memory consumption.

## 13. Customization and Personalization

Users should be able to tailor the application without making its settings unnecessarily complex.

| Priority | Feature | Purpose and benefit |
|--------|--------|--------|
| P0 | Theme selection | Choose light, dark, or system appearance. |
| P0 | Configurable hotkeys | Adapt activation and navigation shortcuts. |
| P1 | Accent color selection | Customize interface highlights. |
| P1 | Adjustable spacing | Choose compact or comfortable layouts. |
| P1 | Configurable startup behavior | Control whether the app launches minimized or displays a window. |
| P1 | Configurable history retention | Set age, item-count, and storage limits. |
| P1 | Configurable paste behavior | Choose whether selecting an item pastes or only copies it. |
| P1 | Settings import/export | Transfer application preferences using local files. |
| P2 | Custom CSS or theme definitions | Support advanced interface customization with appropriate validation. |
| P2 | Customizable entry actions | Configure which commands appear for each content type. |

## 14. Accessibility and Inclusive Design

Accessibility should be part of the initial architecture rather than a late-stage addition.

| Priority | Feature | Purpose and benefit |
|---|---|---|
| P0 | Full keyboard navigation | Operate core functions without a mouse. |
| P0 | Visible keyboard focus | Clearly indicate the currently selected control. |
| P0 | Screen-reader support | Expose accessible names, roles, and values through Windows accessibility APIs. |
| P0 | Sufficient contrast | Ensure readable text and controls. |
| P1 | High-contrast theme | Support Windows contrast themes. |
| P1 | UI scaling | Support Windows text and display scaling. |
| P1 | Reduced-motion support | Minimize animations when requested by the operating system. |
| P1 | Accessible status messages | Announce important actions and errors appropriately. |
| P2 | Custom accessibility profiles | Save combinations of font, spacing, contrast, and motion preferences. |

## 15. Maintenance, Diagnostics, and Distribution

A dependable desktop utility must remain maintainable and easy to troubleshoot.

| Priority | Feature | Purpose and benefit |
|---|---|---|
| P0 | Clean installation | Avoid unnecessary dependencies and background services. |
| P0 | Clean uninstallation | Remove application files while clearly offering a choice about retained user data. |
| P0 | Application version display | Show the installed version and build information. |
| P1 | Offline installer | Install without an internet connection. |
| P1 | Portable executable distribution | Support use from a USB drive or local folder. |
| P1 | Local diagnostic logging | Record operational failures without transmitting information. |
| P1 | Configurable logging | Disable or minimize diagnostic records. |
| P1 | Offline update installation | Install signed update packages provided locally. |
| P2 | Database repair utility | Diagnose and repair supported storage problems. |
| P2 | Self-test mode | Validate clipboard monitoring, storage, and core application components. |

## 16. Specialized Advanced Capabilities

These features differentiate a mature clipboard manager from a conventional history utility.

| Priority | Feature | Purpose and benefit |
|---|---|---|
| P2 | Multiple clipboard slots | Maintain separate user-controlled clipboard registers. |
| P2 | Clipboard collections | Create named libraries of reusable entries. |
| P2 | Drag-and-drop paste | Drag supported clipboard content into compatible applications. |
| P2 | Table-aware clipboard handling | Preserve rows and columns from spreadsheet content. |
| P2 | CSV/TSV preview | Display structured tabular data before pasting. |
| P2 | Clipboard timeline | Browse history visually by time. |
| P2 | Temporary clipboard sessions | Keep task-specific histories separate and optionally discard them afterward. |
| P2 | Local plugin architecture | Extend functionality using explicitly installed local modules. |
| P2 | Clipboard event debugger | Diagnose clipboard-format and ownership problems. |
| P2 | Per-application paste profiles | Apply different formatting and paste rules to selected applications. |

## Recommended performance targets

These are proposed engineering targets, not guaranteed results. They should be validated on the intended hardware and framework.

| Metric | Target | Scope |
|---|---|---|
| Idle CPU usage | < 0.5% | Typical modern PC |
| Idle memory | < 60 MB | Core application, excluding optional modules |
| Popup response | < 100 ms | Warm activation, typical history |
| Search response | < 100 ms | Typical indexed text queries |
| History scale | 100,000+ entries | Without loading all entries into memory |
| Network dependency | Zero | All runtime functionality offline |

## Recommended technical architecture

For a Windows-only clipboard manager, prioritize native Windows integration, predictable resource consumption, and reliable clipboard-format handling.

| Component | Recommended approach |
|---|---|
| Application framework | C++/Win32 for maximum control and efficiency; C#/.NET with WPF as a faster-development alternative |
| Clipboard monitoring | AddClipboardFormatListener and WM_CLIPBOARDUPDATE |
| Clipboard access | Win32 clipboard APIs, including delayed-rendering awareness |
| Storage | SQLite with WAL mode and appropriate indexes |
| Search | SQLite FTS5 for text content |
| Image storage | Local image files or blobs, with cached thumbnails |
| Security | Windows DPAPI where appropriate, ACLs, and optional authenticated database encryption |
| Background operation | Per-user tray application; avoid a privileged Windows service |
| Communication | Internal event queue and asynchronous workers |
| Packaging | MSIX or a conventional signed Windows installer |
| Testing | Automated clipboard-format, stress, crash-recovery, and security tests |

Critical engineering considerations:

- Clipboard content can expose multiple formats simultaneously. Capture a coherent set of supported formats rather than assuming a single text representation.
- Applications may delay rendering clipboard data. The capture pipeline must account for this without hanging the UI.
- Pasting into another application is not universally reliable through simulated keystrokes, particularly across elevation boundaries and protected desktop contexts.
- Clipboard history should not automatically execute scripts, open copied URLs, or render untrusted HTML with active content.
- Local backups and exports must remain optional, user-controlled, and fully offline.

## Development roadmap

### Phase 1 — Reliable core (P0)

Build native clipboard capture, persistent history, search, keyboard navigation, paste, tray integration, essential privacy controls, and recovery mechanisms.

**Exit criterion:** Reliable everyday operation with no network access.

### Phase 2 — Productivity and polish (P1)

Add snippets, tagging, advanced filtering, image tools, local backups, privacy rules, performance optimization, and extensive customization.

**Exit criterion:** A polished utility suitable for daily professional use.

### Phase 3 — Advanced capabilities (P2)

Add automation rules, OCR, semantic search, optional local AI, advanced transformations, diagnostics, and local extensibility.

**Exit criterion:** Advanced capabilities remain modular and do not compromise core speed or privacy.

## Features that should be deliberately excluded

The application should not implement cloud history, cloud synchronization, remote backup, account-based synchronization, remote clipboard sharing, online OCR, external AI processing, mandatory user accounts, advertising SDKs, or telemetry that sends clipboard data outside the device.

Even optional integrations must not silently transmit clipboard contents. Offline operation should be an architectural guarantee, not merely a settings toggle.

## Final engineering recommendation

The most important differentiator is the combination of instant access, accurate capture, reliable paste, searchable local history, strong privacy controls, and negligible idle overhead.

Build the P0 features first, measure them against the performance targets, then introduce P1 and P2 functionality through independently testable modules. This prevents an otherwise useful clipboard manager from becoming a bloated background application.

---

# Claude Code implementation brief

The preceding sections preserve the substantive source specification. This section adds actionable implementation requirements and verification criteria. The source introduction stated “150 features”; the tables actually contain **216 feature rows**, including overlapping requirements. Preserve every row; do not interpret the original count as a scope limit.

## Mission and scope

Implement a real, fully offline, lightweight Windows desktop clipboard manager from this brief. Deliver working source code, a reproducible build, runnable Windows binaries, packaging instructions, and validation evidence. Do not stop at a mockup, design proposal, or scaffold.

All P0 requirements define the initial release. P1 and P2 remain required roadmap scope, delivered in subsequent phases. Do not mark a phase complete until every feature at that priority is implemented and verified or explicitly recorded as blocked. Treat repeated features across categories as shared requirements rather than duplicate implementations.

## Non-negotiable constraints

1. All clipboard content, metadata, search indexes, caches, logs, backups, exports, OCR, and model inference must remain local.
2. The application and its modules must operate without internet access. No accounts, activation servers, telemetry, remote crash reporting, automatic update checks, remote fonts, CDN assets, remote link previews, or background model downloads.
3. Keep app-managed data outside cloud-synced folders by default. Use a per-user local application-data directory. Backups and exports must target local destinations; identify and reject network destinations and known cloud-sync roots where detectable, and explain that external sync software remains outside the app's control.
4. Build-time dependency acquisition is separate from application runtime. Pin dependencies and provide a documented offline build procedure using pre-staged packages. Offline installation must include the required runtime or a self-contained build.
5. Run as the normal signed-in user, without a privileged service or routine administrator requirement. Do not bypass Windows integrity boundaries.
6. Optional OCR, AI, scripting, and plugins must be disabled and unloaded by default. Import models and modules from local files only. Network-free behavior applies to extensions as well as the core.
7. Preserve original supported content. Transformations and lossy image compression produce derived versions; they must not silently replace originals.
8. Do not change Windows clipboard history or sync settings without an explicit user action. Explain that other applications and Windows itself may retain copied content independently.

## Implementation workflow

1. Inspect the repository, existing code, build tools, and applicable project instructions. If an application already exists, map it against this specification before replacing components.
2. Create a requirement matrix with stable IDs for all 216 feature rows (for example, CAP-001, SEARCH-001). Record priority, module, phase, implementation status, verification, and limitations.
3. Choose C++/Win32 as the default for a new implementation. C#/.NET WPF is acceptable when existing code or delivery needs justify it. Record the tradeoff and measure the same performance targets. Do not introduce a browser runtime merely for UI convenience.
4. Implement a vertical slice: capture Unicode text, persist it transactionally, display a virtualized history, search it, copy it, and paste it into the remembered target.
5. Complete and verify all remaining P0 requirements before adding P1 breadth. Continue through P1 and P2 in independently testable increments.
6. Maintain a local progress document and known-limitations list. If a session ends before completion, provide an exact resume point and remaining requirement IDs.
7. Never claim a feature works because its button exists. Distinguish implementation, automated verification, manual verification, and untested behavior.

## Module boundaries

| Module | Responsibilities |
|---|---|
| Windows host | Single per-user instance, tray, lifecycle, hotkeys, startup registration, session notifications |
| Capture service | Clipboard notifications, format enumeration, bounded retries, coherent snapshots, ownership/source hints |
| Privacy policy | Exclusions, sensitive-format handling, pause/private state, retention and redaction decisions |
| Repository | SQLite transactions, payload storage, indexes, migrations, cleanup, integrity and recovery |
| Search service | Indexed queries, filtering, result pagination, cancellation, highlighting |
| Paste service | Target tracking, format restoration, copy-only fallback, queue and paste profiles |
| UI | Virtualized popup, settings, previews, accessible controls and status |
| Transformation service | Pure local text and image operations, preserving originals |
| Backup service | Consistent snapshots, versioned manifests, validation, import/export, rollback |
| Optional workers | OCR, AI, scripts, plugins with explicit loading and isolation |
| Diagnostics | Content-free local errors, bounded logs, self-tests and benchmarks |

Keep capture and database work off the UI thread. Use bounded queues and explicit overload reporting; do not allow unlimited memory growth. Clipboard API calls that may block on another application's delayed rendering need an isolation strategy and watchdog; cancelling a managed task alone does not stop a blocked native call.

## Clipboard correctness

- Handle Unicode text, RTF, HTML clipboard format, bitmap/DIB variants, and file-drop references. Store each entry as a set of supported representations from the same clipboard state.
- Check clipboard sequence changes around acquisition. If the clipboard changes mid-read, discard the inconsistent snapshot and retry within a bounded budget.
- Release handles and close the clipboard on every success and failure path. Handle transient access contention with backoff and a maximum attempt/time budget.
- Suppress self-generated recapture using an ownership marker and sequence bookkeeping without suppressing genuine later copies of the same content.
- Deduplicate using a deterministic hash of content and relevant formats. Do not merge items solely because plain text matches when rich content differs.
- Store file/folder references, not file contents. Never remove or modify referenced files during history deletion. Handle missing or renamed references clearly.
- Do not replay stale file “cut” semantics that could unexpectedly move files. Preserve references and use safe copy behavior unless an explicit supported operation requests otherwise.
- Treat source application identification as best-effort; clipboard owner and foreground window are not always the true originating application.
- Respect recognized sensitive/history-exclusion clipboard hints. Apply privacy decisions before indexing, thumbnail creation, logging, or persistence wherever possible.
- Define a supported-format allowlist. Report unsupported formats safely; never deserialize or execute arbitrary clipboard payloads.

## Paste behavior

Remember the intended foreground target before opening the popup. On paste, restore the selected supported formats, close or hide the popup, attempt to return focus, and issue the supported paste action only when the target is valid.

Provide copy-only fallback when focus restoration, elevation, protected desktops, or application behavior prevent paste. Do not claim successful insertion from a successful simulated keystroke alone. Preserve normal Ctrl+C/Ctrl+V and leave Windows-reserved shortcuts available. If the configured hotkey cannot be registered, explain the conflict and let the user choose another.

Define queue advancement explicitly: advance on the user-triggered dispatch, expose retry/previous controls, and explain that the app cannot universally confirm receipt by the destination. Never silently restore older clipboard data on a timer while another application may still be reading it.

## Local data model and consistency

Use a versioned schema covering:

- Entries: ID, timestamps, last-used time, type, byte size, content hash, pin/favorite state, source hints, sensitivity/expiry state, and session/collection membership.
- Formats: entry ID, format identifier, payload or local blob reference, length, encoding and integrity checksum.
- Search documents: searchable text with an explicit relationship to the source entry.
- Tags and entry-tag relations; snippets, saved searches, rules, paste profiles, settings and schema migrations.
- A versioned backup/export manifest recording formats, checksums, schema version and encryption requirements.

Use indexed metadata and FTS5 where appropriate; do not treat full-text word matching as equivalent to arbitrary substring matching. Specify the supported search semantics and test Unicode, punctuation, exact phrases, and mixed scripts.

Coordinate payload files with database transactions using staged writes and recovery cleanup, or store blobs transactionally. Backup through a consistent database snapshot mechanism; copying only the main SQLite file while WAL writes are active is insufficient.

Perform retention in bounded batches. Pinned items survive ordinary automatic cleanup, but explicit deletion and clear-all must still work after a clear user warning. Sensitive expiry overrides pinning by default and must be explained. Unlimited history means no item-count cap, not unlimited disk capacity: enforce free-space protection and show actionable errors.

## Security details

- Use per-user access controls for data and temporary files. Do not put clipboard payloads, tokens, paths containing private content, or search text in routine diagnostic logs.
- Encryption must cover payloads, image files, indexes, previews, temporary files, WAL/journal artifacts and backups, not merely the primary text field. Document any metadata that remains visible.
- DPAPI may protect a local encryption key; it is not itself whole-database encryption. Do not store plaintext searchable duplicates outside an encrypted store.
- Define lock behavior for UI, capture, key lifetime, caches and exports. Relock on session lock by default when history locking is enabled.
- Use supported Windows authentication for unlocking protected history; do not implement a fake authentication dialog or collect the user's Windows password.
- Explain portable-mode encryption and recovery: a user/machine-bound key may prevent opening data elsewhere. Never silently downgrade encryption.
- Clear-all must remove database records, searchable text and derived caches. Explain that ordinary deletion cannot guarantee forensic erasure from SSDs, backups or external copies.
- Sanitize HTML/Markdown previews; disable scripts, external resources, navigation and active content. Disable XML external entity resolution. Bound regex execution and preview/image decoding.
- Imported archives need path traversal protection, size limits, schema checks and checksums. Validate before replacing live data, and keep a rollback snapshot.
- Arbitrary local scripts/plugins are executable code. Use explicit installation and execution consent, process isolation and resource limits; do not describe an unrestricted subprocess as a sandbox. If reliable network restriction is unavailable, do not load arbitrary code while claiming guaranteed offline execution.

## Initial defaults and priority reconciliation

Some later-priority features require minimal safety groundwork in P0:

- Apply finite per-entry byte and decoded-image limits immediately; expose richer size controls in P1.
- Default to a bounded history, with an explicit unlimited option and low-disk safeguards.
- Start with a registered, configurable shortcut such as Ctrl+Shift+V; never silently steal another application's hotkey.
- Do not capture anything while paused/private. Do not recapture the unchanged clipboard automatically when monitoring resumes.
- Keep originals intact; previews and indexes can be regenerated.
- Keep startup registration optional and reversible.
- No content-bearing notifications by default.
- Support theme definitions appropriate to the chosen native framework; the original “CSS or theme definitions” feature does not require embedding a web engine.

Document chosen numeric safety limits and retention defaults in settings and tests.

## Acceptance criteria and verification

Use synthetic clipboard fixtures, never the user's real clipboard history, for testing.

| Area | Required evidence |
|---|---|
| Capture and persistence | Plain Unicode, mixed scripts, multiline text, HTML+text, RTF+text, images and file references round-trip through restart without unintended degradation |
| Contention and delayed rendering | Bounded retries, changing sequence numbers and an unresponsive source do not freeze the UI or leak resources |
| Duplicate handling | Exact duplicates follow the configured recency policy; distinct rich formats remain distinguishable |
| Search and scale | Seed 100,000+ synthetic entries; test paging, filters, matching and cancellation without loading all payloads |
| Paste | Verify copy-only, plain-text and rich paste in representative Windows apps; record elevated-target and protected-context limitations |
| Privacy | Excluded and marked-sensitive copies leave no payloads, indexes, thumbnails or content-bearing logs; pause/private work immediately |
| Storage failure | Disk full, read-only directory, abrupt termination and incomplete payload writes yield recoverable errors and preserve previously committed data |
| Recovery and migration | Integrity checks, consistent backups, restore, corrupt input rejection and failed-upgrade rollback preserve the original data |
| Encryption | Inspect database, sidecars, image/cache files and exports for plaintext fixtures; test wrong-key, lock/unlock and recovery behavior |
| Accessibility | Complete capture/history/search/paste/settings tasks with keyboard only; verify Narrator labels, focus, contrast and scaling |
| Windows integration | Test restart, optional sign-in startup, duplicate launch, display removal, mixed DPI and clean uninstallation |
| Offline operation | Install and run with network disabled; observe app and child processes during startup, capture, search, preview, export, OCR and AI; no attempted outbound connections |
| Optional modules | Core functions work with every module absent; disabled modules spawn no workers and allocate no model memory |

### Performance measurement protocol

The original targets are goals, not existing measurements. Record CPU model, RAM, OS build, architecture, build configuration, framework/runtime, power mode, dataset and active modules.

- Measure a release build after warm-up, with 100,000+ synthetic entries and a representative mix of images and text.
- Record idle process CPU averaged over at least five minutes; report the normalization method. Target below 0.5%.
- Record private working set and private bytes separately; use private working set for the <60 MB core idle goal and disclose the full process-tree total.
- Measure at least 100 warm popup activations and representative indexed searches. Report median and p95; aim for p95 below 100 ms. Include input handling and rendering for popup timing, and document debounce separately for search.
- Track cold startup, capture latency, burst capture, cache size and disk usage even though the source gives no numeric thresholds for those.
- Run a multi-day soak or clearly label a shorter test. Check for increasing handle counts, memory growth, missed notifications, lockups and database errors.
- Treat failure to meet a goal as an optimization item with recorded evidence, not a reason to invent a passing result.

## Phase gates

**Phase 1:** Every P0 row is implemented and mapped to verification. Core capture, persistence, paste, privacy, accessible operation and crash recovery pass. Offline behavior is verified. Record performance results and any target misses before release.

**Phase 2:** Every P1 row is implemented and verified, including offline packaging, backup/restore, encryption, exclusions, organization and productivity tools. Repeat relevant Phase 1 regression and performance checks.

**Phase 3:** Every P2 row is implemented and verified as optional capabilities where applicable. Test each module independently. Confirm that disabling all optional modules restores baseline memory and CPU behavior.

## Required deliverables

- Full source repository with clear module structure and pinned dependencies.
- Requirement/verification matrix covering every feature row.
- Build instructions for a fresh Windows environment and an offline build from pre-staged dependencies.
- Runnable release package; offline installer and portable distribution by Phase 2.
- Database schema and migration documentation.
- Security/privacy design, encryption and recovery instructions, and supported-format limitations.
- Automated tests, manual Windows verification checklist and measured benchmark report.
- User guide covering shortcuts, privacy controls, local storage, retention, backup/restore and uninstall choices.
- Third-party licenses and a dependency inventory.
- A completion report stating what is implemented, what was actually tested and what remains.

Do not claim a binary is signed unless a valid signing step was completed. If signing credentials are unavailable, deliver an explicitly unsigned development build and document the release signing procedure. Do not fabricate Windows test results when running outside Windows.

## Ready-to-use instruction for Claude Code

Read this entire specification. Inspect the current repository, map all 216 feature rows into a requirement matrix, and implement the application in the stated phases. Start with the P0 vertical slice and continue to a verified core release. Preserve all P1 and P2 scope for subsequent completion. Keep the application fully offline, native to Windows, fast and lightweight. Provide working code and reproducible deliverables, measure the performance goals, and report evidence and limitations honestly. Ask only for decisions that materially block implementation; make and document reasonable reversible engineering choices yourself.

