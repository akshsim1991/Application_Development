#!/usr/bin/env python3
"""Writes a CycloneDX 1.5 software bill of materials. Usage: make_sbom.py VERSION OUT.json"""
import sys, json, hashlib, subprocess, os, datetime
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'); ver, out = sys.argv[1], sys.argv[2]
sha = lambda p: hashlib.sha256(open(os.path.join(root, p), 'rb').read()).hexdigest() if os.path.exists(os.path.join(root, p)) else None
def run(c):
    try: return subprocess.run(c, capture_output=True, text=True).stdout.splitlines()[0]
    except Exception: return 'unknown'
comps = [
 {"type": "library", "name": "Microsoft.Web.WebView2 (SDK headers + WebView2Loader.dll)", "version": "1.0.2792.45", "licenses": [{"license": {"name": "Microsoft BSD-style (see third_party/webview2/LICENSE.txt)"}}], "hashes": [{"alg": "SHA-256", "content": sha('third_party/webview2/build/native/x64/WebView2Loader.dll') or ''}], "purl": "pkg:nuget/Microsoft.Web.WebView2@1.0.2792.45"},
 {"type": "framework", "name": "GCC runtime (libstdc++, libgcc) statically linked", "version": run(['x86_64-w64-mingw32-g++', '-dumpversion']), "licenses": [{"license": {"name": "GPL-3.0-with-GCC-exception"}}]},
 {"type": "application", "name": "NSIS (installer builder, not shipped in the app)", "version": run(['makensis', '/VERSION']), "licenses": [{"license": {"id": "Zlib"}}]},
 {"type": "data", "name": "Feather recommendation list (data/advice.json)", "version": json.load(open(os.path.join(root, 'data/advice.json')))['version'], "hashes": [{"alg": "SHA-256", "content": sha('data/advice.json')}], "licenses": [{"license": {"name": "Project-owned"}}]},
 {"type": "library", "name": "Microsoft Windows SDK APIs (system DLLs, not redistributed)", "version": "Windows 7 or later", "licenses": [{"license": {"name": "Operating system component"}}]},
]
json.dump({"bomFormat": "CycloneDX", "specVersion": "1.5", "version": 1, "metadata": {"timestamp": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "component": {"type": "application", "name": "Feather System Utilities", "version": ver, "supplier": {"name": "Akshaya Simha"}}}, "components": comps}, open(out, 'w'), indent=1); print('SBOM written to', out)
