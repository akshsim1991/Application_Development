#!/usr/bin/env python3
"""Adds Swift files (inside the FileGlade folder) to FileGlade.xcodeproj. Usage: add_to_project.py File1.swift File2.swift"""
import re, sys, os
proj = os.path.join(os.path.dirname(__file__), "FileGlade.xcodeproj", "project.pbxproj")
text = open(proj).read()
used = [int(m, 16) for m in re.findall(r"F0{19}([0-9A-F]{4})", text)]
n = (max(used) + 2) if used else 0x7000
for name in sys.argv[1:]:
    if re.search(r'path = "?%s"?;' % re.escape(name), text): continue
    ref = "F%023X" % n; bld = "F%023X" % (n + 1); n += 2
    text = text.replace(" A00000000000000000000020 = {isa = PBXBuildFile;", " %s = {isa = PBXFileReference; lastKnownFileType = sourcecode.swift; path = \"%s\"; sourceTree = \"<group>\"; };\n %s = {isa = PBXBuildFile; fileRef = %s; };\n A00000000000000000000020 = {isa = PBXBuildFile;" % (ref, name, bld, ref), 1)
    text = text.replace("A00000000000000000000015 = {isa = PBXGroup; children = (", "A00000000000000000000015 = {isa = PBXGroup; children = (%s, " % ref, 1)
    text = text.replace("A00000000000000000000007 = {isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = (", "A00000000000000000000007 = {isa = PBXSourcesBuildPhase; buildActionMask = 2147483647; files = (%s, " % bld, 1)
open(proj, "w").write(text)
