# LocalDevVPN Support Design

**Goal:** Give the self-signed/sideloaded build of the sogen iOS app a working JIT-grant tunnel, since its current mechanism (`TunnelExtension`, an `NEPacketTunnelProvider`) requires the Network Extension entitlement that personal/free Apple developer accounts cannot obtain.

**Context:** The app's JIT-grant flow (`JITGateOrchestrator.run`, `tools/sogen-ios/Sources/JIT/JITGate.swift`) requires a local loopback network route before it can complete the JIT26 handshake. Today that route comes exclusively from the app's own `TunnelExtension` target via `TunnelManager.ensureConnected` (`tools/sogen-ios/Sources/JIT/TunnelManager.swift`). Per `tools/sogen-ios/README.md`'s "JIT grant (Task 8)" section, `TunnelExtension` cannot be signed under a personal-team Apple developer account — Xcode refuses to create a provisioning profile for it, confirmed with a real first-party error message. The Phase 1 resolution was to drop `TunnelExtension`'s embed from the personal-team build entirely, leaving that build with no working tunnel mechanism at all.

**LocalDevVPN** (`https://github.com/jkcoxson/LocalDevVPN`, App Store: `https://apps.apple.com/us/app/localdevvpn/id6755608044`) is a real, open-source, App-Store-distributed app by jkcoxson (idevice/SideStore-adjacent ecosystem, MIT-licensed successor to StosVPN) that holds the Network Extension entitlement itself, under its own paid-account signing. It exposes a URL-scheme automation hook (`localdevvpn://enable?scheme=<callback>`) that a consuming app can use to request the tunnel be brought up and be notified via a callback URL once it is. Its default tunnel addressing (`10.7.1.1`/`10.7.0.1`) already matches the address `TunnelManager.swift`'s own header comment documents as what `JITHelper` expects (`10.7.0.1:49152`), so the two mechanisms are drop-in compatible at the network layer.

Whether an installed build has a working `TunnelExtension` is a **build-time**, not runtime, fact — Apple gives no distinguishable runtime error code for "this app lacks the Network Extension entitlement" (an unentitled provider extension simply cannot load at all). So this is not an auto-detection problem; it's a build-configuration decision made once, before shipping either variant.

**User decisions (already made):**
- Two build configurations, chosen at build time: **developer-signed** (has a paid account that can actually provision `TunnelExtension`) keeps using `TunnelExtension` as today; **self-signed/sideload** (no such account) always uses LocalDevVPN instead. No runtime auto-detection or fallback between the two.
- `TunnelExtension`'s target is dropped entirely from the self-signed build (matching the existing Phase 1 precedent), not kept around unused.
- No settings UI is built in this round. A user-facing toggle between `TunnelExtension`/LocalDevVPN (and, separately, between the FEX/Unicorn backends) is deferred to the later full settings-screen/profiles UI overhaul. This design only wires up the two build-time paths.
- If LocalDevVPN is not installed on the self-signed build's device, show a plain "LocalDevVPN required — tap to open the App Store, then relaunch" message; no more elaborate guided install flow for now.

---

## Architecture

A compile-time flag (mirroring how `SOGEN_IOS_USE_FEX` already selects the emulation backend today, via a `GCC_PREPROCESSOR_DEFINITIONS` entry in `tools/sogen-ios/project.yml`) selects which tunnel mechanism a given build uses:

- `SOGEN_IOS_USE_LOCALDEVVPN=1` (self-signed build): `TunnelExtension` target/embed removed from `project.yml` for this configuration; `JITGateOrchestrator.run` uses the new `LocalDevVPNManager` for its tunnel step.
- Absent (developer-signed build, default): unchanged from today — `TunnelExtension` embedded, `TunnelManager.ensureConnected` used as the tunnel step.

The two managers present the same shape to `JITGateOrchestrator`, so nothing else in the orchestration (`JITCoordinator.enableJIT`'s XPC call, `JITGate.prepareRegion`/`verifyRegion`'s self-test) changes based on which build this is — only the single call at the top of `JITGateOrchestrator.run` differs, gated by `#if SOGEN_IOS_USE_LOCALDEVVPN`.

## Component: `LocalDevVPNManager`

New file, `tools/sogen-ios/Sources/JIT/LocalDevVPNManager.swift`, exposing an entry point shaped like `TunnelManager.ensureConnected` but returning a three-way result instead of today's plain `(Bool, String)`:

```swift
enum LocalDevVPNResult {
    case connected
    case notInstalled
    case failed(String)   // covers timeout and any other failure to receive the callback
}

final class LocalDevVPNManager {
    static func ensureConnected(completion: @escaping (LocalDevVPNResult) -> Void)
}
```

