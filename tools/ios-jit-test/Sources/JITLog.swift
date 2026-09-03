import Foundation

enum JITLog {
    static let path: String = {
        let dir = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true)[0]
        return dir + "/jit_test_log.txt"
    }()

    static func append(_ message: String) {
        let line = "[\(Date())] \(message)\n"
        guard let data = line.data(using: .utf8) else { return }
        let fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0o644)
        guard fd >= 0 else { return }
        data.withUnsafeBytes { raw in
            _ = write(fd, raw.baseAddress, raw.count)
        }
        close(fd)
    }

    static func readAll() -> String {
        (try? String(contentsOfFile: path, encoding: .utf8)) ?? "(no log yet — run a test)"
    }

    static func clear() {
        try? FileManager.default.removeItem(atPath: path)
    }
}
