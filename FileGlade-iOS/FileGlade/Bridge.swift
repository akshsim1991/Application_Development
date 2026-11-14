import Foundation
import Network

// MARK: - Shared between FileGlade for Mac and FileGlade for iPhone
//
// The iPhone app runs a tiny read-only file server that listens only on the phone itself (loopback). The Mac reaches it
// through the USB cable (the same channel Finder uses to talk to the phone), so nothing is exposed to Wi-Fi or the
// internet. A pairing code shown on the phone must be entered on the Mac once.

enum Bridge {
    static let port: UInt16 = 38765
    static let tokenHeader = "X-FileGlade-Token"
}

struct BridgeInfo: Codable {
    var name: String
    var model: String          // e.g. "iPhone 13"
    var machine: String        // e.g. "iPhone14,5"
    var systemName: String
    var systemVersion: String
    var appVersion: String
    var freeBytes: Int64
    var totalBytes: Int64
}
struct BridgeRoot: Codable, Identifiable, Hashable {
    var id: String
    var name: String
    var kind: String           // "files" or "photos"
}
struct BridgeEntry: Codable, Identifiable, Hashable {
    var id: String             // path inside a root, or a photo asset id
    var name: String
    var isDirectory: Bool
    var size: Int64
    var modified: Double?
    var kind: String           // folder, photo, video, file
}
struct BridgeListing: Codable { var entries: [BridgeEntry]; var total: Int }

struct HTTPRequest {
    var method = "GET"
    var path = "/"
    var query: [String: String] = [:]
    var headers: [String: String] = [:]
}
struct HTTPResponse {
    enum Body { case none, data(Data), file(URL, offset: Int64, length: Int64) }
    var status = 200
    var headers: [String: String] = [:]
    var body: Body = .none
    static func json<T: Encodable>(_ value: T) -> HTTPResponse {
        guard let data = try? JSONEncoder().encode(value) else { return HTTPResponse(status: 500) }
        return HTTPResponse(status: 200, headers: ["Content-Type": "application/json"], body: .data(data))
    }
    static func text(_ status: Int, _ message: String) -> HTTPResponse { HTTPResponse(status: status, headers: ["Content-Type": "text/plain"], body: .data(Data(message.utf8))) }
}

/// A minimal HTTP/1.1 server for the bridge. It speaks only GET with one request per connection.
final class BridgeHTTPServer {
    private var listener: NWListener?
    private let queue = DispatchQueue(label: "fileglade.bridge.server")
    var handler: (HTTPRequest) async -> HTTPResponse = { _ in .text(404, "Not found") }
    private(set) var running = false

    func start(port: UInt16 = Bridge.port, loopbackOnly: Bool = true) throws {
        guard !running else { return }
        let parameters = NWParameters.tcp
        if loopbackOnly { parameters.requiredInterfaceType = .loopback }
        let listener = try NWListener(using: parameters, on: NWEndpoint.Port(rawValue: port)!)
        listener.newConnectionHandler = { [weak self] connection in self?.accept(connection) }
        listener.start(queue: queue)
        self.listener = listener; running = true
    }
    func stop() { listener?.cancel(); listener = nil; running = false }

    private func accept(_ connection: NWConnection) {
        connection.start(queue: queue)
        read(connection, buffer: Data())
    }
    private func read(_ connection: NWConnection, buffer: Data) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 16 * 1024) { [weak self] data, _, complete, error in
            guard let self else { return }
            var buffer = buffer
            if let data { buffer.append(data) }
            if let end = buffer.range(of: Data("\r\n\r\n".utf8)) {
                let head = String(decoding: buffer[..<end.lowerBound], as: UTF8.self)
                guard let request = Self.parse(head) else { return self.send(.text(400, "Bad request"), on: connection) }
                Task { let response = await self.handler(request); self.send(response, on: connection) }
            } else if error != nil || complete || buffer.count > 64 * 1024 { connection.cancel() }
            else { self.read(connection, buffer: buffer) }
        }
    }
    static func parse(_ head: String) -> HTTPRequest? {
        let lines = head.components(separatedBy: "\r\n")
        guard let first = lines.first else { return nil }
        let parts = first.split(separator: " ")
        guard parts.count >= 2 else { return nil }
        var request = HTTPRequest(method: String(parts[0]))
        let target = String(parts[1])
        if let q = target.firstIndex(of: "?") {
            request.path = String(target[..<q])
            for pair in target[target.index(after: q)...].split(separator: "&") {
                let kv = pair.split(separator: "=", maxSplits: 1)
                if kv.count == 2 { request.query[String(kv[0])] = String(kv[1]).removingPercentEncoding ?? String(kv[1]) }
            }
        } else { request.path = target }
        for line in lines.dropFirst() {
            if let colon = line.firstIndex(of: ":") { request.headers[line[..<colon].lowercased()] = line[line.index(after: colon)...].trimmingCharacters(in: .whitespaces) }
        }
        return request
    }
    private func send(_ response: HTTPResponse, on connection: NWConnection) {
        var headers = response.headers
        var length: Int64 = 0
        switch response.body { case .none: length = 0; case .data(let d): length = Int64(d.count); case .file(_, _, let l): length = l }
        headers["Content-Length"] = String(length); headers["Connection"] = "close"; headers["Accept-Ranges"] = "bytes"
        let reason = [200: "OK", 206: "Partial Content", 400: "Bad Request", 401: "Unauthorized", 403: "Forbidden", 404: "Not Found", 416: "Range Not Satisfiable", 500: "Internal Server Error"][response.status] ?? "OK"
        var head = "HTTP/1.1 \(response.status) \(reason)\r\n"
        for (k, v) in headers { head += "\(k): \(v)\r\n" }
        head += "\r\n"
        connection.send(content: Data(head.utf8), completion: .contentProcessed { [weak self] _ in
            switch response.body {
            case .none: connection.cancel()
            case .data(let data): connection.send(content: data, completion: .contentProcessed { _ in connection.cancel() })
            case .file(let url, let offset, let length): self?.stream(url, offset: offset, length: length, on: connection)
            }
        })
    }
    /// Sends a file in 256 KB pieces, waiting for each to leave before reading the next, so large videos stay light on memory.
    private func stream(_ url: URL, offset: Int64, length: Int64, on connection: NWConnection) {
        guard let handle = try? FileHandle(forReadingFrom: url) else { connection.cancel(); return }
        try? handle.seek(toOffset: UInt64(offset))
        var remaining = length
        func next() {
            guard remaining > 0, let chunk = try? handle.read(upToCount: Int(min(256 * 1024, remaining))), !chunk.isEmpty else { try? handle.close(); connection.cancel(); return }
            remaining -= Int64(chunk.count)
            connection.send(content: chunk, completion: .contentProcessed { error in if error != nil { try? handle.close(); connection.cancel() } else { next() } })
        }
        next()
    }
}

