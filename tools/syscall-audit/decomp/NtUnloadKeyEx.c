line
__int64 __fastcall NtUnloadKeyEx(__int64 a1, __int64 a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v10 = 0LL;
  v9 = 0LL;
  ((void (__fastcall *)(__int128 *, __int64, __int64, __int64))CmpInitializeThreadInfo)(&v9, a2, a3, a2);
  LOBYTE(v3) = 1;
  v5 = CmUnloadKey(v4, 0LL, v3);
  CmpCleanupThreadInfo(&v9, v6, v5);
  return v7;
}
