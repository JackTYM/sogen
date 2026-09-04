import Darwin

func attemptJIT(label: String) {
    let size = 16384
    JITLog.append("[\(label)] mmap(MAP_JIT) ...")

    var mmapErrno: Int32 = 0
    guard let region = jit_mmap_region(size, &mmapErrno) else {
        JITLog.append("[\(label)] mmap FAILED errno=\(mmapErrno) (\(String(cString: strerror(mmapErrno))))")
        return
    }
    JITLog.append("[\(label)] mmap OK at \(region)")

    jit_toggle_write(0)
    let word = region.assumingMemoryBound(to: UInt32.self)
    word.pointee = 0xD65F_03C0 // AArch64 RET
    jit_toggle_write(1)
    jit_invalidate_icache(region, size)

    JITLog.append("[\(label)] wrote RET, about to execute JIT'd code ...")

    jit_execute(region)

    JITLog.append("[\(label)] EXECUTION SUCCEEDED — JIT works")
    jit_munmap_region(region, size)
}

let jit26RegionSize = 16384

// Calls the JIT26 breakpoint protocol (prepare region, write through a
// writable alias, detach). Must run concurrently with whatever debugger is
// attached and waiting for these brk calls — universal.js's wait loop
// (`while (!detached) { send_command('c') ... }`) blocks until this runs, so
// calling this only *after* the attach completes deadlocks: the attach can
// never report success until this has already happened.
func prepareJIT26(label: String) -> UnsafeMutableRawPointer? {
    let size = jit26RegionSize
    let debugged = jit26_is_debugged()
    JITLog.append("[\(label)] CS_DEBUGGED = \(debugged)")

    guard let region = jit26_prepare_region(nil, size) else {
        JITLog.append("[\(label)] jit26_prepare_region FAILED (returned null)")
        return nil
    }
    JITLog.append("[\(label)] jit26_prepare_region OK at \(region)")

    var kernReturn: Int32 = 0
    var curProt: UInt32 = 0
    var maxProt: UInt32 = 0
    guard let writableAlias = jit26_writable_alias(region, size, &kernReturn, &curProt, &maxProt) else {
        JITLog.append("[\(label)] jit26_writable_alias FAILED kern_return=\(kernReturn)")
        return nil
    }
    JITLog.append("[\(label)] writable alias OK at \(writableAlias), cur_prot=0x\(String(curProt, radix: 16)), max_prot=0x\(String(maxProt, radix: 16))")

    let writeBit: UInt32 = 0x2 // VM_PROT_WRITE
    guard (curProt & writeBit) != 0 else {
        JITLog.append("[\(label)] alias is NOT writable (cur_prot lacks VM_PROT_WRITE) — stopping before the write that would crash")
        return nil
    }

    let word = writableAlias.assumingMemoryBound(to: UInt32.self)
    word.pointee = 0xD65F_03C0 // AArch64 RET
    jit_invalidate_icache(region, size)

    JITLog.append("[\(label)] wrote RET, calling jit26_detach ...")
    jit26_detach()
    JITLog.append("[\(label)] jit26_detach returned")

    return region
}

func executeJIT26(label: String, region: UnsafeMutableRawPointer) {
    JITLog.append("[\(label)] about to execute JIT'd code ...")
    jit_execute(region)
    JITLog.append("[\(label)] EXECUTION SUCCEEDED — JIT works")
}

func attemptJIT26(label: String) {
    guard let region = prepareJIT26(label: label) else { return }
    executeJIT26(label: label, region: region)
}

func selfAttachViaPtrace() -> Bool {
    var attachErrno: Int32 = 0
    let result = jit_self_attach(&attachErrno)

    if result == 0 {
        JITLog.append("ptrace self-attach SUCCEEDED")
        return true
    } else {
        JITLog.append("ptrace self-attach FAILED errno=\(attachErrno) (\(String(cString: strerror(attachErrno))))")
        return false
    }
}
