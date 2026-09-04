import SwiftUI
import UniformTypeIdentifiers

private final class JIT26RegionHolder {
    var region: UnsafeMutableRawPointer?
}

struct ContentView: View {
    @State private var log: String = JITLog.readAll()
    @State private var showingPairingFileImporter = false

    private var pairingFileURL: URL {
        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return documents.appendingPathComponent("pairingFile.plist")
    }

    var body: some View {
        NavigationView {
            VStack(spacing: 16) {
                ScrollView {
                    Text(log)
                        .font(.system(.footnote, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding()
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color(.secondarySystemBackground))
                .cornerRadius(8)

                VStack(spacing: 12) {
                    Button("Test 1: Direct JIT (no debugger)") {
                        JITLog.append("=== Test 1: JIT attempt, no debugger ===")
                        refresh()
                        attemptJIT(label: "Test1")
                        refresh()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Test 2: Self-Attach (ptrace), then JIT") {
                        JITLog.append("=== Test 2: fork+ptrace self-attach, then JIT ===")
                        refresh()
                        _ = selfAttachViaPtrace()
                        refresh()
                        attemptJIT(label: "Test2")
                        refresh()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Open StikDebug (Enable JIT)") {
                        openStikDebugJIT()
                        refresh()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Test 3: Attempt JIT (after StikDebug)") {
                        JITLog.append("=== Test 3: JIT attempt, after StikDebug ===")
                        refresh()
                        attemptJIT(label: "Test3-AfterStikDebug-plain")
                        refresh()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Test 4: JIT26 protocol (after StikDebug)") {
                        JITLog.append("=== Test 4: JIT26 breakpoint-protocol attempt ===")
                        refresh()
                        attemptJIT26(label: "Test4-JIT26")
                        refresh()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Import Pairing File") {
                        showingPairingFileImporter = true
                    }
                    .buttonStyle(.bordered)
                    .fileImporter(isPresented: $showingPairingFileImporter, allowedContentTypes: [.item]) { result in
                        switch result {
                        case .success(let sourceURL):
                            importPairingFile(from: sourceURL)
                        case .failure(let error):
                            JITLog.append("Pairing file import FAILED: \(error.localizedDescription)")
                        }
                        refresh()
                    }

                    Button("Test 5: Built-in JIT (in-app helper)") {
                        JITLog.append("=== Test 5: Built-in JIT via helper extension ===")
                        refresh()
                        attemptBuiltInJIT()
                    }
                    .buttonStyle(.borderedProminent)

                    Button("Clear Log", role: .destructive) {
                        JITLog.clear()
                        refresh()
                    }
                }
                .padding(.horizontal)
            }
            .navigationTitle("JIT Test")
            .onAppear {
                refresh()
                runAutomatedSequence()
            }
        }
    }

    private func refresh() {
        log = JITLog.readAll()
    }

    private func importPairingFile(from sourceURL: URL) {
        let manager = FileManager.default
        let didAccess = sourceURL.startAccessingSecurityScopedResource()
        defer { if didAccess { sourceURL.stopAccessingSecurityScopedResource() } }

        do {
            try manager.createDirectory(at: pairingFileURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            if manager.fileExists(atPath: pairingFileURL.path) {
                try manager.removeItem(at: pairingFileURL)
            }
            try manager.copyItem(at: sourceURL, to: pairingFileURL)
            JITLog.append("Pairing file imported to \(pairingFileURL.path)")
        } catch {
            JITLog.append("Pairing file import FAILED: \(error.localizedDescription)")
        }
    }

    private func attemptBuiltInJIT() {
        guard let pairingData = try? Data(contentsOf: pairingFileURL) else {
            JITLog.append("[Test5] No pairing file imported yet — tap \"Import Pairing File\" first")
            refresh()
            return
        }

        JITLog.append("[Test5] Starting our own tunnel extension ...")
        refresh()
        TunnelManager.ensureConnected { tunnelUp, tunnelMessage in
            JITLog.append("[Test5] Tunnel \(tunnelUp ? "UP" : "FAILED"): \(tunnelMessage)")
            refresh()
            self.startBuiltInJIT(pairingData: pairingData)
        }
    }

    private func startBuiltInJIT(pairingData: Data) {
        let holder = JIT26RegionHolder()

        JITLog.append("[Test5] Calling helper (XPC) — preparing JIT26 region concurrently, not after, since the attached script blocks waiting for our brk calls ...")
        refresh()

        DispatchQueue.global(qos: .userInitiated).async {
            holder.region = prepareJIT26(label: "Test5")
        }

        JITCoordinator.enableJIT(targetPID: getpid(), pairingData: pairingData) { success, message in
            JITLog.append("[Test5] Built-in JIT \(success ? "SUCCEEDED" : "FAILED"): \(message)")
            refresh()
            guard success else { return }
            guard let region = holder.region else {
                JITLog.append("[Test5] helper reported success but no region was prepared")
                refresh()
                return
            }
            executeJIT26(label: "Test5", region: region)
            refresh()
        }
    }

    private func runAutomatedSequence() {
        if !log.contains("Test1-NoDebugger") {
            JITLog.append("=== AUTO: Test 1 (no debugger) ===")
            refresh()
            attemptJIT(label: "Test1-NoDebugger")
            refresh()
        }

        if log.contains("Test1-NoDebugger"), !log.contains("Test2-AfterSelfAttach") {
            JITLog.append("=== AUTO: Test 2 (self-attach + JIT) ===")
            refresh()
            _ = selfAttachViaPtrace()
            refresh()
            attemptJIT(label: "Test2-AfterSelfAttach")
            refresh()
        }
    }
}
