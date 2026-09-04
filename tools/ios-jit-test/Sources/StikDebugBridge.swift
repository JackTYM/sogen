import UIKit

func openStikDebugJIT() {
    guard let bundleID = Bundle.main.bundleIdentifier else {
        JITLog.append("StikDebug request FAILED: no bundle identifier")
        return
    }

    var components = URLComponents()
    components.scheme = "stikdebug"
    components.host = "enable-jit"
    components.queryItems = [
        URLQueryItem(name: "bundle-id", value: bundleID),
        URLQueryItem(name: "pid", value: String(getpid())),
        URLQueryItem(name: "script-name", value: "universal.js"),
    ]

    guard let url = components.url else {
        JITLog.append("StikDebug request FAILED: could not build URL")
        return
    }

    JITLog.append("Opening StikDebug: \(url.absoluteString)")
    UIApplication.shared.open(url)
}