/// Answers the bridge's read-only questions from folders on disk. The iPhone app adds Photos on top of this.
final class FileBridgeService {
    var token: String
    var roots: () -> [(root: BridgeRoot, url: URL?)]
    var info: () -> BridgeInfo
    var extra: (HTTPRequest) async -> HTTPResponse? = { _ in nil }
    init(token: String, roots: @escaping () -> [(root: BridgeRoot, url: URL?)], info: @escaping () -> BridgeInfo) { self.token = token; self.roots = roots; self.info = info }

    func handle(_ request: HTTPRequest) async -> HTTPResponse {
        guard request.method == "GET" else { return .text(400, "Only GET is supported") }
        guard !token.isEmpty, request.headers[Bridge.tokenHeader.lowercased()] == token else { return .text(401, "Pairing code required") }
        switch request.path {
        case "/info": return .json(info())
        case "/roots": return .json(roots().map(\.root))
        case "/list": return list(request)
        case "/file": return file(request)
        default: return await extra(request) ?? .text(404, "Not found")
        }
    }
    private func resolve(_ request: HTTPRequest) -> URL? {
        guard let rootID = request.query["root"], let base = roots().first(where: { $0.root.id == rootID })?.url else { return nil }
        let relative = request.query["path"] ?? ""
        let components = relative.split(separator: "/").map(String.init)
        guard !components.contains("..") else { return nil }
        return components.reduce(base) { $0.appendingPathComponent($1) }
    }
    private func list(_ request: HTTPRequest) -> HTTPResponse {
        guard let url = resolve(request) else { return .text(404, "Unknown folder") }
        let keys: [URLResourceKey] = [.isDirectoryKey, .fileSizeKey, .contentModificationDateKey]
        guard let items = try? FileManager.default.contentsOfDirectory(at: url, includingPropertiesForKeys: keys, options: [.skipsHiddenFiles]) else { return .text(404, "Cannot read folder") }
        let base = request.query["path"] ?? ""
        let entries: [BridgeEntry] = items.map { item in
            let v = try? item.resourceValues(forKeys: Set(keys))
            let isDir = v?.isDirectory ?? false
            let type = BridgeKind.of(item.pathExtension)
            return BridgeEntry(id: (base.isEmpty ? "" : base + "/") + item.lastPathComponent, name: item.lastPathComponent, isDirectory: isDir, size: Int64(v?.fileSize ?? 0), modified: v?.contentModificationDate?.timeIntervalSince1970, kind: isDir ? "folder" : type)
        }.sorted { ($0.isDirectory ? 0 : 1, $0.name.lowercased()) < ($1.isDirectory ? 0 : 1, $1.name.lowercased()) }
        return .json(BridgeListing(entries: entries, total: entries.count))
    }
    private func file(_ request: HTTPRequest) -> HTTPResponse {
        guard let url = resolve(request), let size = (try? FileManager.default.attributesOfItem(atPath: url.path))?[.size] as? NSNumber else { return .text(404, "Unknown file") }
        return Self.fileResponse(url, size: size.int64Value, rangeHeader: request.headers["range"])
    }
    static func fileResponse(_ url: URL, size: Int64, rangeHeader: String?) -> HTTPResponse {
        var offset: Int64 = 0, length = size, status = 200
        var headers = ["Content-Type": "application/octet-stream"]
        if let range = rangeHeader, range.hasPrefix("bytes=") {
            let parts = range.dropFirst(6).split(separator: "-", omittingEmptySubsequences: false)
            if let start = parts.first.flatMap({ Int64($0) }) {
                let end = parts.count > 1 ? (Int64(parts[1]) ?? size - 1) : size - 1
                guard start < size, start <= end else { return HTTPResponse(status: 416) }
                offset = start; length = min(end, size - 1) - start + 1; status = 206
                headers["Content-Range"] = "bytes \(start)-\(offset + length - 1)/\(size)"
            }
        }
        return HTTPResponse(status: status, headers: headers, body: .file(url, offset: offset, length: length))
    }
}

enum BridgeKind {
    static func of(_ ext: String) -> String {
        switch ext.lowercased() {
        case "jpg", "jpeg", "png", "heic", "heif", "gif", "tiff", "tif", "bmp", "webp", "dng", "raw": return "photo"
        case "mov", "mp4", "m4v", "avi", "mkv", "3gp": return "video"
        default: return "file"
        }
    }
}
