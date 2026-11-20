# Feather System Utilities 3.0.0 — new original UI (WebView2)
- Completely new interface (HTML/CSS in a WebView2 window): Today dashboard, Clean with "Why is this listed?" evidence, Programs with leftover preview, Startup with evidence panel, Services, Files (large / duplicates / empty), Storage map (drill-down treemap), Journal & rollback shelf, Settings.
- Same C++ engine as 2.0.x behind a JSON bridge. Every change is still previewed, backed up and journaled.
- Needs the Microsoft Edge WebView2 Runtime (built into Windows 11 and current Windows 10). If it is missing the app falls back to the classic 2.0 window automatically; `--classic` forces the classic window.
- Open file location: path is canonicalized and passed to Explorer via ILCreateFromPath, with an exact `explorer /select,"path"` fallback.
- NOT yet run on real Windows by the developer (no Windows in the build container): please test and report.
- The portable x64/x86 exes need WebView2Loader.dll in the same folder (the installer places it).
