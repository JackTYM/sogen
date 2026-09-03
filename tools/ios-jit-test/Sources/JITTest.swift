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
