import Foundation
import AppKit
import Darwin

// MARK: - USB connection (usbmuxd)
// macOS runs a service called usbmuxd that carries traffic to a cabled iPhone. FileGlade asks it for the list of
// connected devices, then opens a connection to FileGlade's bridge port on the phone.

enum UsbMuxError: LocalizedError {
    case unavailable, refused, failed(String)
    var errorDescription: String? {
        switch self {
        case .unavailable: return "The Mac's USB service is not available."
        case .refused: return "FileGlade is not open on the iPhone, or USB file access is turned off."
        case .failed(let m): return m
        }
    }
}

struct UsbDevice: Hashable { let id: Int; let serial: String }

enum UsbMux {
    static let path = "/var/run/usbmuxd"
    private static func openSocket() throws -> Int32 {
        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { throw UsbMuxError.unavailable }
        var noSigPipe: Int32 = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, socklen_t(MemoryLayout<Int32>.size))
        var tv = timeval(tv_sec: 8, tv_usec: 0); setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
        var address = sockaddr_un(); address.sun_family = sa_family_t(AF_UNIX)
        withUnsafeMutablePointer(to: &address.sun_path) { pointer in
            pointer.withMemoryRebound(to: CChar.self, capacity: 104) { destination in
                path.withCString { source in _ = strncpy(destination, source, 103) }
            }
        }
        let result = withUnsafePointer(to: &address) { $0.withMemoryRebound(to: sockaddr.self, capacity: 1) { Darwin.connect(fd, $0, socklen_t(MemoryLayout<sockaddr_un>.size)) } }
        guard result == 0 else { close(fd); throw UsbMuxError.unavailable }
        return fd
    }
    private static func send(_ fd: Int32, _ message: [String: Any]) throws {
        let payload = try PropertyListSerialization.data(fromPropertyList: message, format: .xml, options: 0)
        var header = [UInt32(16 + payload.count), 1, 8, 1].flatMap { withUnsafeBytes(of: $0.littleEndian) { Array($0) } }
        header.append(contentsOf: payload)
        guard Sock.writeAll(fd, Data(header)) else { throw UsbMuxError.failed("Could not talk to the USB service.") }
    }
    private static func receive(_ fd: Int32) throws -> [String: Any] {
        guard let head = Sock.readExactly(fd, 16) else { throw UsbMuxError.failed("The USB service did not answer.") }
        let length = head.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt32.self) }.littleEndian
        guard length >= 16, length < 1_000_000, let payload = Sock.readExactly(fd, Int(length) - 16) else { throw UsbMuxError.failed("Bad answer from the USB service.") }
        return (try PropertyListSerialization.propertyList(from: payload, options: [], format: nil) as? [String: Any]) ?? [:]
    }
    static func devices() throws -> [UsbDevice] {
        let fd = try openSocket(); defer { close(fd) }
        try send(fd, ["MessageType": "ListDevices", "ClientVersionString": "FileGlade", "ProgName": "FileGlade"])
        let reply = try receive(fd)
        let list = reply["DeviceList"] as? [[String: Any]] ?? []
        return list.compactMap { entry in
            guard let id = entry["DeviceID"] as? Int, let props = entry["Properties"] as? [String: Any], let serial = props["SerialNumber"] as? String, (props["ConnectionType"] as? String ?? "USB") == "USB" else { return nil }
            return UsbDevice(id: id, serial: serial)
        }
    }
    /// Opens a raw connection to a port on the phone. The caller owns the returned descriptor.
    static func connect(device: UsbDevice, port: UInt16 = Bridge.port) throws -> Int32 {
        let fd = try openSocket()
        do {
            try send(fd, ["MessageType": "Connect", "ClientVersionString": "FileGlade", "ProgName": "FileGlade", "DeviceID": device.id, "PortNumber": Int(port.byteSwapped)])
            let reply = try receive(fd)
            guard (reply["Number"] as? Int) == 0 else { throw UsbMuxError.refused }
            return fd
        } catch { close(fd); throw error }
    }
}

