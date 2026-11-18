# Third-party notices and SBOM (Feather System Utilities 1.6.0)

Developed by Akshaya Simha. No third-party code is embedded in the application source.

| Component | Use | License |
|---|---|---|
| Windows SDK APIs (via MinGW-w64 headers/import libs) | OS interfaces (SCM, registry, Task Scheduler, Event Log, WinVerifyTrust, shell) | MinGW-w64 headers: ZPL/public domain mix |
| GCC runtime (libstdc++, libgcc) statically linked | C++ runtime | GPLv3 with GCC Runtime Library Exception |
| NSIS 3.09 (installer builder only; not in the app) | Setup/uninstall stub | zlib/libpng license (+ bzip2, lzma components) |

Build toolchain: x86_64-w64-mingw32-g++ 13 (win32 threads), windres, makensis. No package-registry access at runtime.
No telemetry, no automatic update, no network connections by the app. The app opens your browser only after you confirm "Check on VirusTotal"/"Search online".
Not code-signed: Windows SmartScreen may warn.