Behavior:
1. **Install check.** `UIApplication.shared.canOpenURL(URL(string: "localdevvpn://")!)`. This requires declaring `localdevvpn` under `LSApplicationQueriesSchemes` in the app's `Info.plist`/`project.yml`. If this returns `false`, call back with `.notInstalled` immediately — no network attempt is made.
2. **Handshake.** If installed, register sogen's own callback URL scheme (`sogenios`, added under `CFBundleURLTypes` in `project.yml`) if not already registered, store the pending completion, then call `UIApplication.shared.open(URL(string: "localdevvpn://enable?scheme=sogenios")!)`.
3. **Callback.** `SogenIOSApp`'s (or `ContentView`'s) `onOpenURL` handler recognizes a `sogenios://` URL, and if a `LocalDevVPNManager` completion is pending, resolves it with `.connected`.
4. **Timeout.** If no callback arrives within 8 seconds of step 2, resolve the pending completion with `.failed("LocalDevVPN did not confirm the tunnel in time")` instead of waiting indefinitely.

`JITGateOrchestrator.run`'s own completion contract changes from today's plain `(Bool, String)` to `(JITGateResult) -> Void`, where:

```swift
enum JITGateResult {
    case success
    case tunnelNotInstalled          // LocalDevVPN-only; TunnelExtension path never produces this
    case failed(String)
}
```

Internally, `#if SOGEN_IOS_USE_LOCALDEVVPN`: `LocalDevVPNManager.ensureConnected`'s `.connected`/`.notInstalled`/`.failed(reason)` map 1:1 onto `.success`/`.tunnelNotInstalled`/`.failed(reason)`. `#else` (developer-signed build): `TunnelManager.ensureConnected`'s existing `(Bool, String)` maps `true` → `.success`, `false` → `.failed(tunnelMessage)` (never `.tunnelNotInstalled`, since that case is meaningless for `TunnelExtension`). Either way, once the tunnel step resolves to `.success`, the rest of `JITGateOrchestrator.run` (the `JITCoordinator.enableJIT` XPC call and `JITGate.prepareRegion`/`verifyRegion`) proceeds completely unchanged. `ContentView.attemptBoot`'s completion handler switches on `JITGateResult` directly: `.tunnelNotInstalled` shows the App-Store-prompt UI, `.failed` shows the existing on-screen `ERROR:` line, `.success` proceeds to `startEmulator` exactly as today.

## Data flow (self-signed build)

```
ContentView.attemptBoot
  -> JITGateOrchestrator.run
    -> LocalDevVPNManager.ensureConnected
      -> canOpenURL("localdevvpn://") == false?
           -> .notInstalled -> ContentView shows "install LocalDevVPN" prompt (App Store link), stops here
      -> canOpenURL == true:
           -> open("localdevvpn://enable?scheme=sogenios")
           -> LocalDevVPN activates its own NEPacketTunnelProvider
           -> LocalDevVPN opens "sogenios://" back to sogen
           -> onOpenURL resolves the pending completion -> .connected
      -> (no callback within 8s) -> .failed(...) -> ContentView shows existing ERROR: line, retry-able
    -> (on .connected) JITCoordinator.enableJIT (XPC) + JITGate.prepareRegion/verifyRegion
                        -- unchanged from the TunnelExtension path
    -> success -> startEmulator (unchanged)
```

## Error handling

Three distinct outcomes replace today's single boolean:
- **`.connected`** — proceed exactly as today's success path.
- **`.notInstalled`** — new, dedicated UI: a plain message with a button/link opening the LocalDevVPN App Store page, and a way to retry `attemptBoot` after the user has installed it and come back.
- **`.failed(reason)`** — routes through the existing on-screen `appendLog("ERROR: ...")` convention (`ContentView.swift:241-250` today), same as any other JIT-grant failure; the user can retry manually (e.g. relaunch, or a future retry button — no new retry mechanism beyond what already exists for tunnel failures today).

No automatic fallback between mechanisms exists anywhere in this design — which mechanism a build uses is fixed at compile time.

## Testing

Real-device only; LocalDevVPN is a genuine `NEPacketTunnelProvider` and Simulator already skips the whole JIT-grant flow (`ContentView.attemptBoot`'s existing Simulator branch). Manual verification:
1. Self-signed build, LocalDevVPN not installed on the test device → confirms the `.notInstalled` App-Store-prompt path appears instead of a raw tunnel-failure error.
2. Install LocalDevVPN, relaunch/retry → confirms the `localdevvpn://enable` → `sogenios://` round trip resolves `.connected`, and the JIT grant proceeds through `JITCoordinator.enableJIT`/`prepareRegion`/`verifyRegion` to a successful guest boot, exactly as the existing `TunnelExtension` path already does.
3. Developer-signed build → regression check only: still boots via `TunnelExtension` unchanged.