enum Sock {
    static func writeAll(_ fd: Int32, _ data: Data) -> Bool {
        var sent = 0
        return data.withUnsafeBytes { raw -> Bool in
            while sent < data.count {
                let n = write(fd, raw.baseAddress!.advanced(by: sent), data.count - sent)
                if n <= 0 { return false }
                sent += n
            }
            return true
        }
    }
    static func readExactly(_ fd: Int32, _ count: Int) -> Data? {
        var data = Data(); data.reserveCapacity(count)
        var buffer = [UInt8](repeating: 0, count: min(max(count, 1), 65536))
        while data.count < count {
            let n = read(fd, &buffer, min(buffer.count, count - data.count))
            if n <= 0 { return nil }
            data.append(buffer, count: n)
        }
        return data
    }
}

// MARK: - HTTP over a connection

struct BridgeFailure: LocalizedError {
    let status: Int
    var errorDescription: String? { status == 401 ? "Enter the pairing code shown on the iPhone." : "The iPhone answered with error \(status)." }
}

final class BridgeClient {
    let token: String
    let connect: () throws -> Int32
    init(token: String, connect: @escaping () throws -> Int32) { self.token = token; self.connect = connect }

    private func open(_ path: String, _ query: [String: String], range: String? = nil) throws -> (fd: Int32, status: Int, headers: [String: String], leftover: Data) {
        let fd = try connect()
        var tv = timeval(tv_sec: 30, tv_usec: 0); setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, socklen_t(MemoryLayout<timeval>.size))
        var target = path
        if !query.isEmpty { target += "?" + query.map { "\($0.key)=\($0.value.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed.subtracting(CharacterSet(charactersIn: "&=+#"))) ?? $0.value)" }.joined(separator: "&") }
        var request = "GET \(target) HTTP/1.1\r\nHost: iphone\r\n\(Bridge.tokenHeader): \(token)\r\nConnection: close\r\n"
        if let range { request += "Range: \(range)\r\n" }
        request += "\r\n"
        guard Sock.writeAll(fd, Data(request.utf8)) else { close(fd); throw UsbMuxError.failed("Could not send the request.") }
        var buffer = Data()
        var chunk = [UInt8](repeating: 0, count: 16384)
        while buffer.range(of: Data("\r\n\r\n".utf8)) == nil {
            let n = read(fd, &chunk, chunk.count)
            if n <= 0 { close(fd); throw UsbMuxError.failed("The iPhone closed the connection.") }
            buffer.append(chunk, count: n)
            if buffer.count > 128 * 1024 { close(fd); throw UsbMuxError.failed("Unexpected answer.") }
        }
        let end = buffer.range(of: Data("\r\n\r\n".utf8))!
        let head = String(decoding: buffer[..<end.lowerBound], as: UTF8.self).components(separatedBy: "\r\n")
        let status = head.first.flatMap { $0.split(separator: " ").dropFirst().first }.flatMap { Int($0) } ?? 0
        var headers: [String: String] = [:]
        for line in head.dropFirst() { if let colon = line.firstIndex(of: ":") { headers[line[..<colon].lowercased()] = line[line.index(after: colon)...].trimmingCharacters(in: .whitespaces) } }
        return (fd, status, headers, Data(buffer[end.upperBound...]))
    }

    func get(_ path: String, _ query: [String: String] = [:]) throws -> Data {
        let (fd, status, headers, leftover) = try open(path, query)
        defer { close(fd) }
        guard status == 200 else { throw BridgeFailure(status: status) }
        let length = Int(headers["content-length"] ?? "") ?? leftover.count
        var body = leftover
        if body.count < length, let rest = Sock.readExactly(fd, length - body.count) { body.append(rest) }
        return body
    }
    func json<T: Decodable>(_ path: String, _ query: [String: String] = [:], as type: T.Type) throws -> T { try JSONDecoder().decode(T.self, from: try get(path, query)) }

    /// Streams a file to disk. Existing files are never replaced.
    func download(_ path: String, _ query: [String: String], to destination: URL, progress: @escaping (Int64, Int64) -> Void = { _, _ in }, cancelled: @escaping () -> Bool = { false }) throws -> URL {
        let (fd, status, headers, leftover) = try open(path, query)
        defer { close(fd) }
        guard status == 200 else { throw BridgeFailure(status: status) }
        let total = Int64(headers["content-length"] ?? "") ?? 0
        let target = TransferManager.uniqueURL(destination)
        FileManager.default.createFile(atPath: target.path, contents: nil)
        guard let out = try? FileHandle(forWritingTo: target) else { throw UsbMuxError.failed("Cannot write \(target.lastPathComponent).") }
        var done: Int64 = 0
        do {
            if !leftover.isEmpty { try out.write(contentsOf: leftover); done += Int64(leftover.count) }
            var chunk = [UInt8](repeating: 0, count: 256 * 1024)
            while total == 0 || done < total {
                if cancelled() { throw CancellationError() }
                let n = read(fd, &chunk, chunk.count)
                if n <= 0 { break }
                try out.write(contentsOf: Data(chunk[0..<n])); done += Int64(n)
                progress(done, total)
            }
            try out.close()
            if total > 0 && done < total { throw UsbMuxError.failed("The transfer was interrupted.") }
            return target
        } catch { try? out.close(); try? FileManager.default.removeItem(at: target); throw error }
    }
}

// MARK: - Devices

/// A cabled iPhone that FileGlade can reach over USB.
final class BridgeDevice: ObservableObject, Identifiable {
    enum State: Equatable { case checking, appNotOpen, needsCode, ready, failed(String) }
    let usb: UsbDevice
    var id: String { usb.serial }
    @Published var state: State = .checking
    @Published var info: BridgeInfo?
    @Published var roots: [BridgeRoot] = []
    init(_ usb: UsbDevice) { self.usb = usb }
    var token: String { get { UserDefaults.standard.string(forKey: "bridgeToken." + usb.serial) ?? "" } set { UserDefaults.standard.set(newValue, forKey: "bridgeToken." + usb.serial); objectWillChange.send() } }
    var client: BridgeClient { BridgeClient(token: token) { [usb] in try UsbMux.connect(device: usb) } }
    var title: String { info?.model ?? "iPhone" }
    var name: String { info?.name ?? "iPhone" }

