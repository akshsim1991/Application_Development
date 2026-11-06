import Foundation

let fm = FileManager.default
let root = fm.temporaryDirectory.appendingPathComponent("FileGlade-iOS-tests-\(UUID().uuidString)")
try fm.createDirectory(at: root, withIntermediateDirectories: true)
defer { try? fm.removeItem(at: root) }
let file = root.appendingPathComponent("Notes.txt")
try Data("Keep my contents".utf8).write(to: file)
let directory = try FileOperations.newFolder("Projects", in: root)
let listing = try FileOperations.list(root, hidden: false)
precondition(listing.count == 2)
precondition(listing.contains { $0.name == "Projects" && $0.isDirectory })
let copy = try FileOperations.transfer(file, into: root, moving: false)
precondition(copy.lastPathComponent == "Notes copy 2.txt")
let copiedData = try Data(contentsOf: copy)
let originalData = try Data(contentsOf: file)
precondition(copiedData == originalData)
let renamed = try FileOperations.rename(copy, to: "Renamed.txt")
precondition(fm.fileExists(atPath: renamed.path) && !fm.fileExists(atPath: copy.path))
let moved = try FileOperations.transfer(renamed, into: directory, moving: true)
precondition(fm.fileExists(atPath: moved.path) && !fm.fileExists(atPath: renamed.path))
do { _ = try FileOperations.transfer(directory, into: directory, moving: false); fatalError("Allowed folder inside itself") } catch FileIssue.insideItself {}
do { _ = try FileOperations.newFolder("../bad", in: root); fatalError("Allowed path traversal") } catch FileIssue.invalidName {}
do { _ = try FileOperations.rename(file, to: "Projects"); fatalError("Overwrote existing folder") } catch {}
let trash = root.appendingPathComponent("Recycle")
try FileOperations.recycle(file, trash: trash)
precondition(!fm.fileExists(atPath: file.path))
let records = FileOperations.deletedItems(in: trash)
precondition(records.count == 1 && records[0].name == "Notes.txt")
// A newly-created file must survive restore without being overwritten.
try Data("New document".utf8).write(to: file)
try FileOperations.restore(records[0], trash: trash)
let preserved = try String(contentsOf: file, encoding: .utf8)
precondition(preserved == "New document")
let restored = root.appendingPathComponent("Notes copy 3.txt")
// The earlier copy was renamed away, so copy 2 is available again.
let restored2 = root.appendingPathComponent("Notes copy 2.txt")
precondition(fm.fileExists(atPath: restored2.path) || fm.fileExists(atPath: restored.path))
precondition(FileOperations.deletedItems(in: trash).isEmpty)
print("PASS: directory listing, file copies, collision preservation, rename, cut/move, recursive-copy guard, invalid names, recoverable deletion, and restore")
