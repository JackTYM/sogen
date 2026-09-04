import ExtensionFoundation
import Foundation
import XPC

@available(iOS 26.0, *)
private final class JITHelperSession {
    let process: AppExtensionProcess
    let session: XPCSession

    init(process: AppExtensionProcess, session: XPCSession) {
        self.process = process
        self.session = session
    }

    deinit {
        session.cancel(reason: "JIT request finished")
        process.invalidate()
    }
}

enum JITCoordinator {
    private static var activeSession: AnyObject?

    static func enableJIT(targetPID: Int32, pairingData: Data, completion: @escaping (Bool, String) -> Void) {
        guard #available(iOS 26.0, *) else {
            completion(false, "Built-in JIT requires iOS 26 or later.")
            return
        }

        Task { @MainActor in
            do {
                let monitor = try await AppExtensionPoint.Monitor(appExtensionPoint: .jitHelper)
                guard let identity = monitor.identities.first else {
                    throw NSError(
                        domain: "JITCoordinator", code: 1,
                        userInfo: [NSLocalizedDescriptionKey: "JIT Helper extension was not found in this installation."])
                }

                let process = try await AppExtensionProcess(configuration: .init(
                    appExtensionIdentity: identity,
                    onInterruption: { activeSession = nil }))
                let xpcSession = try process.makeXPCSession()
                try xpcSession.activate()
                activeSession = JITHelperSession(process: process, session: xpcSession)

                let request = JITHelperRequest(targetPID: targetPID, pairingData: pairingData)
                try xpcSession.send(request) { (result: Result<JITHelperResponse, any Error>) in
                    activeSession = nil
                    switch result {
                    case .success(let response):
                        DispatchQueue.main.async { completion(response.success, response.message) }
                    case .failure(let error):
                        DispatchQueue.main.async { completion(false, error.localizedDescription) }
                    }
                }
            } catch {
                activeSession = nil
                completion(false, error.localizedDescription)
            }
        }
    }
}
