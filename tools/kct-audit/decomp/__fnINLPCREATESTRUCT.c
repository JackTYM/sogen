// __fnINLPCREATESTRUCT — KCT slot 0x0A
// user32.dll VA 0x180027470 (RVA 0x27470, image base 0x180000000)
// Decompiled from ~/.cache/sogen-symbols/user32.i64 via idasql SELECT decompile(0x180027470)
//
// All offsets are DECIMAL as emitted by IDA pseudocode.
//
// Arg-buffer layout decoded (decimal offsets → absolute hex):
//   +0  (0x00) [not directly read]
//   +8  (0x08) cCapturedPointers  — DWORD count for the fixup loop
//   +24 (0x18) offPointers        — DWORD offset of the fixup pointer table within a1
//   +32 (0x20) pvVirtualAddress   — QWORD; if zero, skip relocation
//   +40 (0x28) pwnd               — QWORD; reloc base AND arg1 to wndproc
//   +48 (0x30) msg                — DWORD; wMsg arg2 to wndproc
//   +56 (0x38) wParam             — QWORD; used as lParam pointer in 3-arg wndproc call
//   +64 (0x40) lParam             — QWORD; flag: if non-zero, do kernel-addr fixups
//   +72 (0x48) cs.lpCreateParams  — QWORD* (start of embedded CREATESTRUCT)
//   +128 (0x80) cs.lpszName       — QWORD; fixed up if > gHighestUserAddress
//   +136 (0x88) cs.lpszClass      — QWORD; fixed up if > gHighestUserAddress
//   +160 (0xA0) dispatchFn        — QWORD icall target (dispatch_client_message)
//
// Note: *(_QWORD *)(*(_QWORD *)(a1+40) + N) are through-pointer reads of the HWND object,
//       not direct a1+N reads — they do not widen the arg-struct coverage requirement.

NTSTATUS __fastcall __fnINLPCREATESTRUCT(__int64 a1)
{
  v1 = *(_DWORD *)(a1 + 8);
  v13 = 0;
  v14 = 0LL;
  if ( v1 && !*(_QWORD *)(a1 + 32) )
  {
    v3 = 0;
    v4 = (unsigned int *)(a1 + *(unsigned int *)(a1 + 24));
    do
    {
      v5 = *v4;
      ++v3;
      ++v4;
      *(_QWORD *)(v5 + a1) += a1;
    }
    while ( v3 < *(_DWORD *)(a1 + 8) );
  }
  if ( *(_QWORD *)(a1 + 64) )
  {
    v6 = *(_QWORD *)(a1 + 128);
    v7 = gHighestUserAddress;
    if ( v6 > gHighestUserAddress )
    {
      if ( v6 )
      {
        v11 = v6 + *(_QWORD *)(a1 + 40) - *(_QWORD *)(*(_QWORD *)(a1 + 40) + 8LL);
      }
      else
      {
        v11 = 0LL;
      }
      *(_QWORD *)(a1 + 128) = v11;
    }
    v8 = *(_QWORD *)(a1 + 136);
    if ( v8 > v7 )
    {
      if ( v8 )
      {
        v8 = *(_QWORD *)(a1 + 40) + v8 - *(_QWORD *)(*(_QWORD *)(a1 + 40) + 8LL);
      }
      else
      {
        v8 = 0LL;
      }
      *(_QWORD *)(a1 + 136) = v8;
    }
    v9 = *(unsigned __int64 **)(a1 + 72);
    if ( v9 && (*(_BYTE *)(*(_QWORD *)(a1 + 40) + 24LL) & 0x40) != 0 )
    {
      *v9 = v8;
      *(_QWORD *)(*(_QWORD *)(a1 + 72) + 8LL) = *(_QWORD *)(a1 + 128);
    }
  }
  Result = (*(__int64 (__fastcall **)(_QWORD, _QWORD, _QWORD))(a1 + 160))(
               *(_QWORD *)(a1 + 40),
               *(unsigned int *)(a1 + 48),
               *(_QWORD *)(a1 + 56));
  return NtCallbackReturn(&Result, 0x18u, 0);
}
