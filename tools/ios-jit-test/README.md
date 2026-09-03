# iOS JIT capability test

Standalone SwiftUI app to check whether this device/iOS version/signing setup
can execute JIT'd code, before sinking time into an iOS port of the FEXCore
backend. Not part of the sogen build — a throwaway spike.

## Build

```sh
brew install xcodegen   # once
xcodegen generate
open JITTest.xcodeproj
```

Select your team under Signing & Capabilities, then build/install once via
Xcode to catch any compile errors — Xcode's console gives real crash logs.

## Running the actual test

Do **not** read Test 1's result while running under Xcode's debugger — lldb
auto-attaches on `Cmd+R`, which sets `CS_DEBUGGED` before the app even
starts, so a Test-1 "success" there doesn't mean anything.

For a real read:
1. `Product > Archive`, export a Development-signed `.ipa`.
2. Sideload it fresh via Feather with the ArcticSign cert (the real
   deployment path).
3. Launch from the Home Screen (not via Xcode), tap **Test 1** — expect
   this to fail or kill the app outright. If it kills the app, that's the
   expected negative baseline; reopen the app to see the persisted log.
4. Relaunch fresh, tap **Test 2** (fork+ptrace self-attach, then JIT). This
   is the result that actually matters: if it logs "EXECUTION SUCCEEDED",
   self-JIT works under this cert/iOS version with no external tool needed.
   If it fails or kills the app, fall back to Feather's own "Enable JIT" /
   StikDebug before the app launches.

The log persists to a file in the app's Documents directory across
launches (including hard kills), and is shown in-app on every launch.

## Reference

StikDebug source, if the self-attach trick needs replacing with the
tunnel-based debugserver approach: https://github.com/StikDebug/StikDebug
