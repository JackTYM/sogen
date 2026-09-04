# iOS JIT capability test

Standalone SwiftUI app to check whether this device/iOS version/signing setup
can execute JIT'd code, before sinking time into an iOS port of the FEXCore
backend. Not part of the sogen build — a throwaway spike.

Confirmed live on an iPhone 16 Pro Max, iOS 26: raw `mmap(MAP_JIT)` and a
fork+ptrace self-attach are both blocked (`EPERM`), even under a real
`CS_DEBUGGED` grant from StikDebug — this device's TXM/SPTM enforces a
`brk #0xf00d` breakpoint negotiation protocol per executable region. Test 4
implements that protocol (`JIT26.c`/`JIT26.h`) and successfully executes
JIT'd code end to end.

## Architecture

- `JITTest` — host app. Tests 1-4 (see below), plus pairing-file import and
  the Built-in-JIT trigger (Test 5).
- `JITHelper` — an ExtensionKit `extensionkit-extension` embedding StikJIT,
  reached from the host via `AppExtensionPoint`/`AppExtensionProcess`/
  `XPCSession` (iOS 26+ only). Runs the actual `StikJIT.enableJIT(...)` call
  against the host app's PID.
- `TunnelExtension` — a `NEPacketTunnelProvider` app extension, so the app
  doesn't need a separately-installed VPN app (LocalDevVPN/StosVPN) to reach
  the device's own developer services.
- `Vendor/StikJIT-src` — StikJIT (MPL-2.0) vendored from source, not the
  prebuilt xcframework release — the release's private Swift module
  interface was built with an older toolchain than Xcode 26.6 ships, and
  private interfaces aren't guaranteed portable across compiler versions.
  Building locally sidesteps that.

**NetworkExtension entitlement**: `TunnelExtension` needs
`com.apple.developer.networking.networkextension` (`packet-tunnel-provider`),
which Apple does not grant to free personal-team accounts — confirmed via a
real build error ("Personal development teams... do not support the Network
Extensions capability"). ArcticSign's Developer profile already has this
approved (verified by inspecting its embedded entitlements directly), so
this only builds fully signed through that cert, not the personal team used
for Tests 1-4.

## License attribution

- `TunnelExtension/PacketTunnelProvider.swift` is adapted from
  [SideStore/StosVPN](https://github.com/SideStore/StosVPN)'s `TunnelProv`
  (MIT, with a clause requiring attribution and prohibiting rebranding the
  whole project — this is a small adapted component, not a redistribution).
- `Vendor/StikJIT-src` is [StikDebug/StikJIT](https://github.com/StikDebug/StikJIT)
  (MPL-2.0), vendored unmodified.
- The ExtensionKit host/helper architecture (`JITCoordinator.swift`,
  `JITExtensionPoint.swift`, `HelperExtension/JITHelperExtension.swift`) was
  written from scratch, using [EricoEC/PocketJLauncher](https://github.com/EricoEC/PocketJLauncher)
  (GPL-3.0) purely as a reference for understanding the target-type/settings
  pattern (`extensionkit-extension`, `EXAppExtensionAttributes`,
  `AppExtensionPoint`/`XPCSession` usage) — no GPL-3.0 code was copied, to
  avoid a licensing question given sogen itself is GPL-2.0.

## Build

```sh
brew install xcodegen   # once
xcodegen generate
open JITTest.xcodeproj
```

Select your team under Signing & Capabilities, then build/install once via
Xcode to catch any compile errors — Xcode's console gives real crash logs.

Tests 1-4 only need `get-task-allow` and build fine under a free personal
team. Test 5 (Built-in JIT) needs the full app — host + both extensions —
signed with a cert whose account has NetworkExtension approved (ArcticSign
confirmed; a personal team will fail on `TunnelExtension`). To produce an
installable build without a personal-team NetworkExtension entitlement:

```sh
xcodebuild -project JITTest.xcodeproj -scheme JITTest -sdk iphoneos \
  -configuration Release CODE_SIGNING_ALLOWED=NO VALIDATE_PRODUCT=NO build
```

(`VALIDATE_PRODUCT=NO` matters — Release's default `VALIDATE_PRODUCT=YES`
silently strips unsigned embedded extensions from the built `.app` instead
of failing loudly.) Package the resulting `.app` as `Payload/JITTest.app`
inside a zip renamed to `.ipa`, then sideload via Feather with a cert whose
account has NetworkExtension approved — Feather registers the host's and
both extensions' bundle IDs against that account automatically.

## Running the actual test

Do **not** read Test 1's result while running under Xcode's debugger — lldb
auto-attaches on `Cmd+R`, which sets `CS_DEBUGGED` before the app even
starts, so a Test-1 "success" there doesn't mean anything.

For a real read, sideload fresh via Feather (not Xcode) and launch from the
Home Screen:

1. **Test 1** (no debugger) — expect `EPERM` on `mmap(MAP_JIT)`, or the app
   gets killed outright. Either is the expected negative baseline.
2. **Test 2** (fork+ptrace self-attach) — expect `EPERM` on the `ptrace()`
   call itself; Apple has tightened this over iOS versions specifically to
   break naive self-attach.
3. **Open StikDebug (Enable JIT)**, confirm there, return, then **Test 3**
   (plain JIT attempt) — still `EPERM` if TXM/SPTM is present on this
   device, even with real `CS_DEBUGGED`.
4. Same StikDebug round-trip, then **Test 4** (JIT26 protocol) — this is the
   one that actually works: `jit26_prepare_region` gets an RX region from
   the debug server, `mach_vm_remap`+`mprotect` builds a writable alias,
   write the code, `jit26_detach`, execute.
5. **Import Pairing File**, then **Test 5** (Built-in JIT) — routes through
   the in-app `JITHelper` extension instead of the external StikDebug app.
   Needs the full ArcticSign-signed build (see above) and `TunnelExtension`
   actually running so the helper can reach the device's developer services
   without a separately-installed VPN app.

The log persists to a file in the app's Documents directory across
launches (including hard kills), and is shown in-app on every launch. Pull
it from a dev machine without unlocking the phone via:

```sh
xcrun devicectl device copy from --device <udid> \
  --domain-type appDataContainer --domain-identifier com.jacksonyarger.jittest \
  --source Documents/jit_test_log.txt --destination ./jit_test_log.txt
```

## Reference

- StikDebug: https://github.com/StikDebug/StikDebug
- StikJIT + integration guide: https://github.com/StikDebug/StikJIT
- StosVPN: https://github.com/SideStore/StosVPN
- Pairing files (iloader): https://github.com/StikDebug/StikDebug-Guide/blob/main/pairing_file.md
