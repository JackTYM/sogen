// __fnDWORD — KCT slot 0x02
// user32.dll VA 0x180026110 (RVA 0x26110, image base 0x180000000)
// Decompiled from ~/.cache/sogen-symbols/user32.i64 via idasql SELECT decompile(0x180026110)
//
// All offsets are DECIMAL as emitted by IDA pseudocode.

NTSTATUS __fastcall __fnDWORD(__int64 a1)
{
  v3 = 0;
  v4 = 0LL;
  Result = (*(__int64 (__fastcall **)(_QWORD, _QWORD, _QWORD, _QWORD, _QWORD))(a1 + 40))(
               *(_QWORD *)a1,
               *(unsigned int *)(a1 + 8),
               *(_QWORD *)(a1 + 16),
               *(_QWORD *)(a1 + 24),
               *(_QWORD *)(a1 + 32));
  return NtCallbackReturn(&Result, 0x18u, 0);
}
