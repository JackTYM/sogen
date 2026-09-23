# LocalDevVPN Support Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the self-signed/sideload build of the sogen iOS app a working JIT-grant tunnel via LocalDevVPN, since its current mechanism (`TunnelExtension`) needs a Network Extension entitlement personal developer accounts cannot obtain.

**Architecture:** A compile-time flag (`SOGEN_IOS_USE_LOCALDEVVPN`, mirroring the existing `SOGEN_IOS_USE_FEX` toggle pattern in `project.yml`) selects, at the single point `JITGateOrchestrator.run` establishes its tunnel, between the existing `TunnelManager` (`TunnelExtension`-based) and a new `LocalDevVPNManager` (URL-scheme handshake with the separately-installed LocalDevVPN app). Everything downstream of a successful tunnel (`JITCoordinator.enableJIT`, `JITGate.prepareRegion`/`verifyRegion`) is unchanged either way.

**Tech Stack:** Swift/SwiftUI (`tools/sogen-ios/Sources/`), XcodeGen (`project.yml`), no new dependencies.

**User decisions (already made):**
- Two build variants, chosen at build time by manually toggling `project.yml` (same as the existing `SOGEN_IOS_USE_FEX` precedent): **developer-signed** (default, unchanged) keeps `TunnelExtension`; **self-signed/sideload** drops `TunnelExtension`'s target dependency entirely and always uses LocalDevVPN.
- No runtime auto-detection or fallback between the two mechanisms — this is a build-time decision, not a runtime one (Apple gives no distinguishable runtime error code for "this app lacks the Network Extension entitlement").
- No settings UI in this round. A user-facing toggle (this mechanism, and separately FEX/Unicorn) is deferred to a later full settings-screen/profiles UI overhaul.
- If LocalDevVPN isn't installed, show a plain "LocalDevVPN required — tap to open the App Store, then relaunch" message; no more elaborate guided install flow.

Full design rationale: `docs/superpowers/specs/2026-09-16-ios-localdevvpn-design.md`.

---

### Task 1: `project.yml` — URL schemes + documented build-variant toggle

**Goal:** Register the URL schemes both build variants need, and document (without changing default behavior) exactly how to flip `project.yml` into the self-signed/LocalDevVPN variant.

**Files:**
- Modify: `tools/sogen-ios/project.yml`

