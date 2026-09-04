import ExtensionFoundation
import Foundation
import StikJIT
import XPC

private struct JITMessageHandler: XPCPeerHandler {
    func handleIncomingRequest(_ message: JITHelperRequest) -> (any Encodable)? {
        let manager = FileManager.default
        let root = manager.urls(for: .libraryDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("JITHelper", isDirectory: true)
        let pairingURL = root.appendingPathComponent("pairingFile.plist")

        do {
            try manager.createDirectory(at: root, withIntermediateDirectories: true)
            try message.pairingData.write(to: pairingURL, options: .atomic)
            defer { try? manager.removeItem(at: pairingURL) }

            let paths = DDIPaths.default(in: root)
            try StikJIT.enableJIT(
                targetPID: message.targetPID,
                pairingFile: pairingURL,
                ddiPaths: paths,
                script: .universal,
                progress: { NSLog("[JITHelper] %@", $0) })
            return JITHelperResponse(success: true, message: "JIT enabled by the built-in helper.")
        } catch {
            return JITHelperResponse(success: false, message: error.localizedDescription)
        }
    }
}

@main
struct JITHelperExtension: AppExtension {
    @AppExtensionPoint.Bind
    var extensionPoint: AppExtensionPoint {
        AppExtensionPoint.Identifier(host: "com.jacksonyarger.jittest", name: "JITHelper")
    }

    var configuration: some AppExtensionConfiguration {
        ConnectionHandler(onSessionRequest: { request in
            request.accept { _ in
                JITMessageHandler()
            }
        })
    }
}