    func refresh() {
        let client = self.client
        DispatchQueue.global(qos: .utility).async {
            do {
                let info = try client.json("/info", as: BridgeInfo.self)
                let roots = try client.json("/roots", as: [BridgeRoot].self)
                DispatchQueue.main.async { self.info = info; self.roots = roots; self.state = .ready }
            } catch let failure as BridgeFailure {
                DispatchQueue.main.async { self.state = failure.status == 401 ? .needsCode : .failed(failure.localizedDescription) }
            } catch UsbMuxError.refused {
                DispatchQueue.main.async { self.state = .appNotOpen }
            } catch { DispatchQueue.main.async { self.state = .failed(error.localizedDescription) } }
        }
    }
}

final class BridgeStore: ObservableObject {
    static let shared = BridgeStore()
    @Published var devices: [BridgeDevice] = []
    private var timer: Timer?
    func start() {
        guard !Prefs.selfTest, timer == nil else { return }
        poll()
        timer = Timer.scheduledTimer(withTimeInterval: 4, repeats: true) { [weak self] _ in self?.poll() }
    }
    private func poll() {
        DispatchQueue.global(qos: .utility).async {
            let found = (try? UsbMux.devices()) ?? []
            DispatchQueue.main.async {
                let serials = Set(found.map(\.serial))
                self.devices.removeAll { !serials.contains($0.usb.serial) }
                for usb in found {
                    if let existing = self.devices.first(where: { $0.usb.serial == usb.serial }) {
                        if existing.state != .ready { existing.refresh() }
                    } else { let device = BridgeDevice(usb); self.devices.append(device); device.refresh() }
                }
            }
        }
    }
}
