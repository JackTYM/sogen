import SwiftUI

struct ContentView: View {
    @State private var log: String = JITLog.readAll()

    var body: some View {
        NavigationView {
            VStack(spacing: 16) {
                ScrollView {
                    Text(log)
                        .font(.system(.footnote, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding()
                }
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

                    Button("Clear Log", role: .destructive) {
                        JITLog.clear()
                        refresh()
                    }
                }
                .padding(.horizontal)
            }
            .navigationTitle("JIT Test")
            .onAppear { refresh() }
        }
    }

    private func refresh() {
        log = JITLog.readAll()
    }
}
