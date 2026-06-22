// __fnNCDESTROY — KCT slot 0x03
// user32.dll VA 0x180028CC0 (RVA 0x28CC0, image base 0x180000000)
// Decompiled from ~/.cache/sogen-symbols/user32.i64 via idasql SELECT decompile(0x180028CC0)
//
// All offsets are DECIMAL as emitted by IDA pseudocode.
// Note: *(*(_QWORD *)a1 + 224) reads field 0xE0 from the HWND struct (the window object),
//       not from the arg buffer — it does not widen the arg-struct coverage requirement.

NTSTATUS __fastcall __fnNCDESTROY(__int64 a1)
{
  v6 = 0;
  v7 = 0LL;
  v2 = (*(__int64 (__fastcall **)(_QWORD, _QWORD, _QWORD, _QWORD, _QWORD))(a1 + 40))(
           *(_QWORD *)a1,
           *(unsigned int *)(a1 + 8),
           *(_QWORD *)(a1 + 16),
           *(_QWORD *)(a1 + 24),
           *(_QWORD *)(a1 + 32));
  v3 = *(void **)(*(_QWORD *)a1 + 224LL);
  if ( v3 )
  {
    RtlReleaseActivationContext(v3);
  }
  Result = v2;
  return NtCallbackReturn(&Result, 0x18u, 0);
}