**Acceptance Criteria:**
- [ ] `LSApplicationQueriesSchemes` includes `localdevvpn` (needed for `UIApplication.canOpenURL` to detect the app).
- [ ] `CFBundleURLTypes` registers the `sogenios` scheme (needed for LocalDevVPN's completion callback).
- [ ] The existing `TunnelExtension` dependency and `GCC_PREPROCESSOR_DEFINITIONS` are unchanged in their *effective* values (still developer-signed/`TunnelExtension`-embedded by default) — only comments are added showing how to flip them.
- [ ] `xcodegen generate` (run from `tools/sogen-ios/`) succeeds with no errors.

**Verify:** `cd tools/sogen-ios && xcodegen generate` → exits 0, regenerates `SogenIOS.xcodeproj` with no diff to target dependencies/embedded frameworks from before this task.

**Steps:**

- [ ] **Step 1: Add the two URL-scheme entries to the `info.properties` block**

In `tools/sogen-ios/project.yml`, find the `info:` block under the `SogenIOS` target (currently ends around the `UISupportedInterfaceOrientations` list):

```yaml
    info:
      path: Generated-Info.plist
      properties:
        UILaunchScreen: {}
        UIFileSharingEnabled: true
        LSSupportsOpeningDocumentsInPlace: true
        UISupportedInterfaceOrientations:
          - UIInterfaceOrientationPortrait
```

Change it to:

```yaml
    info:
      path: Generated-Info.plist
      properties:
        UILaunchScreen: {}
        UIFileSharingEnabled: true
        LSSupportsOpeningDocumentsInPlace: true
        UISupportedInterfaceOrientations:
          - UIInterfaceOrientationPortrait
        # LocalDevVPN support (see docs/superpowers/specs/2026-09-16-ios-localdevvpn-design.md).
        # Declared unconditionally in both build variants -- harmless when unused (developer-signed
        # builds never call LocalDevVPNManager), and needed at all in the self-signed variant.
        LSApplicationQueriesSchemes:
          - localdevvpn
        CFBundleURLTypes:
          - CFBundleURLName: com.jacksonyarger.sogenios
            CFBundleURLSchemes:
              - sogenios
```

- [ ] **Step 2: Document the `TunnelExtension` dependency toggle**

Find this block (currently around line 43-44, inside `SogenIOS`'s `dependencies:` list):

```yaml
      - target: TunnelExtension
        embed: true
```

Change it to:

```yaml
      # Self-signed/sideload builds: comment out this dependency (both lines below) -- a personal
      # developer account can never provision TunnelExtension's Network Extension entitlement (see
      # tools/sogen-ios/README.md's "JIT grant" section). Also set SOGEN_IOS_USE_LOCALDEVVPN=1 in
      # GCC_PREPROCESSOR_DEFINITIONS below when doing so, so LocalDevVPNManager.swift is used for
      # the JIT-grant tunnel instead of TunnelManager.swift. See
      # docs/superpowers/specs/2026-09-16-ios-localdevvpn-design.md.
      - target: TunnelExtension
        embed: true
```

- [ ] **Step 3: Document the `SOGEN_IOS_USE_LOCALDEVVPN` flag**

Find this line (currently around line 67):

```yaml
        # Developer toggle: construct the FEX backend instead of Unicorn in SogenBridge.mm
        # (see SOGEN_IOS_USE_FEX there). Enabled for the Phase 2 real-device demo build.
        GCC_PREPROCESSOR_DEFINITIONS: ["SOGEN_IOS_USE_FEX=1"]
```

Change it to:

```yaml
        # Developer toggle: construct the FEX backend instead of Unicorn in SogenBridge.mm
        # (see SOGEN_IOS_USE_FEX there). Enabled for the Phase 2 real-device demo build.
        #
        # Self-signed/sideload builds: add "SOGEN_IOS_USE_LOCALDEVVPN=1" to the array below (comma-
        # separated), and comment out the TunnelExtension dependency above -- see that dependency's
        # own comment and docs/superpowers/specs/2026-09-16-ios-localdevvpn-design.md.
        GCC_PREPROCESSOR_DEFINITIONS: ["SOGEN_IOS_USE_FEX=1"]
```

- [ ] **Step 4: Regenerate and verify**

```bash
cd tools/sogen-ios && xcodegen generate
```

Expected: exits 0. `git diff SogenIOS.xcodeproj/project.pbxproj` (the generated project file — check whether it's gitignored; if not, this confirms no dependency/embed changes, only `Info.plist`-adjacent additions for the two new keys).

- [ ] **Step 5: Commit**

```bash
git add tools/sogen-ios/project.yml
git commit -m "feat(ios): register LocalDevVPN URL schemes, document build-variant toggle"
```

---

### Task 2: `LocalDevVPNManager.swift` — the LocalDevVPN tunnel mechanism

**Goal:** A new, self-contained manager that establishes the JIT-grant tunnel via LocalDevVPN's URL-scheme handshake instead of `TunnelExtension`.

**Files:**
- Create: `tools/sogen-ios/Sources/JIT/LocalDevVPNManager.swift`

**Acceptance Criteria:**
- [ ] Exposes `LocalDevVPNResult` (`.connected` / `.notInstalled` / `.failed(String)`) and `LocalDevVPNManager.ensureConnected(completion:)`, matching the shape described in the design spec's "Component" section.
- [ ] Detects "not installed" via `UIApplication.shared.canOpenURL` against the `localdevvpn://` scheme, with zero network/tunnel attempt in that case.
- [ ] Opens `localdevvpn://enable?scheme=sogenios` to request the tunnel when installed.
- [ ] Exposes `LocalDevVPNManager.handleCallback()` for the app's URL-open handler (wired in Task 3) to call when a `sogenios://` callback arrives, resolving the pending completion with `.connected`.
- [ ] Times out (8 seconds) and resolves `.failed(...)` if no callback arrives, rather than hanging forever.
- [ ] Builds cleanly as part of the `SogenIOS` target.

**Verify:** `cmake`/Xcode build of the `SogenIOS` target succeeds with this file added (this project has no Swift unit-test target — see Task 6 for the only real verification, on-device). At minimum: `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`, with this file's symbols present (`nm` on the built binary, or just trust a clean build since Swift would fail loudly on any reference error).

**Steps:**

- [ ] **Step 1: Write the file**

Create `tools/sogen-ios/Sources/JIT/LocalDevVPNManager.swift`:

```swift
import Foundation
import UIKit

// Provides the same JIT-grant tunnel role as TunnelManager.swift, via LocalDevVPN
// (https://github.com/jkcoxson/LocalDevVPN) -- a separately-installed App Store app that holds
// the Network Extension entitlement TunnelExtension needs but can't get under personal-team
// signing. Selected instead of TunnelManager when SOGEN_IOS_USE_LOCALDEVVPN is set (project.yml).
// See docs/superpowers/specs/2026-09-16-ios-localdevvpn-design.md.
enum LocalDevVPNResult {
    case connected
    case notInstalled
    case failed(String)
}

final class LocalDevVPNManager {
    static let shared = LocalDevVPNManager()

    private static let probeURL = URL(string: "localdevvpn://")!
    private static let launchURL = URL(string: "localdevvpn://enable?scheme=sogenios")!
    private static let callbackTimeout: TimeInterval = 8

    private var pendingCompletion: ((LocalDevVPNResult) -> Void)?
    private var timeoutWorkItem: DispatchWorkItem?

    private init() {}

    static func ensureConnected(completion: @escaping (LocalDevVPNResult) -> Void) {
        guard UIApplication.shared.canOpenURL(probeURL) else {
            completion(.notInstalled)
            return
        }

        shared.pendingCompletion = completion

        let timeoutItem = DispatchWorkItem {
            shared.resolve(.failed("LocalDevVPN did not confirm the tunnel in time"))
        }
        shared.timeoutWorkItem = timeoutItem
        DispatchQueue.main.asyncAfter(deadline: .now() + callbackTimeout, execute: timeoutItem)

        UIApplication.shared.open(launchURL)
    }

    // Called from SogenApp's onOpenURL handler (see Task 3) when a "sogenios://" callback arrives.
    static func handleCallback() {
        shared.resolve(.connected)
    }

    private func resolve(_ result: LocalDevVPNResult) {
        timeoutWorkItem?.cancel()
        timeoutWorkItem = nil
        guard let completion = pendingCompletion else { return }
        pendingCompletion = nil
        completion(result)
    }
}
```

- [ ] **Step 2: Build to confirm it compiles**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`. (This builds the default developer-signed variant, so `LocalDevVPNManager` compiles but isn't referenced by `JITGateOrchestrator` yet — that's Task 4. This step only confirms the file itself is syntactically and semantically valid Swift.)

- [ ] **Step 3: Commit**

```bash
git add tools/sogen-ios/Sources/JIT/LocalDevVPNManager.swift
git commit -m "feat(ios): add LocalDevVPNManager for the LocalDevVPN JIT-grant tunnel"
```

---

### Task 3: Route the `sogenios://` callback

**Goal:** Wire LocalDevVPN's completion callback (`sogenios://`) to `LocalDevVPNManager.handleCallback()`.

**Files:**
- Modify: `tools/sogen-ios/Sources/SogenApp.swift`

**Acceptance Criteria:**
- [ ] Opening a `sogenios://` URL while the app is running calls `LocalDevVPNManager.handleCallback()`.
- [ ] Any other URL scheme is ignored (no crash, no unintended side effect).
- [ ] Builds cleanly.

**Verify:** Build succeeds (Step 2's command from Task 2). Full behavioral verification is on-device only (Task 6), since it requires a real `sogenios://` open event from LocalDevVPN.

**Steps:**

- [ ] **Step 1: Add the `onOpenURL` handler**

Current `tools/sogen-ios/Sources/SogenApp.swift`:

```swift
import SwiftUI

@main
struct SogenApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
```

Change to:

```swift
import SwiftUI

@main
struct SogenApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .onOpenURL { url in
                    if url.scheme == "sogenios" {
                        LocalDevVPNManager.handleCallback()
                    }
                }
        }
    }
}
```

- [ ] **Step 2: Build to confirm it compiles**

Same command as Task 2 Step 2. Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 3: Commit**

```bash
git add tools/sogen-ios/Sources/SogenApp.swift
git commit -m "feat(ios): route the sogenios:// callback to LocalDevVPNManager"
```

---

### Task 4: `JITGateOrchestrator` — select the tunnel mechanism, richer result type

**Goal:** `JITGateOrchestrator.run` uses `TunnelManager` or `LocalDevVPNManager` based on `SOGEN_IOS_USE_LOCALDEVVPN`, and reports a three-way `JITGateResult` instead of today's plain `(Bool, String)`, so callers can distinguish "LocalDevVPN not installed" from any other failure.

**Files:**
- Modify: `tools/sogen-ios/Sources/JIT/JITGate.swift`

**Acceptance Criteria:**
- [ ] New `JITGateResult` enum: `.success`, `.tunnelNotInstalled`, `.failed(String)`.
- [ ] `JITGateOrchestrator.run`'s `completion` parameter type changes to `(JITGateResult) -> Void`.
- [ ] With `SOGEN_IOS_USE_LOCALDEVVPN` unset (default/developer-signed): behavior is identical to today except for the completion type — `TunnelManager.ensureConnected` still gates the tunnel step, `.tunnelNotInstalled` is never produced.
- [ ] With `SOGEN_IOS_USE_LOCALDEVVPN` set: `LocalDevVPNManager.ensureConnected` gates the tunnel step instead; its three-way result maps onto `JITGateResult` per the design spec (`.connected`→proceed, `.notInstalled`→`.tunnelNotInstalled`, `.failed`→`.failed`).
- [ ] Everything after a successful tunnel (`JITCoordinator.enableJIT`, `JITGate.prepareRegion`/`verifyRegion`) is unchanged in both branches — extracted into one shared local function, not duplicated.
- [ ] `JITGateOrchestrator.runXcodeDebuggerBypass`'s completion type is updated to `JITGateResult` too, for consistency (it never produces `.tunnelNotInstalled`).

**Verify:** Build succeeds with `SOGEN_IOS_USE_LOCALDEVVPN` **unset** (default state) — confirms the `#else` branch and the rest of the app still compile untouched. Then, temporarily add `SOGEN_IOS_USE_LOCALDEVVPN=1` to `project.yml`'s `GCC_PREPROCESSOR_DEFINITIONS` array, regenerate, and build again to confirm the `#if` branch compiles too — then revert that temporary change (the committed state must stay developer-signed-by-default per Task 1).

**Steps:**

- [ ] **Step 1: Replace `JITGateOrchestrator` in `JITGate.swift`**

Replace the entire `enum JITGateOrchestrator { ... }` block (currently lines 85-153) with:

```swift
enum JITGateResult {
    case success
    case tunnelNotInstalled
    case failed(String)
}

enum JITGateOrchestrator {
    // End-to-end sequence ported from the spike's ContentView.attemptBuiltInJIT/
    // startBuiltInJIT (Test 5): tunnel up, then the concurrent enableJIT/prepareRegion
    // pair, then verify. Calls completion(.success) only once the JIT26 region has actually
    // been proven executable; completion(.failed(reason)) otherwise, and the emulator must
    // not be started in that case. completion(.tunnelNotInstalled) is only reachable when
    // SOGEN_IOS_USE_LOCALDEVVPN is set and LocalDevVPN isn't installed on the device.
    //
    // Which tunnel mechanism gates this is a build-time choice (see project.yml's
    // SOGEN_IOS_USE_LOCALDEVVPN comment and docs/superpowers/specs/2026-09-16-ios-localdevvpn-
    // design.md) -- there is no runtime auto-detection or fallback between the two.
    //
    // JITCoordinator.enableJIT's XPC reply arrives almost immediately (an ack that the JIT26
    // debug session has started, not that it's done -- see JITHelperExtension.swift's own
    // comment: universal.js's watch loop keeps running, attached, for the rest of the process's
    // life in the success case, same as before). So completion here is still driven purely by
    // the *local* self-test (prepareRegion + verifyRegion) succeeding, not by enableJIT's XPC
    // reply. What changed is visibility: onProgressLine below receives every one of universal.js's
    // own log() lines for as long as the session stays attached (polled over the same XPC
    // connection -- see JITCoordinator.swift), not just this one ack.
    static func run(pairingData: Data, log: @escaping (String) -> Void, completion: @escaping (JITGateResult) -> Void) {
        log("[jit] starting tunnel extension ...")

        func proceedAfterTunnel() {
            log("[jit] calling JITHelper (XPC) -- preparing the JIT26 region concurrently, not after")

            JITCoordinator.enableJIT(
                targetPID: getpid(), pairingData: pairingData,
                onProgressLine: log,
                completion: { success, message in
                    log("[jit] JITHelper acknowledged the enableJIT request (success=\(success)): \(message)")
                })

            DispatchQueue.global(qos: .userInitiated).async {
                guard let region = JITGate.prepareRegion(log: log) else {
                    completion(.failed("JIT26 self-test region prepare failed"))
                    return
                }
                JITGate.verifyRegion(region, log: log)
                log("[jit] JIT26 self-test verified; debug session stays attached for Unicorn")
                completion(.success)
            }
        }

#if SOGEN_IOS_USE_LOCALDEVVPN
        LocalDevVPNManager.ensureConnected { result in
            switch result {
            case .connected:
                log("[jit] tunnel up (LocalDevVPN)")
                proceedAfterTunnel()
            case .notInstalled:
                log("[jit] LocalDevVPN is not installed")
                completion(.tunnelNotInstalled)
            case .failed(let message):
                log("[jit] tunnel FAILED (LocalDevVPN): \(message)")
                completion(.failed("tunnel failed to start: \(message)"))
            }
        }
#else
        TunnelManager.ensureConnected { tunnelUp, tunnelMessage in
            log("[jit] tunnel \(tunnelUp ? "up" : "FAILED"): \(tunnelMessage)")
            guard tunnelUp else {
                completion(.failed("tunnel failed to start: \(tunnelMessage)"))
                return
            }
            proceedAfterTunnel()
        }
#endif
    }

    // TEMPORARY DIAGNOSTIC BYPASS -- do not use for the shipping app. Added for the pivot to
    // debugging Unicorn's real TCG execution bug via Xcode's own attached LLDB session instead
    // of our embedded JITHelper/tunnel flow: two debuggers can't attach to the same process, so
    // this skips ever starting our own JIT26 debug session (TunnelManager/JITCoordinator above)
    // entirely. Xcode's own debugger attaching over USB during a normal Run sets CS_DEBUGGED the
    // same way our embedded session does, so JITGate.prepareRegion's existing poll/prepare/
    // verify sequence below is reused completely unchanged -- only the tunnel/XPC machinery that
    // spins up OUR OWN debug session is skipped. See ContentView.attemptBoot for the
    // SOGEN_JIT26_XCODE_DEBUG_BYPASS environment variable that selects this path. Remove once the
    // real bug is found and go back to run() above for the shipping self-contained flow.
    static func runXcodeDebuggerBypass(log: @escaping (String) -> Void, completion: @escaping (JITGateResult) -> Void) {
        log("[jit] SOGEN_JIT26_XCODE_DEBUG_BYPASS set -- skipping the embedded tunnel/JITHelper " +
            "flow, waiting for Xcode's own debugger to attach instead")
        DispatchQueue.global(qos: .userInitiated).async {
            guard let region = JITGate.prepareRegion(log: log) else {
                completion(.failed("JIT26 self-test region prepare failed"))
                return
            }
            JITGate.verifyRegion(region, log: log)
            log("[jit] JIT26 self-test verified (Xcode-debugger bypass path)")
            completion(.success)
        }
    }
}
```

- [ ] **Step 2: Build with `SOGEN_IOS_USE_LOCALDEVVPN` unset (default state)**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: fails at this point, because `ContentView.swift` (Task 5) still calls the old `(Bool, String)` completion signature. **This is expected until Task 5 lands** — do not treat this failure as a Task 4 bug; proceed to Task 5 in the same session before considering Task 4 verified. (If you want an isolated compile check of just this file's own syntax before Task 5, temporarily comment out `ContentView.swift`'s two `JITGateOrchestrator` call sites' completion closures' bodies — but this is optional; Task 5 is next regardless.)

- [ ] **Step 3: Commit**

```bash
git add tools/sogen-ios/Sources/JIT/JITGate.swift
git commit -m "feat(ios): JITGateOrchestrator selects TunnelManager or LocalDevVPNManager, richer result type"
```

---

### Task 5: `ContentView` — handle `JITGateResult`, add the LocalDevVPN install prompt

**Goal:** `ContentView.attemptBoot`'s two completion handlers switch on `JITGateResult`; a new UI state shows an "Install LocalDevVPN" / "Retry" prompt for `.tunnelNotInstalled`.

**Files:**
- Modify: `tools/sogen-ios/Sources/ContentView.swift`

**Acceptance Criteria:**
- [ ] `.success` → `startEmulator(with: layer)`, exactly as `success == true` does today.
- [ ] `.failed(message)` → same on-screen `ERROR: ...` line as today's `success == false` path, unchanged wording/behavior.
- [ ] `.tunnelNotInstalled` → sets a new state that shows "Install LocalDevVPN" (opens the App Store listing) and "Retry" (re-attempts boot) buttons; only reachable when `SOGEN_IOS_USE_LOCALDEVVPN` is set, but the UI code itself isn't `#if`-gated (it's simply never triggered in the developer-signed build, matching how other unreachable-in-this-variant states already behave elsewhere in this file).
- [ ] Builds cleanly; developer-signed default build's visible behavior is unchanged (never shows the new buttons, since `.tunnelNotInstalled` is never produced on that path).

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Add the new state**

In `tools/sogen-ios/Sources/ContentView.swift`, add alongside the existing `@State` declarations (currently lines 5-11):

```swift
    @State private var needsLocalDevVPNInstall = false
```

- [ ] **Step 2: Add the Install/Retry buttons**

In `body`'s `HStack` (currently lines 59-70), add a conditional pair of buttons after the existing two:

```swift
            HStack {
                Button("Download Emulation Root") {
                    rootReady = false
                    ensureEmulationRoot()
                }
                .padding(6)
                Button("Check for Pairing File") {
                    checkForPairingFile()
                }
                .padding(6)
                if needsLocalDevVPNInstall {
                    Button("Install LocalDevVPN") {
                        if let url = URL(string: "https://apps.apple.com/us/app/localdevvpn/id6755608044") {
                            UIApplication.shared.open(url)
                        }
                    }
                    .padding(6)
                    Button("Retry") {
                        needsLocalDevVPNInstall = false
                        bootAttempted = false
                        attemptBoot()
                    }
                    .padding(6)
                }
                Spacer()
            }
```

- [ ] **Step 3: Update the Xcode-debugger-bypass completion handler**

Currently (lines 209-220):

```swift
            JITGateOrchestrator.runXcodeDebuggerBypass(
                log: { line in DispatchQueue.main.async { appendLog(line) } },
                completion: { success, message in
                    DispatchQueue.main.async {
                        if success {
                            startEmulator(with: layer)
                        } else {
                            appendLog("ERROR: JIT grant failed (Xcode-debugger bypass path), " +
                                      "refusing to start the guest: \(message)")
                        }
                    }
                })
```

Change to:

```swift
            JITGateOrchestrator.runXcodeDebuggerBypass(
                log: { line in DispatchQueue.main.async { appendLog(line) } },
                completion: { result in
                    DispatchQueue.main.async {
                        switch result {
                        case .success:
                            startEmulator(with: layer)
                        case .tunnelNotInstalled:
                            // Unreachable on this path -- runXcodeDebuggerBypass never touches
                            // TunnelManager/LocalDevVPNManager -- but the switch must be exhaustive.
                            appendLog("ERROR: unexpected tunnelNotInstalled on the Xcode-debugger bypass path")
                        case .failed(let message):
                            appendLog("ERROR: JIT grant failed (Xcode-debugger bypass path), " +
                                      "refusing to start the guest: \(message)")
                        }
                    }
                })
```

- [ ] **Step 4: Update the main completion handler**

Currently (lines 241-250):

```swift
            completion: { success, message in
                DispatchQueue.main.async {
                    if success {
                        startEmulator(with: layer)
                    } else {
                        appendLog("ERROR: JIT grant failed, refusing to start the guest " +
                                  "(it would crash on the first new Unicorn JIT translation): \(message)")
                    }
                }
            })
```

Change to:

```swift
            completion: { result in
                DispatchQueue.main.async {
                    switch result {
                    case .success:
                        startEmulator(with: layer)
                    case .tunnelNotInstalled:
                        needsLocalDevVPNInstall = true
                        appendLog("[jit] LocalDevVPN is required for this build's JIT-grant tunnel -- " +
                                  "tap \"Install LocalDevVPN\" below, install it from the App Store, " +
                                  "then tap \"Retry\"")
                    case .failed(let message):
                        appendLog("ERROR: JIT grant failed, refusing to start the guest " +
                                  "(it would crash on the first new Unicorn JIT translation): \(message)")
                    }
                }
            })
```

- [ ] **Step 5: Build to confirm it compiles**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`. This confirms Task 4 + Task 5 together compile in the default (developer-signed, `SOGEN_IOS_USE_LOCALDEVVPN` unset) state.

- [ ] **Step 6: Confirm the `SOGEN_IOS_USE_LOCALDEVVPN` branch also compiles, then revert**

Temporarily edit `tools/sogen-ios/project.yml`'s `GCC_PREPROCESSOR_DEFINITIONS` to `["SOGEN_IOS_USE_FEX=1", "SOGEN_IOS_USE_LOCALDEVVPN=1"]` and comment out the `TunnelExtension` dependency (both lines), then:

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`, confirming the `#if SOGEN_IOS_USE_LOCALDEVVPN` branch in `JITGate.swift` and the new `LocalDevVPNManager` are wired correctly end-to-end at compile time. Then **revert** `project.yml` back to the default (developer-signed) state — `git checkout tools/sogen-ios/project.yml` if the earlier `xcodegen generate` hasn't been committed yet, or manually undo the two edits — and regenerate once more (`xcodegen generate`) so the working tree matches the default state before committing.

- [ ] **Step 7: Commit**

```bash
git add tools/sogen-ios/Sources/ContentView.swift
git commit -m "feat(ios): handle JITGateResult in ContentView, add LocalDevVPN install prompt"
```

---

### Task 6: Real-device verification (both build variants)

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in the acceptance criteria has been re-validated independently, with output captured.

**Goal:** Confirm both build variants actually work on real hardware — this is real-device-only functionality (a genuine `NEPacketTunnelProvider`/URL-scheme handshake) that cannot be meaningfully verified any other way.

**Files:** None (verification only — uses the build artifacts from Tasks 1-5).

**Acceptance Criteria:**
- [ ] Self-signed build (`SOGEN_IOS_USE_LOCALDEVVPN=1`, `TunnelExtension` dependency commented out), LocalDevVPN **not** installed on the test device: launching the app and reaching the JIT-grant step shows the "Install LocalDevVPN" / "Retry" buttons, not a raw tunnel-failure `ERROR:` line.
- [ ] Install LocalDevVPN from the App Store, tap "Retry": the `localdevvpn://enable?scheme=sogenios` → `sogenios://` round trip completes, the JIT grant proceeds through `JITCoordinator.enableJIT`/`JITGate.prepareRegion`/`verifyRegion`, and the guest boots to the same `[ngcs] done` bar already established for this app (all 8 frames render, matching the color sequence documented in `tools/sogen-ios/README.md`'s "Run" section).
- [ ] Developer-signed build (default `project.yml` state, unchanged from before this plan): still boots via `TunnelExtension` exactly as before — regression check, not a new behavior.

**Verify:** Manual, on-device, for each of the three bullet points above. Capture the on-screen log (or a `devicectl device copy from` pull of `sogen_log.txt`, per this app's existing evidence-capture convention) for each.

**Steps:**

- [ ] **Step 1: Build and deliver the self-signed variant**

With `project.yml` in the self-signed state (`SOGEN_IOS_USE_LOCALDEVVPN=1` set, `TunnelExtension` dependency commented out — same edit as Task 5 Step 6, but this time NOT reverted), build and package an unsigned `.ipa` following this project's established recipe (`xcodebuild archive` with `CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" CODE_SIGN_STYLE=Manual DEVELOPMENT_TEAM="" PROVISIONING_PROFILE_SPECIFIER="" AD_HOC_CODE_SIGNING_ALLOWED=YES`, confirm `NOUNDEFS` via `otool -hv`, package `Payload/SogenIOS.app` into a `.ipa`), and deliver it to the user for resigning/installing exactly as every prior real-device round in this project has.

- [ ] **Step 2: User tests without LocalDevVPN installed**

User confirms the "Install LocalDevVPN" / "Retry" prompt appears (first acceptance bullet).

- [ ] **Step 3: User installs LocalDevVPN and retries**

User confirms the full round trip and successful guest boot (second acceptance bullet), and shares the resulting log.

- [ ] **Step 4: Regression-check the developer-signed build**

Revert `project.yml` to its default (developer-signed) state, rebuild/redeliver, and confirm it still boots via `TunnelExtension` unchanged (third acceptance bullet).

- [ ] **Step 5: Commit**

No source changes are expected in this task. If `project.yml` was left in a non-default state at any point, confirm `git status`/`git diff` shows it back at the default (developer-signed) state before finishing — do not commit a self-signed default.

```json:metadata
{"userGate": true, "tags": ["user-gate"], "files": [], "verifyCommand": "manual on-device", "acceptanceCriteria": ["self-signed build without LocalDevVPN shows Install/Retry prompt, not a raw ERROR: line", "installing LocalDevVPN and retrying completes the tunnel handshake and reaches [ngcs] done", "developer-signed build still boots via TunnelExtension unchanged"], "modelTier": "standard"}
```

---

## Execution Handoff
